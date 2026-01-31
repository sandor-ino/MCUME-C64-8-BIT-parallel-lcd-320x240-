#include <stdint.h>
#include <stdio.h>
#include "timerutil.h"

//Attention, don't use WFI-instruction - the CPU does not count cycles during sleep
void enableCycleCounter(void) {

}

extern "C" volatile uint32_t systick_millis_count;
void mySystick_isr(void) { systick_millis_count++; }
void myUnused_isr(void) {};

void disableEventResponder(void) {
}

static float setDACFreq(float freq) {

  return (float)0;
}

float setAudioSampleFreq(float freq) {
  int f=0;
  return f;
}

void setAudioOff(void) {

}

void setAudioOn(void) {

}

void listInterrupts() {

}