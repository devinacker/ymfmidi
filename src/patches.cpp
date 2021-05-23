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
	    || loadOP2(file, patches, offset)
	    || loadAIL(file, patches, offset)
	    || loadTMB(file, patches, offset);
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadWOPL(FILE *file, OPLPatch (&patches)[256], int offset)
{
	uint8_t bytes[66] = {0};
	
	fseek(file, offset, SEEK_SET);
	if (fread(bytes, 1, 19, file) != 19)
		return false;
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
		if (fread(bytes, 1, instSize, file) != instSize)
			return false;
		
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
		patch.fourOp            = (bytes[39] & 3) == 1;
		patch.dualTwoOp         = (bytes[39] & 3) == 3;
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
	if (fread(bytes, 1, 8, file) != 8)
		return false;
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
		if (fread(bytes, 1, 36, file) != 36)
			return false;
		
		// read the common data for both 2op voices
		// flag bit 0 is "fixed pitch" (for drums), but it's seemingly only used for drum patches anyway, so ignore it?
		patch.dualTwoOp = (bytes[0] & 4);
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
		if (fread(bytes, 1, 32, file) == 32)
		{
			bytes[31] = '\0';
			if (bytes[0])
				patch.name = (const char*)bytes;
			else
				patch.name = names[key];
		}
		else
		{
			patch.name = names[key];
		}
	//	printf("Read patch: %s\n", bytes);
	}
	
	return true;
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadAIL(FILE *file, OPLPatch (&patches)[256], int offset)
{
	fseek(file, offset, SEEK_SET);
	
	while (true)
	{
		uint16_t key;
		uint8_t entry[6];
		if (fread(entry, 1, 6, file) != 6)
			return false;
		if (entry[0] == 0xff && entry[1] == 0xff)
			return true; // end of patches
		else if (entry[1] == 0)
			key = entry[0] & 0x7f;
		else if (entry[1] == 0x7f)
			key = entry[0] | 0x80;
		else
			continue; // additional melody banks currently not supported
		
		OPLPatch &patch = patches[key];
		// clear patch data
		patch = OPLPatch();
		patch.name = names[key];
		
		unsigned currentPos = ftell(file);
		uint32_t patchPos = entry[2] | (entry[3] << 8) | (entry[4] << 16) | (entry[5] << 24);
		fseek(file, patchPos + offset, SEEK_SET);
		
		uint8_t bytes[0x19];
		// try to read a 4op patch's worth of data, even if it is actually a 2op patch
		// and only fail if we read less than we needed to
		if (fread(bytes, 1, 0x19, file) < bytes[0])
			return false;
		
		if (bytes[0] == 0x0e)
			patch.fourOp = false;
		else if (bytes[0] == 0x19)
			patch.fourOp = true;
		else
			return false;
		fseek(file, currentPos, SEEK_SET);
		
		patch.voice[0].tune = patch.voice[1].tune = (int8_t)bytes[2] - 12;
		patch.voice[0].conn = bytes[8] & 0x0f;
		patch.voice[1].conn = bytes[8] >> 7;
		
		unsigned pos = 3;
		for (int i = 0; i < (patch.fourOp ? 2 : 1); i++)
		{
			PatchVoice &voice = patch.voice[i];
			
			for (int op = 0; op < 2; op++)
			{
				// operator mode
				voice.op_mode[op] = bytes[pos++];
				// KSR & output level
				voice.op_ksr[op] = bytes[pos] & 0xc0;
				voice.op_level[op] = bytes[pos++] & 0x3f;
				// operator envelope
				voice.op_ad[op] = bytes[pos++];
				voice.op_sr[op] = bytes[pos++];
				// operator waveform
				voice.op_wave[op] = bytes[pos++];
				
				// already handled the feedback/connection byte
				if (op == 0)
					pos++;
			}
		}
	}
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadTMB(FILE *file, OPLPatch (&patches)[256], int offset)
{
	fseek(file, offset, SEEK_SET);
	
	for (uint16_t key = 0; key < 256; key++)
	{
		OPLPatch &patch = patches[key];
		// clear patch data
		patch = OPLPatch();
		patch.name = names[key];
		
		uint8_t bytes[13];
		if (fread(bytes, 1, 13, file) != 13)
			return false;
		
		// since this format has no identifying info, we can only really reject it
		// if it has invalid values in a few spots
		if ((bytes[8] | bytes[9] | bytes[10]) & 0xf0)
			return false;
		
		PatchVoice &voice = patch.voice[0];
		voice.op_mode[0]  = bytes[0];
		voice.op_mode[1]  = bytes[1];
		voice.op_ksr[0]   = bytes[2] & 0xc0;
		voice.op_level[0] = bytes[2] & 0x3f;
		voice.op_ksr[1]   = bytes[3] & 0xc0;
		voice.op_level[1] = bytes[3] & 0x3f;
		voice.op_ad[0]    = bytes[4];
		voice.op_ad[1]    = bytes[5];
		voice.op_sr[0]    = bytes[6];
		voice.op_sr[1]    = bytes[7];
		voice.op_wave[0]  = bytes[8];
		voice.op_wave[1]  = bytes[9];
		voice.conn        = bytes[10];
		voice.tune        = (int8_t)bytes[11] - 12;
		patch.velocity    = (int8_t)bytes[12];
	}
	
	return true;
}
