#include "pico.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <cctype>
extern "C" {
#include "emuapi.h"
#include "platform_config.h"
}

#include "pico/multicore.h"
#include "Teensy64.h"
#include <string.h>

#ifdef HAS_SND
#include "reSID.h"
AudioPlaySID playSID;
#endif

using namespace std;

#define C64_KEY_SPACE      0x20
#define C64_KEY_RUNSTOP    0x03 
#define C64_KEY_RETURN     0x0D
#define C64_KEY_DEL        0x7F 
#define C64_KEY_HOME       0x13 


#define C64_KEY_UP        0x91
#define C64_KEY_DOWN      0x11
#define C64_KEY_LEFT      0x9D
#define C64_KEY_RIGHT     0x1D

#define C64_KEY_F1         0x85
#define C64_KEY_F3         0x87
#define C64_KEY_F5         0x89
#define C64_KEY_F7         0x8B

#define C64_KEY_F2         0x86 //
#define C64_KEY_F4         0x88 //
#define C64_KEY_F6         0x8A //
#define C64_KEY_F8         0x8C //


static uint8_t nbkeys = 0;
static uint8_t kcnt = 0;
static bool toggle = true;

static char * textseq;
static char * textload = "LOAD\"\"\r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tRUN\r";
static char textkey[1];

static bool res=false;
static bool firsttime=true;
static int  loadtimeout=100; //100*20ms;




static int joy_setting = 1;              // 1 o 2
int vol_setting = 5;              // 1..10
static int btn2_setting = 0;             // indice keyset associato
char joy_str[16]  = "JOY=(1)";
char vol_str[16]  = "VOL=(5)";
char btn2_str[32] = "BTN2=(SPACE)";

