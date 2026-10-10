/*
 * File:   main.c
 * Author: jack
 *
 * Created on October 9, 2026, 9:02 PM
 */


#include <xc.h>
#include "config.h"

#pragma config FOSC = INTOSC
#pragma config PWRTE = OFF
#pragma config MCLRE = ON
#pragma config CP = OFF
#pragma config BOREN = ON
#pragma config CLKOUTEN = OFF
    
#pragma config WRT = OFF
#pragma config ZCD = OFF
#pragma config PLLEN = OFF
#pragma config STVREN = ON
#pragma config BORV = LO
#pragma config LPBOR = OFF
#pragma config LVP = ON
    
#pragma config WDTCPS = WDTCPS2
#pragma config WDTE = OFF
#pragma config WDTCWS = WDTCWS100
#pragma config WDTCCS = LFINTOSC

#define PERIPHERAL_ID 1

void main(void) {
    byte cmd;
    
    OSCCONbits.IRCF = 15;
    OPTION_REG = 0xC2;
    ANSELA = 0;
    LATA = 0;
    TRISA = 0x2F;
    
    for (;;) {
        do {
            while (OW);
            if (!OW_reset_pulse()) continue;
            __delay_us(30);
            OW_presence_pulse();
            while (OW);
            ow_error = 0;
            cmd = OW_read_byte();
        } while (ow_error);
        while (OW);
        switch (cmd) {
            case 1: OW_write_byte(PERIPHERAL_ID); break;
            case 2: OW_write_byte(PORTA); break;
        }
    }
}
