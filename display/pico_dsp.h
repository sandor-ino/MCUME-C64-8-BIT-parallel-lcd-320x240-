#ifndef _PICO_DSP_H
#define _PICO_DSP_H
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "font8x8.h"
#include <math.h>
#include "pico.h"
#include "iopins.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "ili9486_parallel.pio.h"

// Definizioni per ILI9486
#define ILI9486_WIDTH      480
#define ILI9486_HEIGHT     320
#define LINE_BUFFER_SIZE ILI9486_WIDTH

// ILI9486 Command Set
#define ILI9486_SWRESET     0x01
#define ILI9486_SLPOUT      0x11
#define ILI9486_INVON       0x21
#define ILI9486_DISPOFF     0x28
#define ILI9486_DISPON      0x29
#define ILI9486_CASET       0x2A
#define ILI9486_PASET       0x2B
#define ILI9486_RAMWR       0x2C
#define ILI9486_MADCTL      0x36
#define ILI9486_PIXFMT      0x3A

// MADCTL bits
#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

typedef uint16_t dsp_pixel;  // RGB565 format
enum MsgType {
    MSG_AUDIO = 0,
    MSG_SCALE = 1
};
void core1_func_tft();
void audio_update();
class PICO_DSP {
public:
  PICO_DSP();
  // Initialization
  void writeCommand(uint8_t cmd);
  void writeData(uint16_t data);
  void ili9486_pio_init();
  void ili9486_dma_init();
  void ili9486_gpio_init();
  
  // functions
  void setArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  // DMA functions
  void waitSync();
  void writeLine(dsp_pixel *buf);
  

void drawTokenLineRow(uint16_t* linebuf, const char* const* tokens, int token_count, int row_index, int selected_index, uint16_t fg, uint16_t bg, uint16_t highlight) ;
  
  // NoDMA functions
  void fillScreenNoDma(dsp_pixel color);
  void drawTextNoDma(int16_t x, int16_t y, const char * text, dsp_pixel fgcolor, dsp_pixel bgcolor, bool doublesize);
  void drawRectNoDma(int16_t x, int16_t y, int16_t w, int16_t h, dsp_pixel color);
void fillRectNoDma(int16_t x, int16_t y, int16_t w, int16_t h, dsp_pixel color) ;
// AUDIO functions
 void begin_audio(int samplesize, void (*callback)(short * stream, int len));



private:

    static const int scale_pattern[2]; // pattern: 3,2
    static int scale_state;
    static uint16_t line_buffer[2][320];


};

#endif