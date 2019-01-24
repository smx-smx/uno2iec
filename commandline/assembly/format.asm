	!cpu 6502 ; We want to run on a 1541 disc station.
	*= $0400  ; We'll run in buffer 1 of the 1541, which is left untouched while formatting.

	; ROM routines
	
	print_error = $e60a          ; Print error (a=errno,x=drive).
	led_on = $c100               ; Switch LED on.
	set_track_and_sector = $d6d3 ; Set track and sector number.
	close_all_channels = $d307   ; Close all open channels.

	format_init_head = $facb     ; Move head to track 1 (jmp).
	format_delete_track = $fda3  ; Fill the track with SYNC (0xff).
	format_wait_sync_cnt = $fdc3 ; Wait for ($0621/$0622) number of syncs.
	format_print_error = $fdd3   ; Produce formatting error (jmp).

	dc_set_head_to_read = $fe00  ; Switch drive head to reading.
	dc_wait_for_sync = $f556     ; Wait for sync signal.
	dc_end_of_job_loop = $f99c   ; End of job loop, process head moves (jmp).

	; DC Job codes.

	jc_execute_buffer = $e0 ; Job code to execute code in buffer.
	jr_error = $02 ; Job result code for error.

	; Error numbers when calling print_error

	errno_readerror = $03

	; Error numbers when calling format_print_error
	
	errno_format_readerror = $02
	errno_format_writeprotect = $08

	; Zero page memory locations.

	jm_buffer_0 = $0000 ; Job memory for buffer 0.
	jm_buffer_1 = $0001 ; Job memory for buffer 1.
	jm_buffer_2 = $0002 ; Job memory for buffer 2.
	jm_buffer_3 = $0003 ; Job memory for buffer 3.
	jm_buffer_4 = $0004 ; Job memory for buffer 4.
	jm_buffer_5 = $0005 ; Job memory for buffer 5.

	disc_id_0 = $12     ; Storage for disc ID.
	disc_id_1 = $13

	dc_command_register = $20 ; DC command register for drive 0.

	current_buffer_track_ptr = $32 ; Pointer to memory cell holding current buffer's track no.

	format_current_track = $51 ; During formatting, holds current track number.

	current_track_number = $80

	via1_timer_trigger_clear_by_read = $1804 ; Reading from here clears timer interrupt
	via1_timer_trigger_by_write = $1805 ; Writing here (re)starts timer.
	via1_timer_value_low = $1806 ; Low byte of timer latch.
	via1_timer_value_high = $1807 ; Low byte of timer latch.
	via1_timer_control = $180b ; Timer control register of Via 1.
	via1_interrupt_status = $180d ; Interrupt status register.
	
	via2_drive_port = $1c00	   ; Port B of Via 2.
	
	via2_drive_data = $1c01    ; Port A of Via 2: Read or write data byte.

	
	; Via control bits.

	via_timer_start_stop_bit = $40 ; Bit 6 of timer control register.

	via1_interrupt_status_timer = $40 ; Bit 6 is set if timer underflow occurred.
	
	via2_drive_port_write_protect_bit = $10

	; GCR coding.
	gcr_empty_byte = $55
	
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
	
	lda #errno_readerror
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
	ldy #$00			  ; Zero offset, needed for indirect zero-page addressing.
	cmp (current_buffer_track_ptr), y ; Did we reach a new track?
	beq no_track_change
	sta (current_buffer_track_ptr), y ; Update track for current buffer
	
	jmp dc_end_of_job_loop	          ; Keep moving.
	
no_track_change:
	lda via2_drive_port
	and #via2_drive_port_write_protect_bit
	bne write_protect_off
	
	lda #errno_format_writeprotect
	jmp format_print_error

write_protect_off:
	jsr format_delete_track
	jsr format_wait_sync_cnt ; Wait for number of sync bytes specified in ($0621/$0622).

	lda #gcr_empty_byte
	sta via2_drive_data
	jsr format_wait_sync_cnt
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
wait_for_timer_interrupt:
	bit via2_drive_port
	bpl new_sync_zone_found

	lda via1_interrupt_status
	asl
	bpl wait_for_timer_interrupt

	inx
	bne wait_for_new_sync_zone
	iny
	bne wait_for_new_sync_zone

	; It took way too long to find the next sync marker. Disc not rotating?
	lda #errno_format_readerror
	jmp format_print_error

new_sync_zone_found:
	; At this point, (y * 256 + x) * 0.1 ms = duration between two syncs.
	jmp $fb5c
