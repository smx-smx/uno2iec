	!cpu 6502 ; We want to run on a 1541 disc station.
	*= $0630  ; We'll run in buffer 3 and 4 of the 1541. Buffer 3 is partially used
		  ; for temporary storage, so we skip the first 48 bytes.

	!source "assembly/definitions.asm" ; Include standard definitions.
	
	jsr led_on
	
	lda #$41 ; Set ID for formatting. TODO(aeckleder): Don't hardcode.
	sta disc_id_0
	lda #$45
	sta disc_id_1

	jsr close_all_channels

	lda #$01
	sta current_track_number
	
	lda #$4C  ; Place a JMP instruction at the beginning of buffer 3.
        sta $0600
	lda #<format_job
        sta $0601
	lda #>format_job
	sta $0602
	
	lda #$03  ; Set track and sector number for buffer 3.
	jsr set_track_and_sector
	
	lda $7f   ; Current drive number (always 0)
	ora #jc_execute_buffer

	sta jm_buffer_3 ; Ask DC to execute buffer. This will trigger the requested action.
	
wait_format_complete:
	lda jm_buffer_3
	bmi wait_format_complete
	
	cmp #jr_error
	bcc format_ok ; execution result < jr_error.
	
	lda #errno_readerror_21
	ldx #$00
	jmp print_error

format_ok:
	rts

	; The format job is called in the context of the the DC interrupt.
	; When done, it jumps back into the main job loop.
	
format_job:
	lda format_current_track
	bpl in_progress		; If valid, we're already formatting

	jmp format_init_head    ; Position head at track 1

in_progress:
	ldy #$00			  ; Zero offset, needed for indirect
					  ; zero-page addressing.
	cmp (current_buffer_track_ptr), y ; Did we reach a new track?
	beq no_track_change
	sta (current_buffer_track_ptr), y ; Update track for current buffer
	
	jmp dc_end_of_job_loop	          ; Keep moving.
	
no_track_change:
	lda via2_drive_port
	and #via2_drive_port_write_protect_bit
	bne measure_track_length
	
	lda #errno_format_writeprotect
	jmp format_print_error

measure_track_length:
	jsr format_delete_track
	jsr format_wait_sync_cnt ; Wait for number of sync bytes
				 ; specified in half_format_area_size

	lda #gcr_empty_byte
	sta via2_drive_data
	jsr format_wait_sync_cnt
	
	; As we update our estimates, we should converge on having half
	; the track filled with sync markers,
	; and the other half filled with gcr_empty_byte.
	
	jsr dc_set_head_to_read
	jsr dc_wait_for_sync

	lda #via_timer_start_stop_bit
	ora via1_timer_control
	sta via1_timer_control
	lda #$62 		; 98 cycles or ~ 0.1ms
	sta via1_timer_value_low
	lda #$00
	sta via1_timer_value_high
	sta via1_timer_trigger_by_write

	ldy #$00
	ldx #$00
wait_for_sync_start:
	bit via2_drive_port
	bmi wait_for_sync_start
wait_for_sync_end:
	bit via2_drive_port
	bpl wait_for_sync_end

wait_for_new_sync_zone:	
	lda via1_timer_trigger_clear_by_read
wait_for_timer_interrupt_or_new_sync_zone:
	bit via2_drive_port
	bpl new_sync_zone_found

	lda via1_interrupt_status
	asl
	bpl wait_for_timer_interrupt_or_new_sync_zone

	inx
	bne wait_for_new_sync_zone
	iny
	bne wait_for_new_sync_zone

	; It took way too long to find the next sync marker. Disc not rotating?
	lda #errno_readerror_20
	jmp format_print_error

new_sync_zone_found:
	; At this point, (y * 256 + x) * 0.1 ms = duration between two syncs.
	stx format_area_diff_low
	sty format_area_diff_high

	; Now measure the duration of the new sync area.
	ldx #$00 
	ldy #$00

wait_for_end_sync_zone:	
	lda via1_timer_trigger_clear_by_read

wait_for_timer_interrupt_or_end_of_sync_zone:
	bit via2_drive_port
	bmi sync_zone_end_found

	lda via1_interrupt_status
	asl
	bpl wait_for_timer_interrupt_or_end_of_sync_zone
	inx
	bne wait_for_end_sync_zone
	iny
	bne wait_for_end_sync_zone
	
	; It took way too long to find end of sync zone. Disc not rotating?
	lda #errno_readerror_20
	jmp format_print_error

