#include "Teensy64.h"
#include "vic.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/multicore.h"
#include "pico_dsp.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b)) 

// Scegli una delle seguenti opzioni decommentando la linea corrispondente:

// OPZIONE 1: Gamma originale (può saturare)
//#define GAMMA_R(v) (((v) * (v)) / 256)
//#define GAMMA_G(u) (((u) * (u)) / 256)
//#define GAMMA_B(z) (((z) * (z)) / 256)

// OPZIONE 2: Gamma ridotta 85% (meno saturazione)
//#define GAMMA_R(v) ((((v) * (v)) / 256) * 85 / 100)
//#define GAMMA_G(u) ((((u) * (u)) / 256) * 85 / 100)
//#define GAMMA_B(z) ((((z) * (z)) / 256) * 85 / 100)

// OPZIONE 3: Gamma ridotta 70% (ancora meno saturazione)
//#define GAMMA_R(v) ((((v) * (v)) / 256) * 70 / 100)
//#define GAMMA_G(u) ((((u) * (u)) / 256) * 70 / 100)
//#define GAMMA_B(z) ((((z) * (z)) / 256) * 70 / 100)

// OPZIONE 4: Gamma più dolce (radice quadrata approssimata)
//#define GAMMA_R(v) (((v) * 181) / 256)
//#define GAMMA_G(u) (((u) * 181) / 256)
//#define GAMMA_B(z) (((z) * 181) / 256)

// OPZIONE 5: Lineare ridotto 90%
//#define GAMMA_R(v) ((v) * 90 / 100)
//#define GAMMA_G(u) ((u) * 90 / 100)
//#define GAMMA_B(z) ((z) * 90 / 100)

// OPZIONE 6: Lineare ridotto 80%
#define GAMMA_R(v) ((v) * 80 / 100)
#define GAMMA_G(u) ((u) * 80 / 100)
#define GAMMA_B(z) ((z) * 80 / 100)

// OPZIONE 7: Lineare puro (senza correzione gamma)
//#define GAMMA_R(v) (v)
//#define GAMMA_G(u) (u)
//#define GAMMA_B(z) (z)

// OPZIONE 8: Gamma con clamp per evitare saturazione
//#define GAMMA_CLAMP(x) ((x) > 255 ? 255 : (x))
//#define GAMMA_R(v) GAMMA_CLAMP(((v) * (v)) / 256)
//#define GAMMA_G(u) GAMMA_CLAMP(((u) * (u)) / 256)
//#define GAMMA_B(z) GAMMA_CLAMP(((z) * (z)) / 256)

#define PALETTE(r, g, b) \ 
  (RGBVAL16(GAMMA_R(r), GAMMA_G(g), GAMMA_B(b)))

const uint16_t palette[16] = {
  0x0000, // Nero
  0x528a, // Bianco
  0x8800, // Rosso
  0x07FF, // Ciano
  0xB81F, // Viola
  0x07E0, // Verde
  0x001F, // Blu
  0x8400, // Giallo
  0x5100, // Arancione 0x4100
  0x28a0, // Marrone
  0x4124, // Rosa
  0x2104, // Grigio scuro
  0x3166, // Grigio medio
  0x07FF, // Verde chiaro
  0x01cd, // Azzurro chiaro
  0x39a7  // Grigio chiaro
};


// #define BORDER      	        (240-200)/2
// #define SCREEN_HEIGHT         (200+2*BORDER)
// #define SCREEN_WIDTH          320
//#define LINE_MEM_WIDTH        320
 #define SCREEN_WIDTH          320
 #define SCREEN_HEIGHT         200
 #define FIRSTDISPLAYLINE      51
 #define LASTDISPLAYLINE       250
 #define BORDER_LEFT           0
 #define BORDER_RIGHT          0

typedef uint8_t tpixel;

#define MAXCYCLESSPRITES0_2       3
#define MAXCYCLESSPRITES3_7       5
#define MAXCYCLESSPRITES    (MAXCYCLESSPRITES0_2 + MAXCYCLESSPRITES3_7)

#include "pico_dsp.h"
extern PICO_DSP display;

extern PIO pio;
extern uint sm;
extern int dma_chan;
extern dma_channel_config dma_cfg;
extern int current_line;

/*****************************************************************************************************/

inline __attribute__((always_inline))
__attribute__((section(".time_critical"))) void fastFillLine(tpixel * p, const tpixel * pe, const uint8_t col, uint16_t * spl);
inline __attribute__((always_inline))
__attribute__((section(".time_critical"))) void fastFillLineNoSprites(tpixel * p, const tpixel * pe, const uint8_t col);

/*****************************************************************************************************/

#define SPRITENUM(data) (1 << ((data >> 8) & 0x07))
#define CHARSETPTR() cpu.vic.charsetPtr = cpu.vic.charsetPtrBase + cpu.vic.rc;
#define CYCLES(x) {if (cpu.vic.badline) {cia_clockt(x);} else {cpu_clock(x);} }

#define BADLINE(x) {if (cpu.vic.badline) { \
      cpu.vic.lineMemChr[x] = cpu.RAM[cpu.vic.videomatrix + vc + x]; \
	  cpu.vic.lineMemCol[x] = cpu.vic.COLORRAM[vc + x]; \
	  cia1_clock(1); \
	  cia2_clock(1); \
    } else { \
      cpu_clock(1); \
    } \
  };

#define SPRITEORFIXEDCOLOR() \
  sprite = *spl++; \
  if (sprite) { \
    *p++ = cpu.vic.palette[sprite & 0x0f]; \
  } else { \
    *p++ = col; \
  }


#if 0
#define PRINTOVERFLOW   \
  if (p>pe) { \
    Serial.print("VIC overflow Mode "); \
    Serial.println(mode); \
  }

#define PRINTOVERFLOWS  \
  if (p>pe) { \
    Serial.print("VIC overflow (Sprite) Mode ");  \
    Serial.println(mode); \
  }
#else
#define PRINTOVERFLOW
#define PRINTOVERFLOWS
#endif

