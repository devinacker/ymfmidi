#ifndef __SEQUENCE_MUS_H
#define __SEQUENCE_MUS_H

#include "sequence.h"

class SequenceMUS : public Sequence
{
public:
	SequenceMUS(const uint8_t *data, size_t size);
	
	void reset();
	uint32_t update(OPLPlayer& player);
	
	static bool isValid(const uint8_t *data, size_t size);
	
private:
	uint8_t m_data[1 << 16];
	uint16_t m_pos;
	uint8_t m_lastVol[16];
	
	void setDefaults();
};

#endif // __SEQUENCE_MUS_H
