#include "reSID.h"
#include <math.h>

#define AUDIO_BLOCK_SAMPLES 443
#define SAMPLERATE 22050
#define CLOCKFREQ 985248

void AudioPlaySID::begin(void)
{
	sidptr = &sid;
	this->reset();
	setSampleParameters(CLOCKFREQ, SAMPLERATE);
	playing = true;
}

void AudioPlaySID::setSampleParameters(float clockfreq, float samplerate) {
	sid.set_sampling_parameters(clockfreq, SAMPLE_FAST, samplerate); 
	csdelta = round((float)clockfreq / ((float)samplerate / AUDIO_BLOCK_SAMPLES));
}

void AudioPlaySID::reset(void)
{
	sid.reset();
}

void AudioPlaySID::stop(void)
{
	playing = false;	
}

void AudioPlaySID::update(void * stream, int len) {
	// only update if we're playing
	if (!playing) return;

	cycle_count delta_t = csdelta;
	
	sidptr->clock(delta_t, (short int*)stream, len);
}
