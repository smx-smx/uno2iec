	!cpu 6502 ; We want to run on a 1541 disc station.
	*= $0500
	
	!source "assembly/definitions.asm" ; Include standard definitions.

	; Entry point at the beginning of buffer 2.
	jmp format_job		

	; This is where command execution starts...
	
	jsr led_on
	
	lda #$41 ; Set ID for formatting. TODO(aeckleder): Don't hardcode.
	sta disc_id_0
	lda #$45
	sta disc_id_1

	jsr close_all_channels

	lda #$01
	sta current_track_number
		
	lda #$02  ; Set track and sector number for buffer 2.
	jsr set_track_and_sector
	
	lda #jc_execute_buffer
	sta jm_buffer_2 ; Ask DC to execute buffer. This will trigger the requested action.
	
wait_format_complete:
	lda jm_buffer_2
	bmi wait_format_complete
	
	cmp #jr_error
	bcc format_ok ; execution result < jr_error.
	
	lda #errno_readerror_21
	ldx #$00
	jmp print_error

format_ok:
	lda disc_format_marker  ; Write format marker.
	sta bam_version_code
	lda #$00
	sta bam_dirty_flag	; Clear BAM dirty flag.
	sta disc_change_status  ; Clear disc change status.
	sta drive_activity_status

	rts

	; The format job is called in the context of the the DC interrupt.
	; When done, it jumps back into the main job loop.
	
format_job:	
	lda format_current_track
	bpl in_progress		; If valid, we're already formatting

	lda #(dc_cr_seeking + dc_cr_idle)
	sta dc_command_register
	
	lda #$01
	sta dc_current_track_number
	sta format_current_track

	lda #$a4		; Move 46 tracks outwards to produce BUMP.
	sta number_half_tracks_to_seek

	lda via2_drive_port
	and #$fc
	sta via2_drive_port	; Step 00 for head movement.

	lda #$0a
	sta max_format_errors

	lda #$a0
	sta half_format_area_size_low
	lda #$0f
	sta half_format_area_size_high

	jmp dc_end_of_job_loop

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
	
	lda #errno_writeprotect
	jmp format_print_error

measure_track_length:
	lda format_current_track
	cmp #$24		; When outside the standard formatting zone,
	bcc normal_range	; make sure we still have the correct sector
	lda #$11		; per track count (17 sectors like zone 4).
	sta current_track_sector_count

normal_range:	
	jsr format_delete_track
	jsr wait_sync_cnt
	lda #gcr_empty_byte
	sta via2_drive_data
	jsr wait_sync_cnt
	
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
	; x = number of bytes per gap, a = remainder
	stx format_num_bytes_per_gap

	cpx #$04
	bcs num_bytes_per_gap_ok
	lda #errno_readerror_23
	jmp format_print_error

num_bytes_per_gap_ok:
	lda #$00
	sta format_sector_counter
	tay
	tax			; x = y = format_sector_counter = 0 

prepare_sector_header_loop:
	lda sector_header_signature_byte
	sta format_sector_header_buffer, y
	iny
	iny			; Skip checksum byte. to be written later.
	lda format_sector_counter
	sta format_sector_header_buffer, y
	iny
	lda format_current_track
	sta format_sector_header_buffer, y
	iny
	lda disc_id_1
	sta format_sector_header_buffer, y
	iny
	lda disc_id_0
	sta format_sector_header_buffer, y
	iny
	lda #$0f
	sta format_sector_header_buffer, y
	iny
	sta format_sector_header_buffer, y
	iny
	
	; Calculate checksum from header content and store it in the
	; corresponding field
	offset_from_header_start = 8 ; y = offset_from_header_start + header_start
	lda #$00
	eor format_sector_header_buffer - offset_from_header_start + 2, y
	eor format_sector_header_buffer - offset_from_header_start + 3, y
	eor format_sector_header_buffer - offset_from_header_start + 4, y
	eor format_sector_header_buffer - offset_from_header_start + 5, y	
	sta format_sector_header_buffer - offset_from_header_start + 1, y
	
	inc format_sector_counter
	lda format_sector_counter
	cmp current_track_sector_count
	bcc prepare_sector_header_loop

	tya	
	pha		; Remember buffer fillstate
	
	lda #>format_sector_header_buffer
	sta current_buffer_start_high
	jsr format_convert_header_to_gcr

	pla		; Pull buffer fillstate from the stack.
	tay
	dey		; Point to last element of the buffer.
	jsr format_move_block_buffer_0  ; TODO(aeckleder): This routine hardcodes the
					; buffer number, so we may want to throw it out.
					; Also: Make sure we actually need the unencoded data! 
	jsr format_move_gcr_to_current_buffer

	lda #>format_sector_content_buffer
	sta current_buffer_start_high

	jsr format_calculate_checksum	
	sta sector_data_checksum

	jsr format_convert_content_to_gcr 	; Includes 0x07 data block header signature.

	lda #$00
	sta current_buffer_track_ptr	; Store offset into sector header.

	jsr format_write_empty_track
	
