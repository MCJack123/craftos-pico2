PROCESSOR 16LF1613

CONFIG FOSC = INTOSC
CONFIG PWRTE = OFF
CONFIG MCLRE = ON
CONFIG CP = OFF
CONFIG BOREN = ON
CONFIG CLKOUTEN = ON
    
CONFIG WRT = OFF
CONFIG ZCD = OFF
CONFIG PLLEN = OFF
CONFIG STVREN = ON
CONFIG BORV = LO
CONFIG LPBOR = OFF
CONFIG LVP = ON
    
CONFIG WDTCPS = WDTCPS2
CONFIG WDTE = OFF
CONFIG WDTCWS = WDTCWS100
CONFIG WDTCCS = LFINTOSC

psect intentry,class=CODE,delta=2
psect powerup,class=CODE,delta=2
psect cinit,class=CODE,delta=2
psect functab,class=CODE,delta=2
psect reset_vec,class=CODE,delta=2
psect init,class=CODE,delta=2
_start:
	;movwf 0x05    ; OSCCAL
	movlb 1
	movlw 0x68
	movwf 0x19
	movlw 0x0E
	movwf 0x0C
	movlw 0xC2    ; ~GPWU, ~GPPU, T0CS=FOSC/4,
	              ;   T0SE=LH, PSA=TMR0, PS=1:256
	movwf 0x15
	movlb 2
	clrf  0x0C    ; GPIO
	movlw 0xB0 	  ; should be 0x80 | (ID << 4) -- 0b1xxx0000
	movwf 0x70    ; RAM0
	rlf   0x70, F

main:
	rlf   0x70, F ; RAM0
	movf  0x70, W ; RAM0
	andlw 1
	movlb 2
	andwf 0x0C, F ; GPIO (6)
	iorwf 0x0C, F ; GPIO
	movlb 0
loop:
	movf  0x15, W ; TMR0 (1)
	btfsc 0x03, 2 ; STATUS.Z
	goto  loop
loop2:
	movf  0x15, W ; TMR0 (1)
	btfss 0x03, 2 ; STATUS.Z
	goto  loop2
	goto  main
psect end_init,class=CODE,delta=2

