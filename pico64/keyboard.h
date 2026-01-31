#ifndef Teensy64_keyboard_h_
#define Teensy64_keyboard_h_

void initKeyboard();
void initJoysticks();

void sendKey(char key);
void sendString(const char * p);
void do_sendString();//call in yield()

uint8_t cia1PORTA(void);
uint8_t cia1PORTB(void);

#endif