/*
 *    config.h
 *
 *    created: April 2011
 *    author: Bartek Fabiszewski (www.fabiszewski.net)
 *    version 0.3 sht2x through i2c added
 *    version 0.2 sleep added
 *    version 0.1 first release with support for sht1x
 */

#ifndef CONFIG_H_
#define CONFIG_H_
#include <htc.h>

#define _XTAL_FREQ    16000000 // 16MHz
#define byte unsigned char

#define TRUE                1
#define FALSE               0
#define INPUT               1
#define OUTPUT              0
#define HIGH                1
#define LOW                 0

#define OW                  RA0          //pin 6 for 1-wire data (this can't be changed)
#define TRIS_OW             TRISA0

// some global vars
byte ow_error;      // error on ow line
byte sensor_error;  // error on sensor line
byte scratchpad[6]; // 6-byte buffer [h-msb h-lsb h-crc t-msb t-lsb t-crc]


#include "1wire.h"

#endif /* CONFIG_H_ */
