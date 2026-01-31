#ifndef output_dac_h_
#define output_dac_h_

#include "reSID.h"
extern AudioPlaySID playSID;


class AudioOutputAnalog
{
public:
	//AudioOutputAnalog(void) : AudioStream(1, inputQueueArray) { begin(); }
	virtual void update(void);
	void begin(void);
	void analogReference(int ref);
	//static DMAChannel dma;
//private:
	//static audio_block_t *block_left_1st;
	//static audio_block_t *block_left_2nd;
	static bool update_responsibility;
	//audio_block_t *inputQueueArray[1];
	static void isr(void);
	static uint8_t volume;

};

#endif
