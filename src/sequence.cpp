#include <cstdio>

#include "sequence.h"
#include "sequence_mid.h"
#include "sequence_mus.h"

// ----------------------------------------------------------------------------
Sequence::~Sequence() {}

// ----------------------------------------------------------------------------
Sequence* Sequence::load(const char *path, int offset)
{
	FILE *file = fopen(path, "rb");
	if (!file) return nullptr;
	
	Sequence *seq = load(file, offset);
	
	fclose(file);
	return seq;
}

// ----------------------------------------------------------------------------
Sequence* Sequence::load(FILE *file, int offset)
{
	if (SequenceMUS::isValid(file, offset))
		return new SequenceMUS(file, offset);
	else if (SequenceMID::isValid(file, offset))
		return new SequenceMID(file, offset);
	
	return nullptr;
}
