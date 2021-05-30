#ifndef __SEQUENCE_MID_H
#define __SEQUENCE_MID_H

#include "sequence.h"

class MIDTrack;

class SequenceMID : public Sequence
{
public:
	SequenceMID(const uint8_t *data, size_t size);
	~SequenceMID();
	
	void reset();
	uint32_t update(OPLPlayer& player);
	
	unsigned numSongs() const;
	
	void setUsecPerBeat(uint32_t usec);
	
	static bool isValid(const uint8_t *data, size_t size);

private:
	void readTracks(const uint8_t *data, size_t size);
	void setDefaults();
	
	std::vector<MIDTrack*> m_tracks;
	
	uint16_t m_type;
	uint16_t m_ticksPerBeat;
	double m_ticksPerSec;
};

#endif // __SEQUENCE_MUS_H
