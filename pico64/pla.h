#ifndef Teensy64_pla_h_
#define Teensy64_pla_h_

#define CONSTROM const

#define MEM_BASIC_ROM 0xA000
#define MEM_CHARSET_ROM 0xD000
#define MEM_KERNAL_ROM  0xE000

#define MEM_VIC     0xD000
#define MEM_VICCOLOR  0xD800
#define MEM_SID     0xD400
#define MEM_CIA1    0xDC00
#define MEM_CIA2    0xDD00

//C64 Memory/Device access (PLA)

/* READ */
typedef uint8_t (*r_ptr_t)( uint32_t address ); //Funktionspointer auf uint8_t foo(uint16_t address);
typedef r_ptr_t rarray_t[256];          //Array von Funktionspointern
typedef rarray_t * r_rarr_ptr_t;        //Pointer auf Array von Funktionspointern

/* WRITE */
typedef void (*w_ptr_t)( uint32_t address, uint8_t value ); //Funktionspointer auf void foo( uint16_t address, uint8_t value );
typedef w_ptr_t warray_t[256];                //Array von Funktionspointern
typedef warray_t * w_rarr_ptr_t;              //Pointer auf Array von Funktionspointern


void resetPLA(void);

#endif
