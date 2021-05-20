#include <cstdio>
#include <cstring>

#include "patches.h"

// ----------------------------------------------------------------------------
bool OPLPatch::load(const char *path, OPLPatch (&patches)[256], int offset)
{
	FILE *file = fopen(path, "rb");
	if (!file) return false;

	bool ok = load(file, patches, offset);
	
	fclose(file);
	
	return ok;
}

// ----------------------------------------------------------------------------
bool OPLPatch::load(FILE *file, OPLPatch (&patches)[256], int offset)
{
	return loadWOPL(file, patches, offset)
	       || loadOP2(file, patches, offset);
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadWOPL(FILE *file, OPLPatch (&patches)[256], int offset)
{
	uint8_t bytes[66] = {0};
	
	fseek(file, offset, SEEK_SET);
	fread(bytes, 1, 19, file);
	if (strcmp((const char*)bytes, "WOPL3-BANK"))
		return false;
	
	// mixed endianness? why???
	uint16_t version   = bytes[11] | (bytes[12] << 8);
	uint16_t numMelody = (bytes[13] << 8) | bytes[14];
	uint16_t numPerc   = (bytes[15] << 8) | bytes[16];
	
	if (version > 3)
		return false;
	
	// currently not supported: global LFO flags, volume model options
	
	if (version >= 2) // skip bank names
		fseek(file, offset + 34 * (numMelody + numPerc), SEEK_CUR);
	
	const unsigned instSize = (version >= 3) ? 66 : 62;
	
	for (unsigned i = 0; i < 128 * (numMelody + numPerc); i++)
	{
		fread(bytes, 1, instSize, file);
		
		const unsigned bank = i >> 7;
		unsigned key;
		if (bank < numMelody)
			key = (bank << 8) | (i & 0x7f);
		else
			key = ((bank - numMelody) << 8) | (i & 0x7f) | 0x80;
		// only 1 melody and percussion bank supported right now
		if (key & 0xff00)
			continue;
		
		OPLPatch &patch = patches[key];
		
		// clear patch data
		patch = OPLPatch();
		
		// patch names
		bytes[31] = '\0';
		if (bytes[0])
			patch.name = (const char*)bytes;
		else
			patch.name = names[key];
		
		// patch global settings
		patch.voice[0].tune     = (int8_t)bytes[33] - 12;
		patch.voice[1].tune     = (int8_t)bytes[35] - 12;
		patch.velocity          = (int8_t)bytes[36];
		patch.voice[1].finetune = (int8_t)bytes[37] / 128.0;
		patch.fixedNote         = bytes[38];
		patch.fourOp            = bytes[39] & 1;
		patch.dualTwoOp         = bytes[39] & 2;
		// ignore other data for this patch if it's a blank instrument
		// *or* if one of the rhythm mode bits is set (not supported here)
		if (bytes[39] & 0x3c)
			continue;
		
		patch.voice[0].conn = bytes[40];
		patch.voice[1].conn = bytes[41];
		
		// patch operator settings
		unsigned pos = 42;
		for (unsigned op = 0; op < 4; op++)
		{
			PatchVoice &voice = patch.voice[op/2];
			
			const unsigned n = (op % 2) ^ 1;
			
			voice.op_mode[n]  = bytes[pos++];
			voice.op_ksr[n]   = bytes[pos]   & 0xc0;
			voice.op_level[n] = bytes[pos++] & 0x3f;
			voice.op_ad[n]    = bytes[pos++];
			voice.op_sr[n]    = bytes[pos++];
			voice.op_wave[n]  = bytes[pos++];
		}
	}
	
	return true;
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadOP2(FILE *file, OPLPatch (&patches)[256], int offset)
{
	uint8_t bytes[36] = {0};

	fseek(file, offset, SEEK_SET);
	fread(bytes, 1, 8, file);
	if (strncmp((const char*)bytes, "#OPL_II#", 8))
		return false;

	// read data for all patches (128 melodic + 47 percussion)
	for (int i = 0; i < 128+47; i++)
	{
		// patches 0-127 are melodic; the rest are for percussion notes 35 thru 81
		unsigned key = (i < 128) ? i : (i + 35);
		
		OPLPatch &patch = patches[key];
		// clear patch data
		patch = OPLPatch();
		
		// seek to patch data
		fseek(file, offset + (36*i) + 8, SEEK_SET);
		fread(bytes, 1, 36, file);
		
		// read the common data for both 2op voices
		// flag bit 0 is "fixed pitch" (for drums), but it's seemingly only used for drum patches anyway, so ignore it?
		patch.fourOp = patch.dualTwoOp = (bytes[0] & 4);
		// second voice detune
		patch.voice[1].finetune = (bytes[2] / 128.0) - 1.0;
	
		patch.fixedNote = bytes[3];
		
		// read data for both 2op voices
		unsigned pos = 4;
		for (int j = 0; j < 2; j++)
		{
			PatchVoice &voice = patch.voice[j];
			
			for (int op = 0; op < 2; op++)
			{
				// operator mode
				voice.op_mode[op] = bytes[pos++];
				// operator envelope
				voice.op_ad[op] = bytes[pos++];
				voice.op_sr[op] = bytes[pos++];
				// operator waveform
				voice.op_wave[op] = bytes[pos++];
				// KSR & output level
				voice.op_ksr[op] = bytes[pos++] & 0xc0;
				voice.op_level[op] = bytes[pos++] & 0x3f;
				
				// feedback/connection (first op only)
				if (op == 0)
					voice.conn = bytes[pos];
				pos++;
			}
			
			// midi note offset (int16, but only really need the LSB)
			voice.tune = (int8_t)bytes[pos];
			pos += 2;
		}
		
		// seek to patch name
		fseek(file, offset + (32*i) + (36*175) + 8, SEEK_SET);
		fread(bytes, 1, 32, file);
		bytes[31] = '\0';
		if (bytes[0])
			patch.name = (const char*)bytes;
		else
			patch.name = names[key];
		
	//	printf("Read patch: %s\n", bytes);
	}
	
	return true;
}
