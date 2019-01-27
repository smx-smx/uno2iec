	; Verify what we just wrote. All sectors have the same content, but different headers.
	; The first sector we read is almost certainly not the first one we wrote, so we allow
	; a generous number of retries.	
	lda #$c8
	sta verify_retry_counter 	; 200 retries.

verify_track_retry_loop:
	lda #<format_sector_header_buffer
	sta current_buffer_start_low
	lda #>format_sector_header_buffer
	sta current_buffer_start_high

	lda current_track_sector_count ; Initialize sector counter for verification.
	sta format_sector_counter

verify_sectors_loop:
	jsr dc_wait_for_sync
	ldx #$0a			; Expecting 10 bytes of block header.
	ldy #$00
	
verify_sector_header_loop:	
	bvc verify_sector_header_loop
	clv
	lda via2_drive_data
	cmp (current_buffer_start_low), y
	bne verification_failed
	iny
	dex
	bne verify_sector_header_loop

	clc
	lda current_buffer_start_low     ; Next loop run will check the next sector header.
	adc #$0a			 ; Once the first one is found, we expect to find 
	sta current_buffer_start_low	 ; them in writing order.
	
	jsr dc_wait_for_sync
	ldy #$bb
verify_aux_sector_content_loop:	
	bvc verify_aux_sector_content_loop
	clv
	lda via2_drive_data
	cmp processor_stack_page, y
	bne verification_failed
	iny
	bne verify_aux_sector_content_loop

	ldx #$fc		; TODO(aeckleder): Unclear why we don't verify the whole sector here.
verify_sector_content_loop:
	bvc verify_sector_content_loop
	clv
	lda via2_drive_data
	cmp format_sector_content_buffer, y
	bne verification_failed
	iny
	dex
	bne verify_sector_content_loop

	dec format_sector_counter
	bne verify_sectors_loop	

	<...> inc format_current_track [...]  jmp dc_end_of_job_loop
	
verification_failed:
	dec verify_retry_counter
	bne verify_track_retry_loop
	lda #errno_readerror_24
	jmp format_print_error