/*****************************************************************************************************/
__attribute__((section(".time_critical"))) void mode0(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    uint8_t chr, pixel;
    uint8_t fgcol;
    uint8_t bgcol = cpu.vic.B0C & 0x0F;
    uint8_t x = 0;

    CHARSETPTR();

    if (cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);
            chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            fgcol = cpu.vic.lineMemCol[x] & 0x0F;
            x++;
            
            unsigned m = min(8, pe - p);
            for (unsigned i = 0; i < m; i++) {
                int sprite = *spl++;
                if (sprite) {
                    int spritenum = SPRITENUM(sprite);
                    int spritepixel = sprite & 0x0f;
                    if (sprite & 0x4000) {
                        if (chr & 0x80) {
                            cpu.vic.fgcollision |= spritenum;
                            pixel = fgcol;
                        } else {
                            pixel = spritepixel;
                        }
                    } else {
                        if (chr & 0x80) cpu.vic.fgcollision |= spritenum;
                        pixel = spritepixel;
                    }
                } else {
                    pixel = (chr & 0x80) ? fgcol : cpu.vic.B0C;
                }
                *p++ = pixel;
                chr = chr << 1;
            }
        } while (p < pe);
    } else {
        // Versione senza sprite
        while (p < pe - 8) {
            BADLINE(x);
            chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            fgcol = cpu.vic.lineMemCol[x] & 0x0F;
            x++;
            
            *p++ = (chr & 0x80) ? fgcol : bgcol;
            *p++ = (chr & 0x40) ? fgcol : bgcol;
            *p++ = (chr & 0x20) ? fgcol : bgcol;
            *p++ = (chr & 0x10) ? fgcol : bgcol;
            *p++ = (chr & 0x08) ? fgcol : bgcol;
            *p++ = (chr & 0x04) ? fgcol : bgcol;
            *p++ = (chr & 0x02) ? fgcol : bgcol;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }
        
        while (p < pe) {
            BADLINE(x);
            chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            fgcol = cpu.vic.lineMemCol[x] & 0x0F;
            x++;
            
            *p++ = (chr & 0x80) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x40) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x20) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x10) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x08) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x04) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x02) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }
    }
    while (x<40) {BADLINE(x); x++;}

};

/*****************************************************************************************************/
__attribute__((section(".time_critical"))) void mode1(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    uint8_t colors[4];
    uint8_t pixel;
    uint8_t chr;
    uint8_t x = 0;
uint8_t bgcol = cpu.vic.B0C & 0x0F;
    CHARSETPTR();

    colors[0] = cpu.vic.B0C & 0x0F;

    if (cpu.vic.lineHasSprites) {
        do {
            if (cpu.vic.idle) {
                cpu_clock(1);
                colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);
                uint8_t c = cpu.vic.lineMemCol[x];
                colors[1] = cpu.vic.R[0x22] & 0x0F;
                colors[2] = cpu.vic.R[0x23] & 0x0F;
                colors[3] = c & 0x07;
                chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            }
            x++;

            if ((cpu.vic.lineMemCol[x-1] & 0x08) == 0) { // HIRES
                unsigned m = min(8, pe - p);
                for (unsigned i = 0; i < m; i++) {
                    int sprite = *spl++;
                    if (sprite) {
                        int spritenum = SPRITENUM(sprite);
                        pixel = sprite & 0x0F;
                        if (sprite & 0x4000) {
                            if (chr & 0x80) {
                                cpu.vic.fgcollision |= spritenum;
                                pixel = colors[3];
                            }
                        } else {
                            if (chr & 0x80) cpu.vic.fgcollision |= spritenum;
                        }
                    } else {
                        pixel = (chr >> 7) ? colors[3] : colors[0];
                    }
                    *p++ = pixel;
                    chr = chr << 1;
                }
            } else { // MULTICOLOR
                for (unsigned i = 0; i < 4; i++) {
                    if (p >= pe) break;
                    int c = (chr >> 6) & 0x03;
                    chr = chr << 2;

                    int sprite = *spl++;
                    if (sprite) {
                        int spritenum = SPRITENUM(sprite);
                        pixel = sprite & 0x0F;
                        if (sprite & 0x4000) {
                            if (c & 0x02) {
                                cpu.vic.fgcollision |= spritenum;
                                pixel = colors[c];
                            }
                        } else {
                            if (c & 0x02) cpu.vic.fgcollision |= spritenum;
                        }
                    } else {
                        pixel = colors[c];
                    }
                    *p++ = pixel;
                    if (p >= pe) break;

                    sprite = *spl++;
                    if (sprite) {
                        int spritenum = SPRITENUM(sprite);
                        pixel = sprite & 0x0F;
                        if (sprite & 0x4000) {
                            if (c & 0x02) {
                                cpu.vic.fgcollision |= spritenum;
                                pixel = colors[c];
                            }
                        } else {
                            if (c & 0x02) cpu.vic.fgcollision |= spritenum;
                        }
                    } else {
                        pixel = colors[c];
                    }
                    *p++ = pixel;
                }
            }
        } while (p < pe);
    } else {
        // Versione senza sprite
        while (p < pe - 8) {
            uint8_t c;
            colors[0] = cpu.vic.B0C & 0x0F;

            if (cpu.vic.idle) {
                cpu_clock(1);
                c = colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);
                colors[1] = cpu.vic.R[0x22] & 0x0F;
                colors[2] = cpu.vic.R[0x23] & 0x0F;
                chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
                c = cpu.vic.lineMemCol[x];
            }
            x++;

            if ((c & 0x08) == 0) { // HIRES
                uint8_t fgcol = c & 0x07;
                *p++ = (chr & 0x80) ? fgcol : colors[0];
                *p++ = (chr & 0x40) ? fgcol : colors[0];
                *p++ = (chr & 0x20) ? fgcol : colors[0];
                *p++ = (chr & 0x10) ? fgcol : colors[0];
                *p++ = (chr & 0x08) ? fgcol : colors[0];
                *p++ = (chr & 0x04) ? fgcol : colors[0];
                *p++ = (chr & 0x02) ? fgcol : colors[0];
                *p++ = (chr & 0x01) ? fgcol : colors[0];
            } else { // MULTICOLOR
                colors[3] = c & 0x07;
                pixel = colors[(chr >> 6) & 0x03]; *p++ = pixel; *p++ = pixel;
                pixel = colors[(chr >> 4) & 0x03]; *p++ = pixel; *p++ = pixel;
                pixel = colors[(chr >> 2) & 0x03]; *p++ = pixel; *p++ = pixel;
                pixel = colors[chr & 0x03];       *p++ = pixel; *p++ = pixel;
            }
        }
        
        // Gestione ultimo carattere parziale
        while (p < pe) {
            uint8_t c;
            colors[0] = cpu.vic.B0C & 0x0F;

            if (cpu.vic.idle) {
                cpu_clock(1);
                c = colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);
                colors[1] = cpu.vic.R[0x22] & 0x0F;
                colors[2] = cpu.vic.R[0x23] & 0x0F;
                chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
                c = cpu.vic.lineMemCol[x];
            }
            x++;

            if ((c & 0x08) == 0) { // HIRES
                uint8_t fgcol = c & 0x07;
                *p++ = (chr & 0x80) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x40) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x20) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x10) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x08) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x04) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x02) ? fgcol : colors[0]; if (p >= pe) break;
                *p++ = (chr & 0x01) ? fgcol : colors[0];
            } else { // MULTICOLOR
                colors[3] = c & 0x07;
                pixel = colors[(chr >> 6) & 0x03]; *p++ = pixel; if (p >= pe) break; *p++ = pixel; if (p >= pe) break;
                pixel = colors[(chr >> 4) & 0x03]; *p++ = pixel; if (p >= pe) break; *p++ = pixel; if (p >= pe) break;
                pixel = colors[(chr >> 2) & 0x03]; *p++ = pixel; if (p >= pe) break; *p++ = pixel; if (p >= pe) break;
                pixel = colors[chr & 0x03];       *p++ = pixel; if (p >= pe) break; *p++ = pixel;
            }
        }
    }
    while (x<40) {BADLINE(x); x++;}
};

