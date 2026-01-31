#include "pico_dsp.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "emuapi.h"
#include "Teensy64.h"
#include "vic.h"

//CONFIGURAZIONE PER DMA + PIO
PIO pio = pio0;
uint sm;
int dma_chan;
dma_channel_config dma_cfg;
typedef uint8_t tpixel;
extern uint8_t linebuffer[2][320];
extern uint16_t scaled_buf[3][480];
extern volatile bool scaled_line_ready[3];
extern volatile int current_buffer;
extern volatile bool buffer_ready_flag; 
extern volatile int current_dma_buffer;
extern volatile int current_render_buffer;
extern uint16_t x_lut[480];

 PICO_DSP::PICO_DSP()  { }

#ifndef  inizializzazione LCD vvvv
/***********************************************************************************************
    inizializzazione LCD
 ***********************************************************************************************/
void PICO_DSP::writeCommand(uint8_t cmd) {
    gpio_put(LCD_DC, 0);
    for (int i = 0; i < 16; i++) gpio_put(LCD_DATA_BASE + i, (cmd >> i) & 1);
    gpio_put(LCD_WR, 0); __asm volatile("nop"); gpio_put(LCD_WR, 1);
}

void PICO_DSP::writeData(uint16_t data) {
    gpio_put(LCD_DC, 1);
    for (int i = 0; i < 16; i++) gpio_put(LCD_DATA_BASE + i, (data >> i) & 1);
    gpio_put(LCD_WR, 0); __asm volatile("nop"); gpio_put(LCD_WR, 1);
}

void PICO_DSP::ili9486_gpio_init() {
    for (int i = 0; i < 16; ++i) {
        gpio_init(LCD_DATA_BASE + i);
        gpio_set_dir(LCD_DATA_BASE + i, GPIO_OUT);
    }
    gpio_init(LCD_WR); gpio_set_dir(LCD_WR, GPIO_OUT); gpio_put(LCD_WR, 1);
    gpio_init(LCD_DC); gpio_set_dir(LCD_DC, GPIO_OUT); gpio_put(LCD_DC, 1);
    gpio_init(LCD_CS); gpio_set_dir(LCD_CS, GPIO_OUT); gpio_put(LCD_CS, 0);
    sleep_ms(120);
    writeCommand(0x11); sleep_ms(120);
    writeCommand(0x3A); writeData(0x55);
    writeCommand(0x36); writeData(0x28);
    writeCommand(0x29); sleep_ms(50);
}

void PICO_DSP::ili9486_pio_init() {
    // Prima configura i pin per PIO
    for (int i = 0; i < 16; i++) {
        gpio_set_function(LCD_DATA_BASE + i, GPIO_FUNC_PIO0);
    }
    gpio_set_function(LCD_WR, GPIO_FUNC_PIO0);
    
    // Poi procedi con il resto dell'inizializzazione PIO
    uint offset = pio_add_program(pio, &ili9486_parallel_program);
    sm = pio_claim_unused_sm(pio, true);
    pio_sm_config c = ili9486_parallel_program_get_default_config(offset);
    sm_config_set_out_pins(&c, LCD_DATA_BASE, 16);
    sm_config_set_sideset_pins(&c, LCD_WR);

    sm_config_set_set_pins(&c, 0, 0);                // nessun pin "set"
    sm_config_set_out_shift(&c, false, true, 16);

    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, 4.7f);
    pio_sm_set_consecutive_pindirs(pio, sm, LCD_DATA_BASE, 16, true);
    pio_sm_set_consecutive_pindirs(pio, sm, LCD_WR, 1, true);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void PICO_DSP::ili9486_dma_init() {
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);   
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma_chan, &cfg,
                        &pio->txf[sm],  // Write address (PIO TX FIFO)
                        NULL,           // Read address (set later)
                        480,            // Transfer count (480 pixels)
                        false);         // Don't start immediately
}

