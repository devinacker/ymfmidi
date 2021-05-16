#ifndef __SEQUENCE_H
#define __SEQUENCE_H

#include "player.h"

class Sequence
{
public:
	Sequence(FILE *file) {}
	virtual ~Sequence();
	
	// load a sequence from the given path
	static Sequence* load(const char *path);
	
	// reset track to beginning
	virtual void reset() = 0;
	
	// process and play any pending MIDI events
	// returns the number of output audio samples until the next event(s)
	virtual uint32_t update(OPLPlayer& player) = 0;
};

#endif // __SEQUENCE_H

