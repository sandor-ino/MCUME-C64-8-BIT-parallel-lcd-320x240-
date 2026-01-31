#ifndef Teensy64_cia2_h_
#define Teensy64_cia2_h_


void cia2_clock(int clk) __attribute__ ((hot));
void cia2_checkRTCAlarm() __attribute__ ((hot));
void cia2_write(uint32_t address, uint8_t value) __attribute__ ((hot));
uint8_t cia2_read(uint32_t address) __attribute__ ((hot));

void resetCia2(void);

#endif