void PICO_DSP::setArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    writeCommand(ILI9486_CASET);
    writeData(x1 >> 8);
    writeData(x1 & 0xFF);
    writeData(x2 >> 8);
    writeData(x2 & 0xFF);
    writeCommand(ILI9486_PASET);
    writeData(y1 >> 8);
    writeData(y1 & 0xFF);
    writeData(y2 >> 8);
    writeData(y2 & 0xFF);
    writeCommand(ILI9486_RAMWR);
}
#endif //  inizializzazione LCD ^^^^

#ifndef  DMA functions vvvv
/***********************************************************************************************
    DMA functions
 ***********************************************************************************************/

    __attribute__((section(".time_critical"))) void PICO_DSP::waitSync() {
    while (dma_channel_is_busy(dma_chan))  tight_loop_contents();//__wfi();
    }

   __attribute__((section(".time_critical"))) void PICO_DSP::writeLine(uint16_t* buf) {
     sio_hw->gpio_set = (1u << LCD_DC);
     sio_hw->gpio_clr = (1u << LCD_CS);
    dma_channel_set_read_addr(dma_chan, buf, false);
    dma_channel_set_trans_count(dma_chan, 480, true);
    }

__attribute__((section(".time_critical"))) 
void PICO_DSP::drawTokenLineRow(
    uint16_t* linebuf, 
    const char* const* tokens, 
    int token_count, 
    int row_index, 
    int selected_index, 
    uint16_t fg, 
    uint16_t bg, 
    uint16_t highlight
) {
    int font_row = row_index / 2;
    const int char_width = 16; // ogni carattere è largo 16px (scaling 2x)
    
    // Calcolo larghezza totale dei token per centrare
    int total_width = 0;
    for (int t = 0; t < token_count; t++) {
        total_width += strlen(tokens[t]) * char_width;
        total_width += 2; // gap minimo
    }
    total_width -= 2; // rimuovi gap finale
    
    // Calcolo offset iniziale per centratura
    int xpos = (480 - total_width) / 2;
    if (xpos < 0) xpos = 0; // sicurezza
    
    // Riempie tutta la riga con bg
    for (int i = 0; i < 480; i++) {
        linebuf[i] = bg;
    }

    // Disegno token
    for (int t = 0; t < token_count; t++) {
        const char* token = tokens[t];
        bool selected = (t == selected_index);
        uint16_t fg_c = selected ? bg : fg;
        uint16_t bg_c = selected ? highlight : bg;

        for (int c = 0; token[c]; c++) {
            const uint8_t* chr = font8x8[(uint8_t)token[c]];
            uint8_t row = chr[font_row];

            for (int bit = 0; bit < 8; bit++) {
                bool pixel_on = row & (1 << bit);
                uint16_t color = pixel_on ? fg_c : bg_c;
                linebuf[xpos + bit * 2]     = color;
                linebuf[xpos + bit * 2 + 1] = color;
            }

            xpos += char_width;
        }
        
        xpos += 2; // gap minimo
        if (xpos >= 480 - char_width) break;
    }
}


#endif //  DMA functions ^^^^

#ifndef   No DMA functions vvvv
/***********************************************************************************************
    No DMA functions
    ***********************************************************************************************/
void PICO_DSP::fillScreenNoDma(dsp_pixel color) {
 
        setArea(0, 0, ILI9486_WIDTH-1, ILI9486_HEIGHT-1);
        for (uint32_t i = 0; i < ILI9486_WIDTH * ILI9486_HEIGHT; ++i) {
            writeData(color);
        }
    
    }

