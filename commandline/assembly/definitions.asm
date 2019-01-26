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

	errno_readerror_20 = $02      ; 20, READ ERROR
	errno_readerror_21 = $03      ; 21, READ ERROR
	errno_readerror_22 = $04      ; 22, READ ERROR

	; Error numbers when calling format_print_error
	
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

	current_track_sector_count = $43 ; Number of sectors on current track.

	format_current_track = $51 ; During formatting, holds current track number.

	format_area_diff_low  = $70 ; Temporary storage location for measuring disc rotation speed.
	format_area_diff_high = $71
	
	current_track_number = $80

	half_format_area_size_low = $0621   ; Half the estimated number of sync bytes for this track 
	half_format_area_size_high = $0622

	format_num_bytes_per_track_low = $0625 	; Capacity of a track in bytes.
	format_num_bytes_per_track_high = $0624 ; TODO(aeckleder): Notice the swapped endianness.
						; It's art, essentially. Fix once we no longer
						; need parts of the original firmware.

	format_num_bytes_per_gap = $0626	; Number of bytes in each gap between sectors.
	
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
	
	via2_drive_port_write_protect_bit = $10 ; Bit 4 controls write protection.

	; GCR coding.
	gcr_empty_byte = $55
