#ifndef __SEQUENCE_MID_H
#define __SEQUENCE_MID_H

#include "sequence.h"

class MIDTrack;

class SequenceMID : public Sequence
{
public:
	SequenceMID(FILE *file, int offset = 0);
	~SequenceMID();
	
	void reset();
	uint32_t update(OPLPlayer& player);
	
	void setUsecPerBeat(uint32_t usec);
	
	static bool isValid(FILE *file, int offset = 0);
	
private:
	std::vector<MIDTrack*> m_tracks;
	
	uint16_t m_type;
	uint16_t m_ticksPerBeat;
	double m_ticksPerSec;
};

#endif // __SEQUENCE_MUS_H
