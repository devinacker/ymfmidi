#ifndef __SEQUENCE_MUS_H
#define __SEQUENCE_MUS_H

#include "sequence.h"

class SequenceMUS : public Sequence
{
public:
	SequenceMUS(FILE *file, int offset = 0);
	
	void reset();
	uint32_t update(OPLPlayer& player);
	
	static bool isValid(FILE *file, int offset = 0);
	
private:
	uint8_t m_data[1 << 16];
	uint16_t m_pos;
	uint8_t m_lastVol[16];
};

#endif // __SEQUENCE_MUS_H
