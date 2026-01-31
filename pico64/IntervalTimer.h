#ifndef __INTERVALTIMERX_H__
#define __INTERVALTIMERX_H__

//#include "kinetis.h"

#ifdef __cplusplus
extern "C" {
#endif

class MyIntervalTimer {
private:
	static const uint32_t MAX_PERIOD = UINT32_MAX / (F_BUS / 1000000.0);
public:
	MyIntervalTimer() {
		//channel = NULL;
		//nvic_priority = 128;
	}
	~MyIntervalTimer() {
		end();
	}
	/*
	bool begin(void (*funct)(), unsigned int microseconds) {
		if (microseconds == 0 || microseconds > MAX_PERIOD) return false;
		uint32_t cycles = (F_BUS / 1000000) * microseconds - 1;
		if (cycles < 36) return false;
		return beginCycles(funct, cycles);
	}
	bool begin(void (*funct)(), int microseconds) {
		if (microseconds < 0) return false;
		return begin(funct, (unsigned int)microseconds);
	}
	bool begin(void (*funct)(), unsigned long microseconds) {
		return begin(funct, (unsigned int)microseconds);
	}
	bool begin(void (*funct)(), long microseconds) {
		return begin(funct, (int)microseconds);
	}
	bool begin(void (*funct)(), float microseconds) {
		if (microseconds <= 0 || microseconds > MAX_PERIOD) return false;
		uint32_t cycles = (float)(F_BUS / 1000000) * microseconds - 0.5;
		if (cycles < 36) return false;
		return beginCycles(funct, cycles);
	}
	bool begin(void (*funct)(), double microseconds) {
		return begin(funct, (float)microseconds);
	}
	*/
	void setIntervalFast(float microseconds) { /*NEW*/
		uint32_t cycles = (float)(F_BUS / 1000000) * microseconds - 0.5;
		//channel->LDVAL = cycles;		
	}	
	/*
	bool setInterval(float microseconds) { 
		//if (!channel) return false;
		if (microseconds <= 0 || microseconds > MAX_PERIOD) return false;		
		setIntervalFast(microseconds);
		return true;		
	}
	*/
	void end() {};
	/*
	void priority(uint8_t n) {
		nvic_priority = n;
		if (channel) {
			int index = channel - KINETISK_PIT_CHANNELS;
			//NVIC_SET_PRIORITY(IRQ_PIT_CH0 + index, nvic_priority);
		}
	}
	*/
	//operator IRQ_NUMBER_t() {
		/*
		if (channel) {
			int index = channel - KINETISK_PIT_CHANNELS;
			return (IRQ_NUMBER_t)(IRQ_PIT_CH0 + index);
		}
		*/
		//return (IRQ_NUMBER_t)NVIC_NUM_INTERRUPTS;
	//}
private:
	//KINETISK_PIT_CHANNEL_t *channel;
	//uint8_t nvic_priority;
	bool beginCycles(void (*funct)(), uint32_t cycles);

};

#ifdef __cplusplus
}
#endif

#endif
