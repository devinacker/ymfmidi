#ifndef __PATCHES_H
#define __PATCHES_H

#include <stddef.h>
#include <string>
#include <unordered_map>
#include <cstdint>

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
	double finetune = 1.0; // frequency multiplier
};

typedef std::unordered_map<uint16_t, struct OPLPatch> OPLPatchSet;

struct OPLPatch
{
	std::string name;
	bool fourOp = false; // true 4op
	bool dualTwoOp = false; // only valid if fourOp = false
	uint8_t fixedNote = 0;
	int8_t velocity = 0; // MIDI velocity offset
	
	PatchVoice voice[2];
	
	// default names
	static const char* names[256];
	
	static bool load(OPLPatchSet& patches, const char *path);
	static bool load(OPLPatchSet& patches, FILE *file, int offset = 0, size_t size = 0);
	static bool load(OPLPatchSet& patches, const uint8_t *data, size_t size);

private:
	// individual format loaders
	static bool loadWOPL(OPLPatchSet& patches, const uint8_t *data, size_t size);
	static bool loadOP2(OPLPatchSet& patches, const uint8_t *data, size_t size);
	static bool loadAIL(OPLPatchSet& patches, const uint8_t *data, size_t size);
	static bool loadTMB(OPLPatchSet& patches, const uint8_t *data, size_t size);
};

#endif // __PATCHES_H