void PICO_DSP::fillRectNoDma(int16_t x, int16_t y, int16_t w, int16_t h, dsp_pixel color) {
    // Controllo dei parametri di input
    if (w <= 0 || h <= 0) return;

    // Ottimizzazione: per rettangoli molto stretti, usa drawRectNoDma
    if (w == 1 || h == 1) {
        drawRectNoDma(x, y, w, h, color);
        return;
    }

    // Imposta l'area del rettangolo da riempire
    setArea(x, y, x + w - 1, y + h - 1);

    // Calcola il numero totale di pixel da disegnare
    uint32_t totalPixels = w * h;

    // Riempie l'area con il colore specificato
    for (uint32_t i = 0; i < totalPixels; i++) {
        writeData(color);
    }
}


    void PICO_DSP::drawRectNoDma(int16_t x, int16_t y, int16_t w, int16_t h, dsp_pixel color) {


        // Ottimizzazione: disegna solo i bordi per un rettangolo vuoto
        if (w <= 0 || h <= 0) return;

        // Bordi superiori e inferiori
        setArea(x, y, x + w - 1, y);
        for (int i = 0; i < w; i++) writeData(color);
        
        setArea(x, y + h - 1, x + w - 1, y + h - 1);
        for (int i = 0; i < w; i++) writeData(color);

        // Bordi laterali
        setArea(x, y, x, y + h - 1);
        for (int i = 0; i < h; i++) writeData(color);
        
        setArea(x + w - 1, y, x + w - 1, y + h - 1);
        for (int i = 0; i < h; i++) writeData(color);
    }

    void PICO_DSP::drawTextNoDma(int16_t x, int16_t y, const char *text, dsp_pixel fgcolor, dsp_pixel bgcolor, bool doublesize) {

        while (*text) {
            char c = *text++;
            if (c < 32 || c > 127) continue;

            const uint8_t *glyph = font8x8[c];
            for (int row = 0; row < 8; row++) {
                uint8_t bits = glyph[row];
                for (int col = 0; col < 8; col++) {
                    dsp_pixel color = (bits & 0x01) ? fgcolor : bgcolor;
                    int px = x + (doublesize ? col*2 : col);
                    int py = y + (doublesize ? row*2 : row);

                    if (px >= 0 && px < ILI9486_WIDTH && py >= 0 && py < ILI9486_HEIGHT) {
                        setArea(px, py, px, py);
                        writeData(color);
                        
                        if (doublesize) {
                            setArea(px+1, py, px+1, py);
                            writeData(color);
                            setArea(px, py+1, px, py+1);
                            writeData(color);
                            setArea(px+1, py+1, px+1, py+1);
                            writeData(color);
                        }
                    }
                    bits >>= 1;
                }
            }
            x += (doublesize ? 16 : 8);
        }
    }
#endif //   No DMA functions ^^^^

