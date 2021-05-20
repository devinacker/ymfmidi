#ifndef __SEQUENCE_H
#define __SEQUENCE_H

#include "player.h"

class Sequence
{
public:
	Sequence(FILE *file) { reset(); }
	virtual ~Sequence();
	
	// load a sequence from the given path/file
	static Sequence* load(const char *path, int offset = 0);
	static Sequence* load(FILE *path, int offset = 0);
	
	// reset track to beginning
	virtual void reset() { m_atEnd = false; }
	
	// process and play any pending MIDI events
	// returns the number of output audio samples until the next event(s)
	virtual uint32_t update(OPLPlayer& player) = 0;
	
	// has this track reached the end?
	// (this is true immediately after ending/looping, then becomes false after updating again)
	bool atEnd() const { return m_atEnd; }
	
protected:
	bool m_atEnd;
};

#endif // __SEQUENCE_H

