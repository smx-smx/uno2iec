	!cpu 6502 ; We want to run on a 1541 disc station.
	*= $0500

	!source "assembly/definitions.asm" ; Include standard definitions.

	; Some config values. We're not really that flexible.
	; We expect track and sector number to be specified after the M-E command.

	block_write_data_buffer_start = $0400 	; Data buffer to write from.

	block_read_data_buffer_start = $0600    ; Data buffer to read to.
	block_read_buffer_number = 3            ; Matches memory address above.

	; Entry point for execute buffer. The actual main program starts below.
	jmp read_or_write_block_job

	; Main program (entry point for M-E).
	lda input_buffer + 0x05		; Read from input buffer, offset 5 (after M-E<mem_lo><mem_hi>).
	sta track_for_job_buffer_2 	; We run in buffer 2 (0x500), but read to buffer 1 (0x400).
	lda input_buffer + 0x06
	sta sector_for_job_buffer_2
	lda #jc_execute_buffer
	sta jm_buffer_2
wait_for_completion:
	lda jm_buffer_2
	bmi wait_for_completion
	cmp #jr_error
	bcc ok
	ldx #$00
	jmp print_error
ok:
	rts

read_or_write_block_job:
	; Figure out whether to read or write (offset 7 after M-E<mem_lo><mem_hi><track><sector>).
	lda input_buffer + 0x07
	beq read_sector 	; Zero: Read sector, write sector otherwise.

	; We're writing.

	; We started in the wrong buffer. Change to the buffer whose data should be written.
	lda #<block_write_data_buffer_start
	sta current_buffer_start_low
	lda #>block_write_data_buffer_start
	sta current_buffer_start_high
	
	jsr format_calculate_checksum	
	sta sector_data_checksum

	lda via2_drive_port
	and #via2_drive_port_write_protect_bit
	bne disc_is_writable

	lda #errno_writeprotect
	jmp dc_end_job_loop_with_status

disc_is_writable:
	jsr format_convert_content_to_gcr
	jsr dc_search_block_header

	ldx #$09
skip_header_loop:
	bvc skip_header_loop
	clv
	dex
	bne skip_header_loop

	lda #via2_drive_direction_write
	sta via2_drive_direction

	lda via2_aux_control	; Write to disc.
	and #$1f
	ora #$c0
	sta via2_aux_control

	lda #gcr_sync_byte
	ldx #$05
	sta via2_drive_data
	clv
write_content_sync_loop:
	bvc write_content_sync_loop
	clv
	dex
	bne write_content_sync_loop

	ldy #$bb
write_aux_content_loop:	
	lda processor_stack_page, y
write_aux_content_byte_loop:	
	bvc write_aux_content_byte_loop
	clv
	sta via2_drive_data
	iny
	bne write_aux_content_loop

write_content_loop:	
	lda (current_buffer_start_low),y
write_content_byte_loop:
	bvc write_content_byte_loop
	clv
	sta via2_drive_data
	iny
	bne write_content_loop

wait_last_content_byte_loop:
	bvc wait_last_content_byte_loop

	lda via2_aux_control	; Switch back to read.
	ora #$e0
	sta via2_aux_control

	lda #via2_drive_direction_read
	sta via2_drive_direction

	jsr format_convert_gcr_to_binary

	; TODO(aeckleder): Should we verify what was written here?

	lda #$01
	jmp dc_end_job_loop_with_status

read_sector:
	; We started in the wrong buffer. Change to the buffer we should read to.
	lda #<block_read_data_buffer_start
	sta current_buffer_start_low
	lda #>block_read_data_buffer_start
	sta current_buffer_start_high

	jsr dc_search_block_header_and_sync

	ldy #$00
read_content_loop:
	bvc read_content_loop
	clv
	lda via2_drive_data
	sta (current_buffer_start_low), y
	iny
	bne read_content_loop 	; Read 256 bytes.

	ldy #$ba
read_content_aux_loop:
	bvc read_content_aux_loop
	clv
	lda via2_drive_data
	sta processor_stack_page, y
	iny
	bne read_content_aux_loop ; Read another 70 bytes into aux space.

	jsr read_convert_gcr_to_binary

	lda data_block_signature_byte
	cmp data_block_identifier
	beq calculate_checksum

	lda #errno_readerror_22
	jmp dc_end_job_loop_with_status

calculate_checksum:
	jsr format_calculate_checksum
	cmp sector_data_checksum
	beq read_successful

	; Report checksum error.
	lda #errno_readerror_23
	jmp dc_end_job_loop_with_status

read_successful:
	; Done reading.
	lda #$01
	jmp dc_end_job_loop_with_status
