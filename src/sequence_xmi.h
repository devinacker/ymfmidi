#ifndef __SEQUENCE_XMI_H
#define __SEQUENCE_MID_H

#include "sequence.h"

class XMITrack;

class SequenceXMI : public Sequence
{
public:
	SequenceXMI(FILE *file, int offset = 0);
	~SequenceXMI();
	
	void reset();
	uint32_t update(OPLPlayer& player);
	
	unsigned numSongs() const;
	
	void setUsecPerBeat(uint32_t usec);
	
	static bool isValid(FILE *file, int offset = 0);
	
private:
	uint32_t readRootChunk(FILE *file);
	void setDefaults();

	std::vector<XMITrack*> m_tracks;
	
	double m_ticksPerSec;
};

#endif // __SEQUENCE_MUS_H