/*****************************************************************************************************/
__attribute__((section(".time_critical"))) void mode2(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    uint8_t chr, pixel;
    uint8_t fgcol, bgcol;
    uint8_t x = 0;
    uint8_t *bP = cpu.vic.bitmapPtr + vc * 8 + cpu.vic.rc;

    if (cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);
            uint8_t t = cpu.vic.lineMemChr[x];
            fgcol = t >> 4;
            bgcol = t & 0x0F;
            chr = bP[x * 8];
            x++;

            unsigned m = min(8, pe - p);
            for (unsigned i = 0; i < m; i++) {
                int sprite = *spl++;
                chr = chr << 1;
                if (sprite) {
                    int spritenum = SPRITENUM(sprite);
                    pixel = sprite & 0x0F;
                    if (sprite & 0x4000) {
                        if (chr & 0x80) {
                            cpu.vic.fgcollision |= spritenum;
                            pixel = fgcol;
                        }
                    } else {
                        if (chr & 0x80) cpu.vic.fgcollision |= spritenum;
                    }
                } else {
                    pixel = (chr & 0x80) ? fgcol : bgcol;
                }
                *p++ = pixel;
            }
        } while (p < pe);
    } else {
        // Versione senza sprite
        while (p < pe) {
            if (x >= 40) break;
            BADLINE(x);
            uint8_t t = cpu.vic.lineMemChr[x];
            fgcol = t >> 4;
            bgcol = t & 0x0F;
            chr = bP[x * 8];
            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x40) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x20) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x10) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x08) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x04) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x02) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        };
    }
    while (x<40) {BADLINE(x); x++;}
}
/*****************************************************************************************************/
__attribute__((section(".time_critical"))) void mode3(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    uint8_t *bP = cpu.vic.bitmapPtr + vc * 8 + cpu.vic.rc;
    uint8_t colors[4];
    uint8_t pixel;
    uint8_t chr, x = 0;
uint8_t bgcol = cpu.vic.B0C & 0x0F;
    x = 0;
    colors[0] = cpu.vic.B0C & 0x0F;

    if (cpu.vic.lineHasSprites) {
        do {
            if (cpu.vic.idle) {
                cpu_clock(1);
                colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);
                uint8_t t = cpu.vic.lineMemChr[x];
                colors[1] = t >> 4;
                colors[2] = t & 0x0F;
                colors[3] = cpu.vic.lineMemCol[x] & 0x0F;
                chr = bP[x * 8];
            }
            x++;

            for (unsigned i = 0; i < 4; i++) {
                if (p >= pe) break;
                uint8_t c = (chr >> 6) & 0x03;
                chr = chr << 2;

                int sprite = *spl++;
                if (sprite) {
                    int spritenum = SPRITENUM(sprite);
                    pixel = sprite & 0x0F;
                    if (sprite & 0x4000) {
                        if (c & 0x02) {
                            cpu.vic.fgcollision |= spritenum;
                            pixel = colors[c];
                        }
                    } else {
                        if (c & 0x02) cpu.vic.fgcollision |= spritenum;
                    }
                } else {
                    pixel = colors[c];
                }
                *p++ = pixel;
                if (p >= pe) break;

                sprite = *spl++;
                if (sprite) {
                    int spritenum = SPRITENUM(sprite);
                    pixel = sprite & 0x0F;
                    if (sprite & 0x4000) {
                        if (c & 0x02) {
                            cpu.vic.fgcollision |= spritenum;
                            pixel = colors[c];
                        }
                    } else {
                        if (c & 0x02) cpu.vic.fgcollision |= spritenum;
                    }
                } else {
                    pixel = colors[c];
                }
                *p++ = pixel;
            }
        } while (p < pe);
    } else {
        // Versione senza sprite
        while (p < pe) {
            if (x >= 40) break;
            colors[0] = cpu.vic.B0C & 0x0F;

            if (cpu.vic.idle) {
                cpu_clock(1);
                colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);
                uint8_t t = cpu.vic.lineMemChr[x];
                colors[1] = t >> 4;
                colors[2] = t & 0x0F;
                colors[3] = cpu.vic.lineMemCol[x] & 0x0F;
                chr = bP[x * 8];
            }
            x++;

            if (p + 8 <= pe) {
                pixel = colors[(chr >> 6) & 0x03]; *p++ = pixel; *p++ = pixel;
                pixel = colors[(chr >> 4) & 0x03]; *p++ = pixel; *p++ = pixel;
                pixel = colors[(chr >> 2) & 0x03]; *p++ = pixel; *p++ = pixel;
                pixel = colors[chr & 0x03];       *p++ = pixel; *p++ = pixel;
            } else {
                // Ultimo carattere parziale
                pixel = colors[(chr >> 6) & 0x03]; *p++ = pixel; if (p >= pe) break; *p++ = pixel; if (p >= pe) break;
                pixel = colors[(chr >> 4) & 0x03]; *p++ = pixel; if (p >= pe) break; *p++ = pixel; if (p >= pe) break;
                pixel = colors[(chr >> 2) & 0x03]; *p++ = pixel; if (p >= pe) break; *p++ = pixel; if (p >= pe) break;
                pixel = colors[chr & 0x03];       *p++ = pixel; if (p >= pe) break; *p++ = pixel;
            }
        }
    }
    while (x<40) {BADLINE(x); x++;}
}
/*****************************************************************************************************/
__attribute__((section(".time_critical"))) void mode4(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    uint8_t chr, pixel;
    uint8_t fgcol;
    uint8_t bgcol;
    uint8_t x = 0;

    CHARSETPTR();
    if (cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);
            uint32_t td = cpu.vic.lineMemChr[x];
            bgcol = cpu.vic.R[0x21 + ((td >> 6) & 0x03)] & 0x0F;
            chr = cpu.vic.charsetPtr[(td & 0x3f) * 8];
            fgcol = cpu.vic.lineMemCol[x] & 0x0F;
            x++;

            unsigned m = min(8, pe - p);
            for (unsigned i = 0; i < m; i++) {
                int sprite = *spl++;
                if (sprite) {
                    int spritenum = SPRITENUM(sprite);
                    if (sprite & 0x4000) {
                        if (chr & 0x80) {
                            cpu.vic.fgcollision |= spritenum;
                            pixel = fgcol;
                        } else pixel = bgcol;
                    } else {
                        if (chr & 0x80) cpu.vic.fgcollision |= spritenum;
                        pixel = sprite & 0x0F;
                    }
                } else {
                    pixel = (chr & 0x80) ? fgcol : bgcol;
                }
                chr = chr << 1;
                *p++ = pixel;
            }
        } while (p < pe);
    } else {
        // Versione senza sprite
        while (p < pe) {
            if (x >= 40) break;
            BADLINE(x);
            uint32_t td = cpu.vic.lineMemChr[x];
            bgcol = cpu.vic.R[0x21 + ((td >> 6) & 0x03)] & 0x0F;
            chr = cpu.vic.charsetPtr[(td & 0x3f) * 8];
            fgcol = cpu.vic.lineMemCol[x] & 0x0F;
            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x40) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x20) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x10) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x08) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x04) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x02) ? fgcol : bgcol; if (p >= pe) break;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        };
    }
    while (x<40) {BADLINE(x); x++;}
}