sync_zone_end_found:
	sec
	
	txa  ; Build diff = duration_of_sync - duration_of_non_sync
	sbc format_area_diff_low
	tax
	sta format_area_diff_low
	tya
	sbc format_area_diff_high
	tay
	sta format_area_diff_high

	; This block produces the absolute value x + y * 256 = | x + y * 256 |
	bpl abs_done
	eor #$ff   
	tay
	txa	
	eor #$ff
	tax
	inx	
	bne abs_done
	iny ; Overflow from lower to higher byte.
abs_done:
	; x + y * 256 now holds the absolute value of the duration difference (in 0.1 ms units).

	tya
	bne has_difference_in_high_byte

	cpx #$04
	bcc difference_acceptable ; difference is less than 4 * 0.1 ms.
has_difference_in_high_byte:

	asl format_area_diff_low  ; format_area_diff = format_area_diff * 2
	rol format_area_diff_high ; This may look unintuitive at first, but note that
	clc                       ; format_area_diff has a different unit (0.1 ms units)
				  ; than half_format_area_size (number of sync markers).
	                          ; From this logic it follows that duration(sync) ~ 4 * 0.1 ms.

	lda format_area_diff_low  ; Add 2*difference to half_format_area_size (signed).
	adc half_format_area_size_low
	sta half_format_area_size_low
	lda format_area_diff_high
	adc half_format_area_size_high
	sta half_format_area_size_high
	
	jmp measure_track_length

difference_acceptable:
	; half_format_area_size now contains half the number of sync bytes for this track.
	ldx #$00
	ldy #$00
	clv
count_bytes:	
	lda via2_drive_port 	; Count bytes until the next sync. Pray we haven't missed
	bpl sync_found		; any while checking our duration deviation.
	bvc count_bytes		; The overflow flag is wired to be the BYTE READY signal.
	clv

	; We have a byte. Count it.
	inx
	bne count_bytes
	iny
	bne count_bytes

	lda errno_readerror_21 	; Too many bytes until next sync byte.
	jmp format_print_error
	
sync_found:
	txa 			; num_bytes_per_track = (y * 256 + x) * 2
	asl
	sta format_num_bytes_per_track_low
	tya
	rol
	sta format_num_bytes_per_track_high

	lda #!via_timer_start_stop_bit ; Stop timer.
	and via1_timer_control
	sta via1_timer_control

	ldx current_track_sector_count
	ldy #$00
	tya
sectors_left:
	clc
	adc #$66		; 0x66 = 102
	bcc no_overflow

	iny			; Exceeded 
no_overflow:
	iny
	
	dex			; count down sectors to process.
	bne sectors_left

	; After this counting exercise, we have the space needed for all our sector
	; content plus overhead.
	; a = (current_track_sector_count * 102) % 256
	; y = current_track_sector_count + (current_track_sector_count * 102) / 256

	eor #$ff
	sec
	adc #$00 			   	; a = -a
	clc
	adc format_num_bytes_per_track_low
	bcs carry_set
	dec format_num_bytes_per_track_high 	; We wrapped to positive due to the addition.
						; Decrease high byte as well.
carry_set:
	; a = total_bytes_in_gap % 256
	; Now calculate the high byte of total_bytes_in_gap.

	tax					; x = a
	tya
	eor #$ff
	sec
	adc #$00				; a = -y
	clc
	adc format_num_bytes_per_track_high
	bpl total_bytes_in_gap_positive

	lda #errno_readerror_22	; Not enough capacity to fit our payload.
	jmp format_print_error
	
total_bytes_in_gap_positive:
	; total bytes in gap = a * 256 + x
	
	tay			; y = total_gap_bytes / 256
	txa			; a = total_gap_bytes % 256
	ldx #$00
bytes_per_gap_division_loop:	
	sec
	sbc current_track_sector_count
	bcs bytes_per_gap_division_no_overflow ; Carry flag still set?
	dey
	bmi bytes_per_gap_division_done ; More gap bytes?
bytes_per_gap_division_no_overflow:
	inx
	bne bytes_per_gap_division_loop
bytes_per_gap_division_done:
	stx format_num_bytes_per_gap
	jmp $fc27

	
	
