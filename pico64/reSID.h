#include "reSID/sid.h"
#include <stdint.h>

#ifndef play_sid_h_
#define play_sid_h_


class AudioPlaySID
{
public:
	AudioPlaySID(void) { begin(); }
	void begin(void);
	void setSampleParameters(float clockfreq, float samplerate);
	inline void setreg(int ofs, int val) { sid.write(ofs, val); }
	inline uint8_t getreg(int ofs) { return sid.read(ofs); }
	void reset(void);
	void stop(void);
	void update(void * stream, int len);	
	inline bool isPlaying(void) { return playing; }	
private:
	cycle_count csdelta;
	volatile bool playing;
	SID sid;
	SID* sidptr;
};


#endif