const char* btn2set[] = { "SPACE","A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                           "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
                           "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
const int btn2set_lengths[] =  {sizeof(btn2set)/sizeof(char*)};
bool keyboard_active = false;
bool settings_active = false;
int key_cursor = 0;
int keyset_index = 0;
int settings_cursor = 0;

const char* keysets0[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                           "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
const char* keysets1[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
const char* keysets2[] = {  "\"",".", ",", "-", "+", "*", "/", "=", "(", ")","<", ">", ":", ";", "!", "?","@","#","$","%","&" };
const char* keysets3[] = { "SPACE", "RETURN", "DEL", "RUN" ,"RUNSTOP" };
const char* keysets4[] = { "F1","F2","F3","F4","F5","F6","F7","F8"};
const char* keysets5[] = { "PRINT", "LIST", "GOTO", "POKE", "LOAD" };
const char* keysets6[] = { "UP", "DOWN", "LEFT", "RIGHT", "HOME" };

const char** keysets[] = { keysets0, keysets1, keysets2, keysets3, keysets4, keysets5, keysets6 };
extern const int keyset_lengths[] = {
    sizeof(keysets0)/sizeof(char*),
    sizeof(keysets1)/sizeof(char*),
    sizeof(keysets2)/sizeof(char*),
    sizeof(keysets3)/sizeof(char*),
    sizeof(keysets4)/sizeof(char*),
    sizeof(keysets5)/sizeof(char*),
    sizeof(keysets6)/sizeof(char*)
};

int g_volume = 9;  // default: massimo

void audio_set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 9) vol = 9;
    g_volume = vol;
}




__attribute__((section(".time_critical")))  static void oneRasterLine(void) {
  static unsigned short lc = 1;
  while (true) {
    cpu.lineStartTime = fbmicros(); //get_ccount();
    cpu.lineCycles = cpu.lineCyclesAbs = 0;
    if (!cpu.exactTiming) {
      vic_do();
    } else {
      vic_do_simple();
    }
    if (--lc == 0) {
      lc = LINEFREQ / 10; // 10Hz
      cia1_checkRTCAlarm();
      cia2_checkRTCAlarm();
    }
    //Switch "ExactTiming" Mode off after a while:
    if (!cpu.exactTiming) break;
    if ( (fbmicros() - cpu.exactTimingStartTime)*1000 >= EXACTTIMINGDURATION ) {
      cpu_disableExactTiming();
      break;
    }
  };
}

const uint32_t ascii2scan[] = {
 //0 1 2 3:runstop 4 5 6 7 8 9 A B C D E F
   0,0,0,0x03,0,0,0,0,0,0,0,0,0,0x28,0,0, // return
 //     17:down  19:home                                             29:right
   0x00,0x51,0x00,0x1116,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4f,0x00,0x00,
   //sp  !       "     #     $      %      &      '     (        )     *    +    ,    -    .    / 
   0x2c,0x201e,0x201f,0x2020,0x2021,0x2022,0x2023,0x2024,0x2025,0x2026,0x55,0x57,0x36,0x56,0x37,0x54,
   //0  1    2    3    4    5    6    7    8    9    :    ;    <      =    >      ?
   0x27,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x33,0x34,0x2036,0x32,0x2037,0x2054,
   //@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O
   47,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,
   //P  Q    R    S    T    U    V    W    X    Y    Z    [      \     ]     ^    _  
   0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x2026,0x31,0x2027,0x00,0x00,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // ' a b c d e f g h i j k l m n o
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1117, // p q r s t u v w x y z { | } ~ DEL
 //? ?                  133:f1   f2   f3   f4   f5   f6   f7   f8 
   75,78,0x00,0x00,0x00,0x3a,0x203a,0x3c,0x203c,0x3e,0x203e,0x40,0x2040,0x00,0x00,0x00,  // 128-143
 //     145:up                                                       157:left
   0x00,0x2051,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x204f,0x00,0x00   // 144-159
};

// we also use USB matrix for the moment
static const uint8_t keymatrixmap[2][256] = {
  //Rows:
  // 0    1     2     3    4     5     6      7     8      9     A     B     C     D     E     F
  { 0x00, 0x00, 0x00, 0x80, 0x02, 0x08, 0x04, 0x04, 0x02, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x20, //0x00
    0x10, 0x10, 0x10, 0x20, 0x80, 0x04, 0x02, 0x04, 0x08, 0x08, 0x02, 0x04, 0x08, 0x02, 0x80, 0x80, //0x10
    0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x01, 0x80, 0x01, 0x00, 0x80, 0x00, 0x00, 0x20, //0x20
    0x00, 0x00, 0x40, 0x20, 0x40, 0x00, 0x20, 0x20, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, //0x30
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x00, 0x00, 0x80, 0x01, //0x40
    0x00, 0x01, 0x00, 0x00, 0x40, 0x40, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x50
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x60
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x70
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x80
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x90
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xA0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xB0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xC0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xD0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xE0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x40, 0x02
  }, //0xF0
  //Columns:
  // 0    1     2     3    4     5     6      7     8      9     A     B     C     D     E     F
  { 0x00, 0x00, 0x00, 0x80, 0x04, 0x10, 0x10, 0x04, 0x40, 0x20, 0x04, 0x20, 0x02, 0x04, 0x20, 0x04, //0x00
    0x10, 0x80, 0x40, 0x02, 0x40, 0x02, 0x20, 0x40, 0x40, 0x80, 0x02, 0x80, 0x02, 0x10, 0x01, 0x08, //0x10
    0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x02, 0x80, 0x01, 0x00, 0x10, 0x00, 0x00, 0x40, //0x20
    0x00, 0x00, 0x20, 0x20, 0x04, 0x00, 0x80, 0x10, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x40, 0x00, //0x30
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x40, 0x00, 0x00, 0x02, 0x04, //0x40
    0x00, 0x80, 0x00, 0x00, 0x80, 0x02, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x50
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x60
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x70
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x80
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x90
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xA0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xB0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xC0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xD0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0xE0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x04, 0x10, 0x80
  }
}; //0xF0 

struct {
  union {
    uint32_t kv;
    struct {
      uint8_t ke,   //Extratasten SHIFT, STRG, ALT...
              kdummy,
              k,    //Erste gedrückte Taste
              k2;   //Zweite gedrückte Taste
    };
  };
  uint32_t lastkv;
  uint8_t shiftLock;
} kbdData = {0, 0, 0};




static void setKey(uint32_t k, bool pressed) {
  if (pressed) {
    kbdData.kv = (k << 16);
    kbdData.ke = kbdData.k2;
    kbdData.k2 = 0;
  }
  else
  {
    kbdData.kv = 0;       
  }
}

static void pushStringToTextEntry(char * text) {
    char c;
    while ((c = *text++)) {
        setKey(ascii2scan[c], true); 
        sleep_ms(20);
        setKey(ascii2scan[c], false); 
        sleep_ms(20);
    }
}


uint8_t cia1PORTA(void) {
  uint8_t v;
  v = ~cpu.cia1.R[0x02] | (cpu.cia1.R[0x00] & cpu.cia1.R[0x02]);
  int keys = emu_GetPad();
#ifndef PICOMPUTER
  /*
  if (oskbActive) keys = 0;
  */
#endif  
  // Disabilita il joystick quando la tastiera virtuale è attiva
  if (!keyboard_active) {
    if (!cpu.swapJoysticks) {
      if (keys & MASK_JOY2_BTN) v &= 0xEF;
      if (keys & MASK_JOY2_UP) v &= 0xFE;
      if (keys & MASK_JOY2_DOWN) v &= 0xFD;
      if (keys & MASK_JOY2_RIGHT) v &= 0xFB;
      if (keys & MASK_JOY2_LEFT) v &= 0xF7;
    } else {
      if (keys & MASK_JOY1_BTN) v &= 0xEF;
      if (keys & MASK_JOY1_UP) v &= 0xFE;
      if (keys & MASK_JOY1_DOWN) v &= 0xFD;
      if (keys & MASK_JOY1_RIGHT) v &= 0xFB;
      if (keys & MASK_JOY1_LEFT) v &= 0xF7;
    }
  }	


  if (!kbdData.kv) return v; //Keine Taste gedrückt

  uint8_t filter = ~cpu.cia1.R[0x01] & cpu.cia1.R[0x03];
  
  if (kbdData.k) {
    if ( keymatrixmap[1][kbdData.k] & filter)  v &= ~keymatrixmap[0][kbdData.k];
  }

  if (kbdData.ke) {
    if (kbdData.ke & 0x02) { //Shift-links
      if ( keymatrixmap[1][0xff] & filter) v &= ~keymatrixmap[0][0xff];
    }
    if (kbdData.ke & 0x20) { //Shift-rechts
      if ( keymatrixmap[1][0xfe] & filter) v &= ~keymatrixmap[0][0xfe];
    }
    if (kbdData.ke & 0x11) { //Control
      if ( keymatrixmap[1][0xfd] & filter) v &= ~keymatrixmap[0][0xfd];
    }
    if (kbdData.ke & 0x88) { //Windows (=> Commodore)
      if ( keymatrixmap[1][0xfc] & filter) v &= ~keymatrixmap[0][0xfc];
    }
  }
 
  return v;
}


uint8_t cia1PORTB(void) {

  uint8_t v;

  v = ~cpu.cia1.R[0x03] | (cpu.cia1.R[0x00] & cpu.cia1.R[0x02]) ;
  int keys = emu_GetPad();

  // Disabilita il joystick quando la tastiera virtuale è attiva
  if (!keyboard_active) {
    if (!cpu.swapJoysticks) {
      if (keys & MASK_JOY1_BTN) v &= 0xEF;
      if (keys & MASK_JOY1_UP) v &= 0xFE;
      if (keys & MASK_JOY1_DOWN) v &= 0xFD;
      if (keys & MASK_JOY1_RIGHT) v &= 0xFB;
      if (keys & MASK_JOY1_LEFT) v &= 0xF7;
    } else {
      if (keys & MASK_JOY2_BTN) v &= 0xEF;
      if (keys & MASK_JOY2_UP) v &= 0xFE;
      if (keys & MASK_JOY2_DOWN) v &= 0xFD;
      if (keys & MASK_JOY2_RIGHT) v &= 0xFB;
      if (keys & MASK_JOY2_LEFT) v &= 0xF7;
    }
  }

  if (!kbdData.kv) return v; //Keine Taste gedrückt

  uint8_t filter = ~cpu.cia1.R[0x00] & cpu.cia1.R[0x02];

  if (kbdData.k) {
    if ( keymatrixmap[0][kbdData.k] & filter) v &= ~keymatrixmap[1][kbdData.k];
  }

  if (kbdData.ke) {
    if (kbdData.ke & 0x02) { //Shift-links
      if ( keymatrixmap[0][0xff] & filter) v &= ~keymatrixmap[1][0xff];
    }
    if (kbdData.ke & 0x20) { //Shift-rechts
      if ( keymatrixmap[0][0xfe] & filter) v &= ~keymatrixmap[1][0xfe];
    }
    if (kbdData.ke & 0x11) { //Control
      if ( keymatrixmap[0][0xfd] & filter) v &= ~keymatrixmap[1][0xfd];
    }
    if (kbdData.ke & 0x88) { //Windows (=> Commodore)
      if ( keymatrixmap[0][0xfc] & filter) v &= ~keymatrixmap[1][0xfc];
    }
  }

  return v;
}



void updateSettingsStrings() {
    snprintf(joy_str, sizeof(joy_str), "JOY=(%d)", joy_setting);
    snprintf(vol_str, sizeof(vol_str), "VOL=(%d)", vol_setting);

    // Preleva token da keysets1..6 in base a btn2_setting
    const char* btn2_token = nullptr;
    if (btn2_setting >= 0 && btn2_setting < btn2set_lengths[0]) {
        btn2_token = btn2set[btn2_setting];   // esempio da keyset1
    }
    // in alternativa: puoi decidere se prenderli da tutti i set (1..6)

    if (btn2_token) {
        snprintf(btn2_str, sizeof(btn2_str), "BTN2=(%s)", btn2_token);
    } else {
        snprintf(btn2_str, sizeof(btn2_str), "BTN2=(SPACE)");
    }
}



void c64_Init(void)
{
  updateSettingsStrings();
  disableEventResponder();
  resetPLA();
  resetCia1();
  resetCia2();
  resetVic();
  cpu_reset();
#ifdef HAS_SND  
  playSID.begin();  
  emu_sndInit();
#endif  
}


void c64_Step(void)
{
	oneRasterLine();
}

void c64_Start(char * filename)
{
}




void sendKeyFromVirtualKeyboard(const char* token) {
    if (!token || strlen(token) == 0) return;

    static char tempbuffer[64];
    memset(tempbuffer, 0, sizeof(tempbuffer));
    tempbuffer[0] = '\t';

    // Mappatura tasti speciali
    if (strcasecmp(token, "RETURN") == 0) {
        tempbuffer[0] = C64_KEY_RETURN;
    }
    else if (strcasecmp(token, "HOME") == 0) {
        tempbuffer[0] = C64_KEY_HOME;
    }
    else if (strcasecmp(token, "RUN") == 0) {
         strncpy(tempbuffer, "RUN\r", sizeof(tempbuffer)-1);
    }
    else if (strcasecmp(token, "RUNSTOP") == 0) {
        tempbuffer[0] = C64_KEY_RUNSTOP;
    }
    else if (strcasecmp(token, "SPACE") == 0) {
        tempbuffer[0] = C64_KEY_SPACE;
    }
    else if (strcasecmp(token, "DEL") == 0) {
        tempbuffer[0] = C64_KEY_DEL;
    }
    else if (strcasecmp(token, "F1") == 0) {
        tempbuffer[0] = C64_KEY_F1;
    }
    else if (strcasecmp(token, "F2") == 0) {
      tempbuffer[0] = C64_KEY_F2;
    }
    else if (strcasecmp(token, "F3") == 0) {
      tempbuffer[0] = C64_KEY_F3;
    }
    else if (strcasecmp(token, "F4") == 0) {
      tempbuffer[0] = C64_KEY_F4;
    }
    else if (strcasecmp(token, "F5") == 0) {
      tempbuffer[0] = C64_KEY_F5;
    }
    else if (strcasecmp(token, "F6") == 0) {
      tempbuffer[0] = C64_KEY_F6;
    }
    else if (strcasecmp(token, "F7") == 0) {
      tempbuffer[0] = C64_KEY_F7;
    }
    else if (strcasecmp(token, "F8") == 0) {
      tempbuffer[0] = C64_KEY_F8;
    }
    else if (strcasecmp(token, "UP") == 0) {
      tempbuffer[0] = C64_KEY_UP;
    }
    else if (strcasecmp(token, "DOWN") == 0) {
      tempbuffer[0] = C64_KEY_DOWN;
    }
    else if (strcasecmp(token, "LEFT") == 0) {
      tempbuffer[0] = C64_KEY_LEFT;
    }
    else if (strcasecmp(token, "RIGHT") == 0) {
      tempbuffer[0] = C64_KEY_RIGHT;
    }
    else if (strcasecmp(token, joy_str) == 0) {
      cpu.swapJoysticks = !cpu.swapJoysticks;
      tempbuffer[0] = '\t';
    }
    else if (strcasecmp(token, vol_str) == 0) {

      tempbuffer[0] = '\t';
    }
    else if (strcasecmp(token, btn2_str) == 0) {
      //da implementare schelta funzione abbinata al btn2

      tempbuffer[0] = '\t';
    }





    else {
        // Per caratteri normali
        strncpy(tempbuffer, token, sizeof(tempbuffer)-1);
    }

    tempbuffer[sizeof(tempbuffer)-1] = '\t';
    textseq = tempbuffer;
    
    nbkeys = strlen(textseq);
    
    kcnt = 0;
    toggle = true;
}



void c64_Input(int bClick) {
  static int prevClick = 0;
  static bool toggle = true;
  static absolute_time_t last_key_time = nil_time;

  static uint32_t lastKeyTime = 0;
  static uint16_t lastKeyPressed = 0;
  static uint32_t debounceTime = 0;
  
  // Per gestire la pressione prolungata di USER2 (attivazione tastiera) e USER1 (menu settings)
  static uint32_t user2PressTime = 0;
  static bool user2WasPressed = false;
  static uint32_t user1PressTime = 0;
  static bool user1WasPressed = false;
  static bool user1ShortPressDetected = false;
  static bool user1JustOpenedSettings = false;
  static bool user2JustOpenedKeyboard = false;

  uint16_t bCurState = emu_ReadKeys();
  uint32_t now = to_ms_since_boot(get_absolute_time());

  int keyDelay  = 350;   // ms prima del repeat
  int keyRepeat = 60;   // ms tra ripetizioni
  int debounceMs = 50;   // ms per ignorare rimbalzi

  bClick = 0;

  if (bCurState != 0) {
      if (bCurState != lastKeyPressed) {
          if (now - debounceTime > (uint32_t)debounceMs) {
              bClick = bCurState;
              lastKeyPressed = bCurState;
              lastKeyTime = now;
              debounceTime = now;
          }
      } else {
          if (now - lastKeyTime > (uint32_t)keyDelay) {
              bClick = bCurState;
              lastKeyTime = now - (keyDelay - keyRepeat);
          }
      }
  } else {
      lastKeyPressed = 0;
  }




  // Gestione tastiera virtuale
  // Se la tastiera è attiva: pressione semplice per chiudere
  // Se la tastiera è disattivata: pressione prolungata (800ms) per aprire
  if ((bCurState & MASK_JOY2_UP) && (bCurState & MASK_JOY2_DOWN)) {
    if (!user2WasPressed) {
      // Inizia a contare il tempo di pressione
      user2PressTime = now;
      user2WasPressed = true;
      user2JustOpenedKeyboard = false;
    } else {
      // Se la tastiera è già attiva, chiudi subito senza aspettare il timeout
      if (keyboard_active && !user2JustOpenedKeyboard) {
        // Non fare nulla, aspetta il rilascio
      } else if (!keyboard_active) {
        // Tastiera non attiva: controlla se sono stati premuti abbastanza a lungo (800ms)
        if ((now - user2PressTime) > 800) {
          // Apri tastiera
          keyboard_active = true;
          settings_active = false;
          key_cursor = 0;
          keyset_index = 0;
          nbkeys = 0;
          kcnt = 0;
          textseq = nullptr;
          user2JustOpenedKeyboard = true;
          return;
        }
      }
    }
  } else {
    // Tasti rilasciati
    if (user2WasPressed) {
      // Se la tastiera era attiva e NON è stata appena aperta, chiudila
      if (keyboard_active && !user2JustOpenedKeyboard) {
        keyboard_active = false;
      }
    }
    user2WasPressed = false;
    user2PressTime = 0;
    user2JustOpenedKeyboard = false;
  }

  // Gestione menu settings
  // Se settings è attivo: pressione semplice per chiudere
  // Se settings è disattivato: pressione prolungata (800ms) per aprire
  if ((bCurState & MASK_JOY2_LEFT) && (bCurState & MASK_JOY2_RIGHT)) {
    if (!user1WasPressed) {
      // Inizia a contare il tempo di pressione
      user1PressTime = now;
      user1WasPressed = true;
      user1ShortPressDetected = false;
      user1JustOpenedSettings = false;
    } else {
      // Se settings è già attivo, chiudi subito senza aspettare il timeout
      if (settings_active && !user1JustOpenedSettings) {
        // Non fare nulla, aspetta il rilascio
      } else if (!settings_active) {
        // Settings non attivo: controlla se sono stati premuti abbastanza a lungo (800ms)
        if ((now - user1PressTime) > 800) {
          // Apri menu settings
          settings_active = true;
          keyboard_active = false;
          settings_cursor = 0;
          user1JustOpenedSettings = true;
          return;
        }
      }
    }
  } else {
    // Tasti rilasciati
    if (user1WasPressed) {
      // Se settings era attivo e NON è stato appena aperto, chiudilo
      if (settings_active && !user1JustOpenedSettings) {
        settings_active = false;
        user1WasPressed = false;
        user1PressTime = 0;
        user1ShortPressDetected = false;
      }
      // Se settings NON era attivo e abbiamo rilasciato prima del timeout = pressione breve
      else if (!settings_active && (now - user1PressTime) < 800) {
        user1ShortPressDetected = true;
      }
    }
    user1WasPressed = false;
    user1PressTime = 0;
    user1JustOpenedSettings = false;
  }

  // USER1 (LEFT+RIGHT) - Solo se è stata rilevata una pressione breve
  if (user1ShortPressDetected) {
    user1ShortPressDetected = false;
    
    // Con la tastiera attiva, USER1 funziona sempre (anche in fase di preload)
    if (keyboard_active) {
      const char* token = nullptr;
      if (btn2_setting >= 0 && btn2_setting < btn2set_lengths[0]) {
          token = btn2set[btn2_setting];
      }
      if (token) {
          sendKeyFromVirtualKeyboard(token);
      }
      prevClick = bClick;
      return;
    }
    // Con tastiera disattivata, dopo la fase di LOAD invia il tasto configurato
    else if (loadtimeout == 0 && !firsttime) {
      const char* token = nullptr;
      if (btn2_setting >= 0 && btn2_setting < btn2set_lengths[0]) {
          token = btn2set[btn2_setting];
      }
      if (token) {
          sendKeyFromVirtualKeyboard(token);
      }
      prevClick = bClick;
      return;
    }
    // Altrimenti esegui LOAD (nella fase iniziale)
    else if (loadtimeout == 0 && firsttime) {
      firsttime = false;
      textseq = textload;
      nbkeys = strlen(textseq);
      kcnt = 0;
      toggle = true;
      return;
    }
  }

  // Gestione menu settings indipendente
  if (settings_active) {
    const int num_settings = 3; // JOY, VOL, BTN2

    // Navigazione tra le impostazioni (LEFT/RIGHT)
    if ((bClick & MASK_JOY2_LEFT) && !(prevClick & MASK_JOY2_LEFT)) {
      settings_cursor = (settings_cursor + 1) % num_settings;
    }
    if ((bClick & MASK_JOY2_RIGHT) && !(prevClick & MASK_JOY2_RIGHT)) {
      settings_cursor = (settings_cursor - 1 + num_settings) % num_settings;
    }

    // Modifica valori (UP/DOWN)
    if ((bClick & MASK_JOY2_UP) && !(prevClick & MASK_JOY2_UP)) {
      if (settings_cursor == 0) {          // JOY
        joy_setting = (joy_setting == 1) ? 2 : 1;
        cpu.swapJoysticks = (joy_setting == 2);
      } else if (settings_cursor == 1) {   // VOL
        if (vol_setting < 9) vol_setting++;
        audio_set_volume(vol_setting);
      } else if (settings_cursor == 2) {   // BTN2
        btn2_setting = (btn2_setting + 1) % btn2set_lengths[0];
      }
      updateSettingsStrings();
    }
    if ((bClick & MASK_JOY2_DOWN) && !(prevClick & MASK_JOY2_DOWN)) {
      if (settings_cursor == 0) {          // JOY
        joy_setting = (joy_setting == 1) ? 2 : 1;
        cpu.swapJoysticks = (joy_setting == 2);
      } else if (settings_cursor == 1) {   // VOL
        if (vol_setting > 0) vol_setting--;
        audio_set_volume(vol_setting);
      } else if (settings_cursor == 2) {   // BTN2
        btn2_setting = (btn2_setting - 1 + btn2set_lengths[0]) % btn2set_lengths[0];
      }
      updateSettingsStrings();
    }

    prevClick = bClick;
    return;
  }

if (keyboard_active) {
    const char** current_set = keysets[keyset_index];
    int set_len = keyset_lengths[keyset_index];

    // Navigazione tra i token (LEFT/RIGHT)
    if ((bClick & MASK_JOY2_LEFT) && !(prevClick & MASK_JOY2_LEFT)) {
        key_cursor = (key_cursor + 1) % set_len;
    }
    if ((bClick & MASK_JOY2_RIGHT) && !(prevClick & MASK_JOY2_RIGHT)) {
        key_cursor = (key_cursor - 1 + set_len) % set_len;
    }

    // Cambio keyset con UP/DOWN
    if ((bClick & MASK_JOY2_UP) && !(prevClick & MASK_JOY2_UP)) {
        keyset_index = (keyset_index + 1) % (sizeof(keysets)/sizeof(keysets[0]));
        key_cursor = 0;
    }
    if ((bClick & MASK_JOY2_DOWN) && !(prevClick & MASK_JOY2_DOWN)) {
        keyset_index = (keyset_index + (sizeof(keysets)/sizeof(keysets[0])) - 1) % (sizeof(keysets)/sizeof(keysets[0]));
        key_cursor = 0;
    }

    // BTN (JOY2_BTN) = invio token
    if ((bClick & MASK_JOY2_BTN) && !(prevClick & MASK_JOY2_BTN)) {
        const char* token = keysets[keyset_index][key_cursor];
        sendKeyFromVirtualKeyboard(token);
    }

    // Invio sequenza key (come già avevi)
    if (nbkeys > 0) {
        char k = textseq[kcnt];
        if (k != '\t') {
            setKey(ascii2scan[k], toggle);
            if (!toggle) { kcnt++; nbkeys--; }
            toggle = !toggle;
        } else {
            kcnt++; nbkeys--;
        }
    }

    prevClick = bClick;
    return;
}


    // Gestione LOAD (rimossa, ora gestita sopra in USER1 pressione breve)
  if (nbkeys == 0) {
    if (loadtimeout > 0) loadtimeout--;
  } else if (nbkeys > 0) {
    char k = textseq[kcnt];
    if (k != '\t') {
      setKey(ascii2scan[k], toggle);
      if (!toggle) {
        kcnt++;
        nbkeys--;
      }
      toggle = !toggle;
    } else {
      kcnt++;
      nbkeys--;
    }
  }
  prevClick = bClick;
}

#ifdef HAS_SND      
void  SND_Process( void * stream, int len )
{
    playSID.update(stream, len);
}
#endif  