/*****************************************************************************************************/
/* Ungültige Modi ************************************************************************************/
/*****************************************************************************************************/
__attribute__((section(".time_critical"))) void mode5(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    uint8_t x = 0;
    const uint8_t bgcol = 0; // Nero

    if (cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);
            x++;
            unsigned m = min(8, pe - p);
            for (unsigned i = 0; i < m; i++) {
                int sprite = *spl;
                *spl++ = 0;
                *p++ = (sprite) ? (sprite & 0x0F) : bgcol;
            }
        } while (p < pe);
    } else {
        fastFillLineNoSprites(p, pe, bgcol);
    }
    while (x<40) {BADLINE(x); x++;}
}

// Mode6 e Mode7 sono simili a Mode5
__attribute__((section(".time_critical"))) void mode6(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    mode5(p, pe, spl, vc); // Stessa implementazione
}

__attribute__((section(".time_critical"))) void mode7(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    mode5(p, pe, spl, vc); // Stessa implementazione
}
/*****************************************************************************************************/
/*****************************************************************************************************/

typedef void (*modes_t)( tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc ); //Funktionspointer
const modes_t modes[8] = {mode0, mode1, mode2, mode3, mode4, mode5, mode6, mode7};




#include "emuapi.h"
__attribute__((section(".time_critical_data"))) tpixel linebuffer[2][SCREEN_WIDTH]; // Doppio buffer
__attribute__((section(".time_critical_data"))) uint16_t scaled_buf[3][480];
volatile bool scaled_line_ready[3] = {false, false, false};
volatile int current_buffer = 0;
volatile int current_dma_buffer = 0;
volatile int current_render_buffer  = 0;
volatile bool buffer_ready_flag = false;





const int scale_pattern[2] = {2, 1};

extern uint16_t x_lut[480];
extern uint16_t y_lut[300];
extern uint16_t buff_border[480];
extern bool fillfirsthalf;

extern bool keyboard_active;
extern int key_cursor;
extern int keyset_index;



extern const char* keysets0[];
extern const char* keysets1[];
extern const char* keysets2[];
extern const char* keysets3[];
extern const char* keysets4[];
extern const char* keysets5[];
extern const char* keysets6[];
extern const char** keysets[];
extern const int keyset_lengths[];
extern void audio_update();
extern int skip;
extern bool settings_active;
extern int settings_cursor;
extern char joy_str[];
extern char vol_str[];
extern char btn2_str[]; 

