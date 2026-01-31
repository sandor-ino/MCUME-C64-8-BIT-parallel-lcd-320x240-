#include "pico/stdlib.h"
#include <stdio.h>
#include "pico.h"
#include "hardware/clocks.h" 
extern "C" {
  #include "iopins.h"  
  #include "emuapi.h"  
}
#include "c64.h"
#include "pico_dsp.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "pico/multicore.h"
#include "ili9486_parallel.pio.h"
volatile bool vbl=true;

extern volatile bool scaled_line_ready[3];
extern volatile int current_buffer;
extern volatile bool buffer_ready_flag;
extern uint16_t linebuffer[2][320];
extern uint16_t scaled_buf[3][480];
extern void audio_update();
extern void core1_func_tft();
extern int vol_setting;
PICO_DSP display;
int skip=0;

static unsigned short palette16[PALETTE_SIZE];
static uint16_t prev_com = 0;
static int action=0;

extern bool menuActive(void);
extern int handleMenu(uint16_t bClick);
extern char * menuSelection(void);
extern void toggleMenu(bool on);
extern const int scale_pattern[2] = {2, 1};

__attribute__((section(".time_critical_data"))) uint16_t x_lut[480]; 
uint16_t y_lut[300];
uint16_t buff_border[480];


bool repeating_timer_callback(struct repeating_timer *t) {
  if (!menuActive()) {    
    uint16_t bClick = emu_DebounceLocalKeys();
    emu_Input(bClick);  
    if (vbl) {
      vbl = false;
    } else {
      vbl = true;
    }   
    return true;
  }
}

void init_buffers() {
    for (int i = 0; i < 480; i++) {
        // Usa fixed-point per migliorare le performance
        x_lut[i] = (i * 320) / 480;
        
        // Allinea a 16 byte per il prefetch
        if (i % 16 == 0) {
            __builtin_prefetch(&x_lut[i], 0, 3);
        }
    }  
for (int r = 0; r < 300; r++) y_lut[r] = scale_pattern[(r) % 2];
for (int i = 0; i < 480; ++i) buff_border[i] = ( RGBVAL16(0,0,0) );

}

int main() {
  set_sys_clock_khz(250000, true);  
  stdio_init_all();
  init_buffers();
  display.ili9486_gpio_init();
  display.fillScreenNoDma( RGBVAL16(0x00,0x00,0xff) );
  emu_init();     

  char * filename;
#ifdef FILEBROWSER
  while (true) {  
    if (menuActive()) {    
      uint16_t buttonState = emu_ReadKeys();  
      int action = handleMenu(buttonState);
      filename = menuSelection(); 
      if (action == ACTION_RUN) {
      break;
      }   
    }
  }
#endif  
  struct repeating_timer timer;
  add_repeating_timer_ms(25, repeating_timer_callback, NULL, &timer);  
  emu_start();
  multicore_launch_core1(core1_func_tft);
  emu_Init(filename);
  while (true) {
    emu_Step();
  }
}


void emu_SetPaletteEntry(unsigned char r, unsigned char g, unsigned char b, int index) {
    if (index < PALETTE_SIZE) {
        palette16[index] = RGBVAL16(r,g,b);
    }
}


void emu_DrawVsync(void)
 {
  if (vol_setting != 0){
  audio_update(); 
  audio_update();
  }

  skip++;
  volatile bool vb=vbl; 
  }



#ifdef HAS_SND

#include "AudioPlaySystem.h"
AudioPlaySystem mymixer;
void emu_sndInit() {
  display.begin_audio(256, mymixer.snd_Mixer);
  mymixer.start();    
}

void emu_sndPlaySound(int chan, int volume, int freq)
{
  if (chan < 6) {
    mymixer.sound(chan, freq, volume); 
  }
}

void emu_sndPlayBuzz(int size, int val) {
  mymixer.buzz(size,val); 
}

#endif