write_sectors_loop:
	lda #gcr_sync_byte
	sta via2_drive_data
	ldx #$05
write_pre_header_sync_loop:	
	bvc write_pre_header_sync_loop
	clv
	dex
	bne write_pre_header_sync_loop 	; Write 5 x SYNC.

	ldx #$0a
	ldy current_buffer_track_ptr    ; Offset into sector header.
write_header_loop:	
	bvc write_header_loop
	clv
	lda format_sector_header_buffer, y
	sta via2_drive_data
	iny
	dex
	bne write_header_loop		; Write 10 bytes in total.
	
	lda #gcr_empty_byte		; Write 9 empty bytes.
	ldx #$09
write_pre_content_empty_bytes_loop:	
	bvc write_pre_content_empty_bytes_loop
	clv
	sta via2_drive_data
	dex
	bne write_pre_content_empty_bytes_loop 

	lda #gcr_sync_byte
	ldx #$05
write_pre_content_sync_loop:
	bvc write_pre_content_sync_loop
	clv
	sta via2_drive_data
	dex
	bne write_pre_content_sync_loop

	ldx #$bb  ; After GCR encoding, not all data fits into our content buffer.
	          ; The GCR encoder therefore wrote a bunch of data into an auxiliary buffer.
write_aux_buffer_sector_content_loop:
	bvc write_aux_buffer_sector_content_loop
	clv
	lda processor_stack_page, x	; access auxiliary buffer from $1bb to $1ff.
	sta via2_drive_data
	inx
	bne write_aux_buffer_sector_content_loop

	ldy #$00  ; Now write the rest of the sector content.
write_sector_content_loop:	
	bvc write_sector_content_loop
	clv
	lda (current_buffer_start_low), y
	sta via2_drive_data
	iny
	bne write_sector_content_loop
	
	lda #gcr_empty_byte
	ldx format_num_bytes_per_gap
write_post_sector_gap_loop:
	bvc write_post_sector_gap_loop
	clv
	sta via2_drive_data
	dex
	bne write_post_sector_gap_loop
	
	lda current_buffer_track_ptr ; Move to header of next sector.
	clc
	adc #$0a
	sta current_buffer_track_ptr

	dec format_sector_counter
	bne write_sectors_loop

flush_last_gap_byte_loop:
	bvc flush_last_gap_byte_loop
	clv
write_last_track_byte_loop:	; Not exactly sure what this is for, but I assume
				; that this is making the assumption that the number of
				; total gap bytes will never be exactly a multiple of the
				; number of bytes per gap, so it writes an extra one.
	bvc write_last_track_byte_loop
	clv

	jsr dc_set_head_to_read ; Turn off writing, otherwise we just overwrite all we did above.

	; We don't verify anything during format. We'll find out soon enough
	; if anything's broken when copying.

	inc format_current_track
	lda format_current_track
	cmp #$29		; Do we have all 40 tracks yet?
	bcs done_formatting	; TODO(aeckleder): Should we have an option to do 40 vs. 35?

	jmp dc_end_of_job_loop

done_formatting:	
	lda #$01
exit_with_error:
	ldy #$ff
	sty format_current_track
	iny
	sty buffer_gcr_status
	jmp dc_end_job_loop_with_status

	; Decrease max error count and bail out if too many errors occurred.
format_print_error:
	dec max_format_errors
	beq exit_with_error

	jmp dc_end_of_job_loop

	; Wait for half_format_area_size sync signals.
wait_sync_cnt:
	ldx half_format_area_size_low
	ldy half_format_area_size_high
wfs_cnt_loop:
	bvc wfs_cnt_loop
	clv
	dex
	bne wfs_cnt_loop
	dey
	bpl wfs_cnt_loop
	rts

	; Auxiliary variables.

max_format_errors:
	!8 0			; Max number of errors during formatting.
half_format_area_size_low:
	!8 0			; Half the estimated number of sync bytes for this track.
half_format_area_size_high:
	!8 0
format_num_bytes_per_track_low:
	!8 0			; Capacity of a track in bytes.
format_num_bytes_per_track_high:
	!8 0
format_num_bytes_per_gap:
	!8 0			; Number of bytes in each gap between sectors.
format_sector_counter:
	!8 0			; Counts sectors while building the data buffer.
verify_retry_counter:
	!8 0			; Count down number of retries during verify.