__attribute__((section(".time_critical"))) void vic_do(void) {

  uint16_t vc;
  uint16_t xscroll;
  tpixel *pe;
  tpixel *p = linebuffer[current_buffer];
  uint16_t *spl;
  uint8_t mode;

  /* Linecounter ***************************************************************************************/

  if ( cpu.vic.rasterLine >= LINECNT ) {
    
    unsigned long m = fbmicros();
    cpu.vic.neededTime = (m - cpu.vic.timeStart);
    cpu.vic.timeStart = m;
    cpu.vic.lineClock.setIntervalFast((LINETIMER_DEFAULT_FREQ - ((float)cpu.vic.neededTime / (float)LINECNT - LINETIMER_DEFAULT_FREQ )));

    cpu.vic.rasterLine = 0;
    cpu.vic.vcbase = 0;
    cpu.vic.denLatch = 0;


    emu_DrawVsync();  



  } else  {

    cpu.vic.rasterLine++;

}
  int r = cpu.vic.rasterLine;

  if (r == cpu.vic.intRasterLine )//Set Rasterline-Interrupt
    cpu.vic.R[0x19] |= 1 | ((cpu.vic.R[0x1a] & 1) << 7);

    /*
Uno stato di cattiva linea è disponibile in qualsiasi ciclo di clock se il
fianco negativo di Ø0 all'inizio della griglia del ciclo> = $ 30 e griglia <=
$ F7 e i tre bit inferiori dalla griglia con yscroll e in
Qualsiasi ciclo di Raster Line $ 30 che è stato impostato. 

(Predefinito 3)
Yscroll: Poke 53265, Peek (53265) e 248 o 1: Poke 1024.0
Yscroll: Poke 53265, Peek (53265) e 248 o 1

Den: Poke 53265, Peek (53265) e 224 Screen

L'unico uso di yscroll è il confronto con r nella linea bassa

*/
  if (r == 0x30 ) cpu.vic.denLatch |= cpu.vic.DEN;
  /* 3.7.2
2. Nella prima fase del ciclo 14 ogni linea viene caricata con VCBase
(VCBase-> VC) e VMLI eliminati. Se a quel tempo a
C'è anche uno stato di cattiva linea, anche RC è impostato su zero. 
*/
  vc = cpu.vic.vcbase;

  cpu.vic.badline = (cpu.vic.denLatch && (r >= 0x30) && (r <= 0xf7) && ( (r & 0x07) == cpu.vic.YSCROLL));

  if (cpu.vic.badline) {
    cpu.vic.idle = 0;
    cpu.vic.rc = 0;
  }

  /*****************************************************************************************************/
#if 1
  {
    int t = MAXCYCLESSPRITES3_7 - cpu.vic.spriteCycles3_7;
    if (t > 0) cpu_clock(t);
    if (cpu.vic.spriteCycles3_7 > 0) cia_clockt(cpu.vic.spriteCycles3_7);
  }
#endif

   //HBlank:
   cpu_clock(10);

#ifdef ADDITIONALCYCLES
  cpu_clock(ADDITIONALCYCLES);
#endif

  /* Rand oben /unten **********************************************************************************/
  /*
    RSEL  Höhe des Anzeigefensters  Erste Zeile   Letzte Zeile
    0 24 Textzeilen/192 Pixel 55 ($37)  246 ($f6) = 192 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
    1 25 Textzeilen/200 Pixel 51 ($33)  250 ($fa) = 200 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
  */


  if (cpu.vic.borderFlag) {
    int firstLine = (cpu.vic.RSEL) ? 0x33 : 0x33; //0x37
    if ((cpu.vic.DEN) && (r == firstLine)) cpu.vic.borderFlag = false;
  } else {
    int lastLine = (cpu.vic.RSEL) ? 0xfb : 0xfb; //0xf7
    if (r == lastLine) cpu.vic.borderFlag = true;
  }

  if (r < FIRSTDISPLAYLINE || r > LASTDISPLAYLINE ) {
    if (r == 0)
      cpu_clock(CYCLESPERRASTERLINE - 10 - 2 - MAXCYCLESSPRITES - 1); // (minus hblank l + r)
    else
      cpu_clock(CYCLESPERRASTERLINE - 10 - 2 - MAXCYCLESSPRITES  );
    goto noDisplayIncRC;

  }

  //max_x =  (!cpu.vic.CSEL) ? 40:38;
  p = &linebuffer[current_buffer][0];
  //Left Screenborder: Cycle 10
  spl = &cpu.vic.spriteLine[24];
  cpu_clock(6);


if (cpu.vic.borderFlag) {
    cpu_clock(5);
    pe = p + SCREEN_WIDTH;
    fastFillLineNoSprites(p, pe + BORDER_RIGHT, cpu.vic.colors[0] & 0x0F);
    goto noDisplayIncRC;
}


  /* DISPLAY *******************************************************************************************/

  xscroll = cpu.vic.XSCROLL;

  if (xscroll > 0) {
    uint16_t col = cpu.vic.colors[0];

    if (!cpu.vic.CSEL) {
      cpu_clock(1);
      uint16_t sprite;
      for (int i = 0; i < xscroll; i++) {
        SPRITEORFIXEDCOLOR();
      }
    } else {
      spl += xscroll;
      for (unsigned i = 0; i < xscroll; i++) {
        *p++ = col;
      }
    }
  }

  // pe viene calcolato per scrivere esattamente (320 - xscroll) pixel nelle mode functions
  pe = p + (SCREEN_WIDTH - xscroll);

  /*****************************************************************************************************/

  cpu.vic.fgcollision = 0;
  mode = (cpu.vic.ECM << 2) | (cpu.vic.BMM << 1) | cpu.vic.MCM;

  if ( !cpu.vic.idle)  {

#if 0
    static uint8_t omode = 99;
    if (mode != omode) {
      Serial.print("Graphicsmode:");
      Serial.println(mode);
      omode = mode;
    }
#endif

    modes[mode](p, pe, spl, vc);
    vc = (vc + 40) & 0x3ff;

  } else {

	//Modes 1 & 3
    if (mode == 1 || mode == 3) {
		modes[mode](p, pe, spl, vc);
    } else {//TODO: all other modes
	fastFillLine(p, pe, cpu.vic.palette[0], spl);
	}
  }

  if (cpu.vic.fgcollision) {
    if (cpu.vic.MD == 0) {
      cpu.vic.R[0x19] |= 2 | ( (cpu.vic.R[0x1a] & 2) << 6);
    }
    cpu.vic.MD |= cpu.vic.fgcollision;
  }

  /*****************************************************************************************************/

  if (!cpu.vic.CSEL) {
    cpu_clock(1);
    uint16_t col = cpu.vic.colors[0];
    p = &linebuffer[current_buffer][0]; // display.getLineBuffer((r - ));
#if 0
    // Sprites im Rand
    uint16_t sprite;
    uint16_t * spl;
    spl = &cpu.vic.spriteLine[24 + xscroll];

    SPRITEORFIXEDCOLOR()
    SPRITEORFIXEDCOLOR()
    SPRITEORFIXEDCOLOR()
    SPRITEORFIXEDCOLOR()
    SPRITEORFIXEDCOLOR()
    SPRITEORFIXEDCOLOR()
    SPRITEORFIXEDCOLOR() //7
#else
    //keine Sprites im Rand
    *p++ = col; *p++ = col; *p++ = col; *p++ = col;
    *p++ = col; *p++ = col; *p = col;

#endif

    //Rand rechts:
    p = &linebuffer[current_buffer][SCREEN_WIDTH - 9 + BORDER_LEFT]; //display.getLineBuffer((r - FIRSTDISPLAYLINE)) + SCREEN_WIDTH - 9 + BORDER_LEFT;
    pe = p + 9;

#if 0
    // Sprites im Rand
    spl = &cpu.vic.spriteLine[24 + SCREEN_WIDTH - 9 + xscroll];
    while (p < pe) {
      SPRITEORFIXEDCOLOR();
    }
#else
    //keine Sprites im Rand
    //while (p < pe) {
    //  *p++ = col;
    //}
#endif

}
    if (skip & 1) {
        // Frame skip attivo → salta scaling e writeLine per questa riga
    } else {
if ((r >= FIRSTDISPLAYLINE && r <= LASTDISPLAYLINE)) {


        int pattern = y_lut[r];
        uint line_idx = current_render_buffer;  
        current_buffer ^= 1;                  
        scaled_line_ready[line_idx] = false;
        uint32_t msg = (MSG_SCALE << 16) | line_idx;
        multicore_fifo_push_blocking(msg);
        while (!scaled_line_ready[line_idx]) {
            tight_loop_contents();
        }
        for (int i = 0; i < pattern; i++) {
            display.waitSync();
            display.writeLine(scaled_buf[line_idx]);
        }
        current_render_buffer = (current_render_buffer + 1) % 3;
    }

if ((r == LASTDISPLAYLINE)) {
  for (int i = 0; i < 20; i++) {
      display.waitSync();
      // Tastiera virtuale
      if (keyboard_active && i >= 2 && i < 18) {
          int row = i - 2;
          uint16_t linebuf[480];
          const char** tokens = keysets[keyset_index];
          int token_count = keyset_lengths[keyset_index];

          display.drawTokenLineRow(linebuf, tokens, token_count, row, key_cursor,
                                   0xFFFF,    // fg bianco
                                   0x0000,    // bg nero
                                   0xFFE0);   // highlight giallo
          display.writeLine(linebuf);
      }
      // Menu settings indipendente
      else if (settings_active && i >= 2 && i < 18) {
          int row = i - 2;
          uint16_t linebuf[480];
          const char* settings_tokens[3] = { joy_str, vol_str, btn2_str };

          display.drawTokenLineRow(linebuf, settings_tokens, 3, row, settings_cursor,
                                   0xFFFF,    // fg bianco
                                   0x0000,    // bg nero
                                   0xF800);   // highlight rosso
          display.writeLine(linebuf);
      }
      else {
          display.writeLine(buff_border);
      }
  }
}
}

//Rechter Rand nach CSEL, im Textbereich
cpu_clock(5);

noDisplayIncRC:

  if (cpu.vic.rc == 7) {
    cpu.vic.idle = 1;
    cpu.vic.vcbase = vc;
  }
  //Ist dies richtig ??
  if ((!cpu.vic.idle) || (cpu.vic.denLatch && (r >= 0x30) && (r <= 0xf7) && ( (r & 0x07) == cpu.vic.YSCROLL))) {
    cpu.vic.rc = (cpu.vic.rc + 1) & 0x07;
  }

  /*****************************************************************************************************/
  /* Sprites *******************************************************************************************/
  /*****************************************************************************************************/

  cpu.vic.spriteCycles0_2 = 0;
  cpu.vic.spriteCycles3_7 = 0;

  if (cpu.vic.lineHasSprites) {
	cpu.vic.lineHasSprites = 0;
    memset(cpu.vic.spriteLine, 0, sizeof(cpu.vic.spriteLine) );
  }

  uint32_t spriteYCheck = cpu.vic.R[0x15]; //Sprite enabled Register

  if (spriteYCheck) {

    unsigned short R17 = cpu.vic.R[0x17]; //Sprite-y-expansion
    unsigned char collision = 0;
    short lastSpriteNum = 0;

    for (unsigned short i = 0; i < 8; i++) {
      if (!spriteYCheck) break;

      unsigned b = 1 << i;

      if (spriteYCheck & b )  {
        spriteYCheck &= ~b;
        short y = cpu.vic.R[i * 2 + 1];

        if ( (r >= y ) && //y-Position > Sprite-y ?
             (((r < y + 21) && (~R17 & b )) || // ohne y-expansion
              ((r < y + 2 * 21 ) && (R17 & b ))) ) //mit y-expansion
        {

          //Sprite Cycles
          if (i < 3) {
            if (!lastSpriteNum) cpu.vic.spriteCycles0_2 += 1;
            cpu.vic.spriteCycles0_2 += 2;
          } else {
            if (!lastSpriteNum) cpu.vic.spriteCycles3_7 += 1;
            cpu.vic.spriteCycles3_7 += 2;
          }
          lastSpriteNum = i;
          //Sprite Cycles END


          if (r < FIRSTDISPLAYLINE || r > LASTDISPLAYLINE ) continue;

          uint16_t x =  (((cpu.vic.R[0x10] >> i) & 1) << 8) | cpu.vic.R[i * 2];
          if (x >= SPRITE_MAX_X) continue;

          unsigned short lineOfSprite = r - y;
          if (R17 & b) lineOfSprite = lineOfSprite / 2; // Y-Expansion
          unsigned short spriteadr = cpu.vic.bank | cpu.RAM[cpu.vic.videomatrix + (1024 - 8) + i] << 6 | (lineOfSprite * 3);
          unsigned spriteData = ((unsigned)cpu.RAM[ spriteadr ] << 16) | ((unsigned)cpu.RAM[ spriteadr + 1 ] << 8) | ((unsigned)cpu.RAM[ spriteadr + 2 ]);

          if (!spriteData) continue;
          cpu.vic.lineHasSprites = 1;

          uint16_t * slp = &cpu.vic.spriteLine[x]; //Sprite-Line-Pointer
          unsigned short upperByte = ( 0x80 | ( (cpu.vic.MDP & b) ? 0x40 : 0 ) | i ) << 8; //Bit7 = Sprite "da", Bit 6 = Sprite-Priorität vor Grafik/Text, Bits 3..0 = Spritenummer

          //Sprite in Spritezeile schreiben:
          if ((cpu.vic.MMC & b) == 0) { // NO MULTICOLOR

            uint16_t color = upperByte | cpu.vic.R[0x27 + i];

            if ((cpu.vic.MXE & b) == 0) { // NO MULTICOLOR, NO SPRITE-EXPANSION

              for (unsigned cnt = 0; (spriteData > 0) && (cnt < 24); cnt++) {
                int c = (spriteData >> 23) & 0x01;
                spriteData = (spriteData << 1);

                if (c) {
                  if (*slp == 0) *slp = color;
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                }
                slp++;

              }

            } else {    // NO MULTICOLOR, SPRITE-EXPANSION

              for (unsigned cnt = 0; (spriteData > 0) && (cnt < 24); cnt++) {
                int c = (spriteData >> 23) & 0x01;
                spriteData = (spriteData << 1);
                //So wie oben, aber zwei gleiche Pixel

                if (c) {
                  if (*slp == 0) *slp = color;
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                  if (*slp == 0) *slp = color;
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                } else {
                  slp += 2;
                }

              }
            }



          } else { // MULTICOLOR
            /* Im Mehrfarbenmodus (Multicolor-Modus) bekommen alle Sprites zwei zusätzliche gemeinsame Farben.
              Die horizontale Auflösung wird von 24 auf 12 halbiert, da bei der Sprite-Definition jeweils zwei Bits zusammengefasst werden.
            */
            uint16_t colors[4];
            //colors[0] = 1; //dummy, color 0 is transparent
            colors[1] = upperByte | cpu.vic.R[0x25];
            colors[2] = upperByte | cpu.vic.R[0x27 + i];
            colors[3] = upperByte | cpu.vic.R[0x26];

            if ((cpu.vic.MXE & b) == 0) { // MULTICOLOR, NO SPRITE-EXPANSION
              for (unsigned cnt = 0; (spriteData > 0) && (cnt < 24); cnt++) {
                int c = (spriteData >> 22) & 0x03;
                spriteData = (spriteData << 2);

                if (c) {
                  if (*slp == 0) *slp = colors[c];
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                  if (*slp == 0) *slp = colors[c];
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                } else {
                  slp += 2;
                }

              }

            } else {    // MULTICOLOR, SPRITE-EXPANSION
              for (unsigned cnt = 0; (spriteData > 0) && (cnt < 24); cnt++) {
                int c = (spriteData >> 22) & 0x03;
                spriteData = (spriteData << 2);

                //So wie oben, aber vier gleiche Pixel
                if (c) {
                  if (*slp == 0) *slp = colors[c];
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                  if (*slp == 0) *slp = colors[c];
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                  if (*slp == 0) *slp = colors[c];
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                  if (*slp == 0) *slp = colors[c];
                  else collision |= b | (1 << ((*slp >> 8) & 0x07));
                  slp++;
                } else {
                  slp += 4;
                }

              }

            }
          }

        }
        else lastSpriteNum = 0;
      }

    }

    if (collision) {
      if (cpu.vic.MM == 0) {
        cpu.vic.R[0x19] |= 4 | ((cpu.vic.R[0x1a] & 4) << 5 );
      }
      cpu.vic.MM |= collision;
    }

  }
  /*****************************************************************************************************/
#if 0
  {
    int t = MAXCYCLESSPRITES0_2 - cpu.vic.spriteCycles0_2;
    if (t > 0) cpu_clock(t);
    if (cpu.vic.spriteCycles0_2 > 0) cia_clockt(cpu.vic.spriteCycles0_2);
  }
#endif

   //HBlank:
#if PAL
   cpu_clock(2);
#else
   cpu_clock(3);
#endif


#if 0
   if (cpu.vic.idle) {
	   Serial.print("Cycles line ");
	   Serial.print(r);
	   Serial.print(": ");
	   Serial.println(cpu.lineCyclesAbs);
   }
#endif


  return;
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
// In vic.cpp
// In vic.cpp
__attribute__((section(".time_critical"))) void fastFillLineNoSprites(tpixel * p, const tpixel * pe, const uint8_t col) {
    int i = 0;
    while (p < pe) {
        *p++ = col;
        i = (i + 1) & 0x07;
        if (!i) CYCLES(1);
    }
}

__attribute__((section(".time_critical"))) void fastFillLine(tpixel * p, const tpixel * pe, const uint8_t col, uint16_t * spl) {
    if (spl != NULL && cpu.vic.lineHasSprites) {
        int i = 0;
        uint16_t sprite;
        while (p < pe) {
            sprite = *spl++;
            *p++ = (sprite) ? (sprite & 0x0f) : col;
            i = (i + 1) & 0x07;
            if (!i) CYCLES(1);
        };
    } else {
        fastFillLineNoSprites(p, pe, col);
    }
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

void vic_displaySimpleModeScreen(void) {
}


void vic_do_simple(void) {
  uint16_t vc;
  int cycles = 0;

if ( cpu.vic.rasterLine >= LINECNT ) {

    //reSID sound needs much time - too much to keep everything in sync and with stable refreshrate
    //but it is not called very often, so most of the time, we have more time than needed.
    //We can measure the time needed for a frame and calc a correction factor to speed things up.
    unsigned long m = fbmicros();
    cpu.vic.neededTime = (m - cpu.vic.timeStart);
    cpu.vic.timeStart = m;
    cpu.vic.lineClock.setIntervalFast(LINETIMER_DEFAULT_FREQ - ((float)cpu.vic.neededTime / (float)LINECNT - LINETIMER_DEFAULT_FREQ ));

    cpu.vic.rasterLine = 0;
    cpu.vic.vcbase = 0;
    cpu.vic.denLatch = 0;

  } else  {
	  cpu.vic.rasterLine++;
	  cpu_clock(1);
	  cycles += 1;
  }

  int r = cpu.vic.rasterLine;

  if (r == cpu.vic.intRasterLine )//Set Rasterline-Interrupt
    cpu.vic.R[0x19] |= 1 | ((cpu.vic.R[0x1a] & 1) << 7);

  cpu_clock(9);
  cycles += 9;

  if (r == 0x30 ) cpu.vic.denLatch |= cpu.vic.DEN;

  vc = cpu.vic.vcbase;

  cpu.vic.badline = (cpu.vic.denLatch && (r >= 0x30) && (r <= 0xf7) && ( (r & 0x07) == cpu.vic.YSCROLL));

  if (cpu.vic.badline) {
    cpu.vic.idle = 0;
    cpu.vic.rc = 0;
  }


  /* Rand oben /unten **********************************************************************************/
  /*
    RSEL  Höhe des Anzeigefensters  Erste Zeile   Letzte Zeile
    0 24 Textzeilen/192 Pixel 55 ($37)  246 ($f6) = 192 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
    1 25 Textzeilen/200 Pixel 51 ($33)  250 ($fa) = 200 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
  */

  if (cpu.vic.borderFlag) {
    int firstLine = (cpu.vic.RSEL) ? 0x33 : 0x37;
    if ((cpu.vic.DEN) && (r == firstLine)) cpu.vic.borderFlag = false;
  } else {
    int lastLine = (cpu.vic.RSEL) ? 0xfb : 0xf7;
    if (r == lastLine) cpu.vic.borderFlag = true;
  }


 //left screenborder
 cpu_clock(6);
 cycles += 6;

 CYCLES(40);
 cycles += 40;
 vc += 40;

 //right screenborder
 cpu_clock(4); //1
 cycles += 4;


  if (cpu.vic.rc == 7) {
    cpu.vic.idle = 1;
    cpu.vic.vcbase = vc;
  }
  //Ist dies richtig ??
  if ((!cpu.vic.idle) || (cpu.vic.denLatch && (r >= 0x30) && (r <= 0xf7) && ( (r & 0x07) == cpu.vic.YSCROLL))) {
    cpu.vic.rc = (cpu.vic.rc + 1) & 0x07;
  }

  cpu_clock(3); //1
 cycles += 3;

 int cyclesleft = CYCLESPERRASTERLINE - cycles;
 if (cyclesleft) cpu_clock(cyclesleft);

}


/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

void installPalette(void) {
 memcpy(cpu.vic.palette, (void*)palette, sizeof(cpu.vic.palette));
}


/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

void vic_adrchange(void) {
  uint8_t r18 = cpu.vic.R[0x18];
  cpu.vic.videomatrix =  cpu.vic.bank + (unsigned)(r18 & 0xf0) * 64;

  unsigned charsetAddr = r18 & 0x0e;
  if  ((cpu.vic.bank & 0x4000) == 0) {
    if (charsetAddr == 0x04) cpu.vic.charsetPtrBase =  ((uint8_t *)&rom_characters);
    else if (charsetAddr == 0x06) cpu.vic.charsetPtrBase =  ((uint8_t *)&rom_characters) + 0x800;
    else
      cpu.vic.charsetPtrBase = &cpu.RAM[charsetAddr * 0x400 + cpu.vic.bank] ;
  } else
    cpu.vic.charsetPtrBase = &cpu.RAM[charsetAddr * 0x400 + cpu.vic.bank];

  cpu.vic.bitmapPtr = (uint8_t*) &cpu.RAM[cpu.vic.bank | ((r18 & 0x08) * 0x400)];
  if ((cpu.vic.R[0x11] & 0x60) == 0x60)  cpu.vic.bitmapPtr = (uint8_t*)((uintptr_t)cpu.vic.bitmapPtr & 0xf9ff);

}
/*****************************************************************************************************/
void vic_write(uint32_t address, uint8_t value) {

  address &= 0x3F;

  switch (address) {
    case 0x11 :
    cpu.vic.R[address] = value;
      cpu.vic.intRasterLine = (cpu.vic.intRasterLine & 0xff) | ((((uint16_t) value) << 1) & 0x100);
      if (cpu.vic.rasterLine == 0x30 ) cpu.vic.denLatch |= value & 0x10;

      cpu.vic.badline = (cpu.vic.denLatch && (cpu.vic.rasterLine >= 0x30) && (cpu.vic.rasterLine <= 0xf7) && ( (cpu.vic.rasterLine & 0x07) == (value & 0x07)));

    if (cpu.vic.badline) {
    cpu.vic.idle = 0;
    }

    vic_adrchange();

      break;
    case 0x12 :
      cpu.vic.intRasterLine = (cpu.vic.intRasterLine & 0x100) | value;
      cpu.vic.R[address] = value;
      break;
    case 0x18 :
      cpu.vic.R[address] = value;
      vic_adrchange();
      break;
    case 0x19 : //IRQs
      cpu.vic.R[0x19] &= (~value & 0x0f);
      break;
    case 0x1A : //IRQ Mask
      cpu.vic.R[address] = value & 0x0f;
      break;
    case 0x1e:
    case 0x1f:
      cpu.vic.R[address] = 0;
      break;
    case 0x20 ... 0x2E:
      cpu.vic.R[address] = value & 0x0f;
      cpu.vic.colors[address - 0x20] = cpu.vic.palette[value & 0x0f];
      break;
    case 0x2F ... 0x3F:
      break;
    default :
      cpu.vic.R[address] = value;
      break;
  }

  //#if DEBUGVIC
#if 0
  Serial.print("VIC ");
  Serial.print(address, HEX);
  Serial.print("=");
  Serial.println(value, HEX);
  //logAddr(address, value, 1);
#endif
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

uint8_t vic_read(uint32_t address) {
  uint8_t ret;

  address &= 0x3F;
  switch (address) {

    case 0x11:
      ret = (cpu.vic.R[address] & 0x7F) | ((cpu.vic.rasterLine & 0x100) >> 1);
      break;
    case 0x12:
      ret = cpu.vic.rasterLine;
      break;
    case 0x16:
      ret = cpu.vic.R[address] | 0xC0;
      break;
    case 0x18:
      ret = cpu.vic.R[address] | 0x01;
      break;
    case 0x19:
      ret = cpu.vic.R[address] | 0x70;
      break;
    case 0x1a:
      ret = cpu.vic.R[address] | 0xF0;
      break;
    case 0x1e:
    case 0x1f:
      ret = cpu.vic.R[address];
      cpu.vic.R[address] = 0;
      break;
    case 0x20 ... 0x2E:
      ret = cpu.vic.R[address] | 0xF0;
      break;
    case 0x2F ... 0x3F:
      ret = 0xFF;
      break;
    default:
      ret = cpu.vic.R[address];
      break;
  }

#if DEBUGVIC
  Serial.print("VIC ");
  logAddr(address, ret, 0);
#endif
  return ret;
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

void resetVic(void) {
  enableCycleCounter();

  cpu.vic.intRasterLine = 0;
  cpu.vic.rasterLine = 0;
  cpu.vic.lineHasSprites = 0;
  memset(&cpu.RAM[0x400], 0, 1000);
  memset(&cpu.vic, 0, sizeof(cpu.vic));
  


  installPalette();  

  //http://dustlayer.com/vic-ii/2013/4/22/when-visibility-matters
  cpu.vic.R[0x11] = 0x9B;
  cpu.vic.R[0x16] = 0x08;
  cpu.vic.R[0x18] = 0x14;
  cpu.vic.R[0x19] = 0x0f;

  for (unsigned i = 0; i < sizeof(cpu.vic.COLORRAM); i++)
    cpu.vic.COLORRAM[i] = (rand() & 0x0F);

  cpu.RAM[0x39FF] = 0x0;
  cpu.RAM[0x3FFF] = 0x0;
  cpu.RAM[0x39FF + 16384] = 0x0;
  cpu.RAM[0x3FFF + 16384] = 0x0;
  cpu.RAM[0x39FF + 32768] = 0x0;
  cpu.RAM[0x3FFF + 32768] = 0x0;
  cpu.RAM[0x39FF + 49152] = 0x0;
  cpu.RAM[0x3FFF + 49152] = 0x0;

  vic_adrchange();
}


/*
  ?PEEK(678) NTSC =0
  ?PEEK(678) PAL = 1
  PRINT TIME$
*/
/*
          Raster-  Takt-   sichtb.  sichtbare
  VIC-II  System  zeilen   zyklen  Zeilen   Pixel/Zeile
  -------------------------------------------------------
  6569    PAL    312     63    284     403
  6567R8  NTSC   263     65    235     418
  6567R56A  NTSC   262   64    234     411
*/
