	!cpu 6502 ; We want to run on a 1541 disc station.
	*= $0500  ; We'll run in buffer 2 of the 1541.

	print_error = $e60a  ; ROM routine to print error (a=errno,x=drive)

	; The following code will perform a drive head 'bump'.
	; TODO(aeckleder): This is just for testing, obviously.
	lda #$12
	sta $06
	lda #$c0
	sta $00   ; Execute in buffer 0
.loop
	lda $00
	bmi .loop
	cmp #$02
	bcc .ok
	lda #$03  ; Report read error
	ldx #$00  ; Drive 0
	jmp print_error
.ok
	rts       ; And we're done.