#ifndef  SCALING  vvvv
    /***********************************************************************************************
        SCALING orizzontale
    ***********************************************************************************************/

    __attribute__((section(".time_critical"))) void core1_func_tft() {
        while (true) {
            while (multicore_fifo_rvalid()) {
                uint32_t msg = multicore_fifo_pop_blocking();
                uint16_t type = msg >> 16;
                uint16_t val = msg & 0xFFFF;

                if (type == MSG_SCALE) {
                    uint16_t* __restrict dst = scaled_buf[val];
                    uint8_t* __restrict src = linebuffer[current_buffer ^ 1];
                    const uint16_t* __restrict lut = x_lut;
                    const uint16_t* __restrict pal = cpu.vic.palette;
                    
                    // Processa 8 pixel alla volta con prefetch ottimizzato
                    for (int x = 0; x < 480; x += 8) {
                        if ((x % 32) == 0) __builtin_prefetch(&src[lut[x+32]], 0, 0);
                        
                        dst[x]   = pal[src[lut[x]]];
                        dst[x+1] = pal[src[lut[x+1]]];
                        dst[x+2] = pal[src[lut[x+2]]];
                        dst[x+3] = pal[src[lut[x+3]]];
                        dst[x+4] = pal[src[lut[x+4]]];
                        dst[x+5] = pal[src[lut[x+5]]];
                        dst[x+6] = pal[src[lut[x+6]]];
                        dst[x+7] = pal[src[lut[x+7]]];
                    }
                    scaled_line_ready[val] = true;
    }
          
    // if (type == MSG_SCALE) {
    //     uint16_t* __restrict dst = scaled_buf[val];
    //     uint8_t* __restrict src = linebuffer[current_buffer ^ 1];
    //     const uint16_t* __restrict lut = x_lut;
    //     const uint16_t* __restrict pal = cpu.vic.palette;

    //     // Pulizia: bordi neri a sinistra (0–39)
    //     for (int x = 0; x < 40; ++x) {
    //         dst[x] = 0;  // RGB565 nero
    //     }

    //     // Scaling: 400 pixel centrati (40–439)
    //     for (int x = 0; x < 400; x += 8) {
    //         int dst_x = x + 40;
    //         if ((x % 32) == 0) __builtin_prefetch(&src[lut[x+32]], 0, 0);

    //         dst[dst_x]     = pal[src[lut[x]]];
    //         dst[dst_x+1]   = pal[src[lut[x+1]]];
    //         dst[dst_x+2]   = pal[src[lut[x+2]]];
    //         dst[dst_x+3]   = pal[src[lut[x+3]]];
    //         dst[dst_x+4]   = pal[src[lut[x+4]]];
    //         dst[dst_x+5]   = pal[src[lut[x+5]]];
    //         dst[dst_x+6]   = pal[src[lut[x+6]]];
    //         dst[dst_x+7]   = pal[src[lut[x+7]]];
    //     }

    //     // Bordo nero a destra (440–479)
    //     for (int x = 440; x < 480; ++x) {
    //         dst[x] = 0;
    //     }

    //     scaled_line_ready[val] = true;
    // }

        }
      }
    }
#endif //   SCALING  ^^^^

#ifndef  AUDIO functions vvvv
    /***********************************************************************************************
        AUDIO functions
    ***********************************************************************************************/
    bool fillfirsthalf = true;
    static uint16_t cnt = 0;
    #define AUDIO_RING_SIZE 1024
    static int16_t audio_ring[AUDIO_RING_SIZE];
    static volatile uint32_t audio_write_pos = 0;
    static volatile uint32_t audio_read_pos = 0;
    static void (*fillsamples)(int16_t *stream, int len) = nullptr;

extern int g_volume;  // default: massimo

    __attribute__((section(".time_critical"))) void audio_update() {
        if (!fillsamples) return;
        // Buffer troppo pieno? Non aggiungere sample
        if ((audio_write_pos - audio_read_pos) >= (AUDIO_RING_SIZE - 256)) return;
        int16_t tempbuf[256];
        fillsamples(tempbuf, 256);
for (int i = 0; i < 256; ++i) {
    int32_t sample = tempbuf[i];

    // Applica volume (0..9)
    sample = (sample * g_volume) / 9;

    // Clipping di sicurezza
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    audio_ring[audio_write_pos & (AUDIO_RING_SIZE - 1)] = (int16_t)sample;
    audio_write_pos++;
}
    }

    __attribute__((section(".time_critical"))) static void AUDIO_isr() {
        pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));

        if (audio_read_pos != audio_write_pos) {
            int16_t s = audio_ring[audio_read_pos & (AUDIO_RING_SIZE - 1)];
            pwm_set_gpio_level(AUDIO_PIN, (s + 32768) >> 8);
            audio_read_pos++;
        } else {
            pwm_set_gpio_level(AUDIO_PIN, 128);  // valore neutro se buffer vuoto
        }
    }

void PICO_DSP::begin_audio(int samplesize, void (*callback)(int16_t * stream, int len)) {
    fillsamples = callback;
    audio_write_pos = 0;
    audio_read_pos = 0;
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, AUDIO_isr);
    irq_set_priority(PWM_IRQ_WRAP, 128);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 50.0f);
    pwm_config_set_wrap(&config, 254);
    pwm_init(audio_pin_slice, &config, true);
    pwm_set_gpio_level(AUDIO_PIN, 0);
    printf("sound initialized\n");
}

#endif // AUDIO functions ^^^^