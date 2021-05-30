#ifndef __SEQUENCE_XMI_H
#define __SEQUENCE_MID_H

#include "sequence.h"

class XMITrack;

class SequenceXMI : public Sequence
{
public:
	SequenceXMI(const uint8_t *data, size_t size);
	~SequenceXMI();
	
	void reset();
	uint32_t update(OPLPlayer& player);
	
	unsigned numSongs() const;
	
	void setUsecPerBeat(uint32_t usec);
	
	static bool isValid(const uint8_t *data, size_t size);
	
private:
	uint32_t readRootChunk(const uint8_t *data, size_t size);
	void setDefaults();

	std::vector<XMITrack*> m_tracks;
	
	double m_ticksPerSec;
};

#endif // __SEQUENCE_MUS_H
