#ifndef IOPINS_H
#define IOPINS_H

#include "platform_config.h"


// Speaker
#define AUDIO_PIN       19

// SD (see SPI0 in code!!!)
#define SD_SPIREG       spi1
#define SD_SCLK         26
#define SD_MOSI         28
#define SD_MISO         27 
#define SD_CS           29
#define SD_DETECT       255 // 22


// GPIO definitions for 16-bit parallel interface
#define LCD_DATA_BASE 0  // D0-D15 on GPIO 0-15
#define LCD_WR       16
#define LCD_DC       17
#define LCD_CS       18
//#define LCD_RST      19

// Joystic
#define PIN_JOY2_1     21        // DOWN
#define PIN_JOY2_2     20        // UP
#define PIN_JOY2_3     23        // LEFT
#define PIN_JOY2_4     22       // RIGHT
#define PIN_JOY2_BTN   24

#endif