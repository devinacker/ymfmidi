#ifndef __PATCHES_H
#define __PATCHES_H

#include <string>

// one carrier/modulator pair in a patch, out of a possible two
struct PatchVoice
{
	uint8_t op_mode[2] = {0};  // regs 0x20+
	uint8_t op_ksr[2]   = {0}; // regs 0x40+ (upper bits)
	uint8_t op_level[2] = {0}; // regs 0x40+ (lower bits)
	uint8_t op_ad[2] = {0};    // regs 0x60+
	uint8_t op_sr[2] = {0};    // regs 0x80+
	uint8_t conn = 0;          // regs 0xC0+
	uint8_t op_wave[2] = {0};  // regs 0xE0+
	
	int8_t tune = 0; // MIDI note offset
	int16_t finetune = 0;
};

struct OPLPatch
{
	std::string name;
	bool fourOp = false;
	bool dualTwoOp = false; // only valid if fourOp = true
	uint8_t fixedNote = 0;
	
	PatchVoice voice[2];
	
	static bool load(const char *path, OPLPatch (&patches)[256]);

private:
	// individual format loaders
	static bool loadOP2(FILE *file, OPLPatch (&patches)[256]);
};

#endif // __PATCHES_H
