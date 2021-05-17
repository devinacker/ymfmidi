#include <cstdio>
#include <cstring>

#include "patches.h"

// ----------------------------------------------------------------------------
bool OPLPatch::load(const char *path, OPLPatch (&patches)[256])
{
	FILE *file = fopen(path, "rb");
	if (!file) return false;

	bool ok = loadOP2(file, patches);
	
	fclose(file);
	
	return ok;
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadOP2(FILE *file, OPLPatch (&patches)[256])
{
	uint8_t bytes[36] = {0};

	fseek(file, 0, SEEK_SET);
	fread(bytes, 1, 8, file);
	if (strncmp((const char*)bytes, "#OPL_II#", 8))
		return false;

	// read data for all patches (128 melodic + 47 percussion)
	for (int i = 0; i < 128+47; i++)
	{
		// patches 0-127 are melodic; the rest are for percussion notes 35 thru 81
		OPLPatch &patch = (i < 128) ? patches[i] : patches[i + 35];
		
		// clear patch data
		patch = OPLPatch();
		
		// seek to patch data
		fseek(file, (36*i) + 8, SEEK_SET);
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
		fseek(file, (32*i) + (36*175) + 8, SEEK_SET);
		fread(bytes, 1, 32, file);
		bytes[32] = '\0';
		patch.name = (const char*)bytes;
		
	//	printf("Read patch: %s\n", bytes);
	}
	
	return true;
}
