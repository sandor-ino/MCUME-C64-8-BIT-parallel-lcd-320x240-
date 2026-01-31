#ifndef Teensy64_h_
#define Teensy64_h_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#define F_CPU 140000000.0
#define F_BUS 140000000.0


#include "settings.h"

#define VERSION "09"
#define NTSC (!PAL)
#define USBHOST (!PS2KEYBOARD)

extern "C" {
  #include "emuapi.h"
}

inline unsigned long millis() {
    return (to_ms_since_boot(get_absolute_time ()));
}

void initMachine();
void resetMachine() __attribute__ ((noreturn));
void resetExternal();
unsigned loadFile(const char *filename);


#if PAL == 1
#define CRYSTAL       	17734475.0f
#define CLOCKSPEED      ( CRYSTAL / 18.0f) // 985248,61 Hz
#define CYCLESPERRASTERLINE 63
#define LINECNT         312 //Rasterlines
#define VBLANK_FIRST    13
#define VBLANK_LAST     15

#else
#define CRYSTAL       	14318180.0f
#define CLOCKSPEED      ( CRYSTAL / 14.0f) // 1022727,14 Hz
#define CYCLESPERRASTERLINE 64
#define LINECNT       	263 //Rasterlines
#define VBLANK_FIRST    13
#define VBLANK_LAST     40
#endif

#define LINEFREQ      			(CLOCKSPEED / CYCLESPERRASTERLINE) //Hz
#define REFRESHRATE       		(LINEFREQ / LINECNT) //Hz
#define LINETIMER_DEFAULT_FREQ (1000000.0f/LINEFREQ)

// Exact timing disabled!!! JMH
//#define MCU_C64_RATIO   ((float)F_CPU / CLOCKSPEED) //MCU Cycles per C64 Cycle
#define US_C64_CYCLE    (1000000.0f / CLOCKSPEED) // Duration (µs) of a C64 Cycle

#define AUDIOSAMPLERATE     (LINEFREQ * 2)// (~32kHz)

#define ISR_PRIORITY_RASTERLINE   255



#if 0
#define WRITE_ATN_CLK_DATA(value) { \
    digitalWriteFast(PIN_SERIAL_ATN, (~value & 0x08));\//PTA13 IEC ATN 3
digitalWriteFast(PIN_SERIAL_CLK, (~value & 0x10)); \ //PTA14 IEC CLK 4
digitalWriteFast(PIN_SERIAL_DATA, (~value & 0x20)); \ //PTA15 IEC DATA 5
}
#define READ_CLK_DATA() \
  ((digitalReadFast(PIN_SERIAL_CLK) << 6) | \
   (digitalReadFast(PIN_SERIAL_DATA) << 7))

#else
#define WRITE_ATN_CLK_DATA(value) {}
#define READ_CLK_DATA() (0)
#endif

#include "output_dac.h"
#include "cpu.h"
#endif
