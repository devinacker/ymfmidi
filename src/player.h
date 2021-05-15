#ifndef __PLAYER_H
#define __PLAYER_H

#include <ymfm_opl.h>

#include "patches.h"

struct MIDIChannel
{
	uint8_t patchNum = 0;
	uint8_t volume = 127;
	uint8_t pan = 64;
	double pitch = 0.0;
};

struct OPLVoice
{
	const MIDIChannel *channel = nullptr;
	const PatchVoice *patchVoice = nullptr;
	
	uint8_t num = 0;
	uint16_t op = 0; // base operator number, set based on voice num.
	
	bool on = false;
	uint8_t note = 0;
	uint8_t velocity = 0;
	
	// tuning information from the currently playing patch
	int8_t tune = 0; // MIDI note offset
	int16_t finetune = 0; // TODO
	
	uint16_t freq = 0; // block and F number, calculated from note and channel pitch
	
	uint32_t duration = UINT_MAX; // how many samples have been output since this note
};

class OPLPlayer : public ymfm::ymfm_interface
{
public:
	static const unsigned masterClock = 14320000;

	enum {
		REG_TEST        = 0x01,
	
		REG_OP_MODE     = 0x20,
		REG_OP_LEVEL    = 0x40,
		REG_OP_AD       = 0x60,
		REG_OP_SR       = 0x80,
		REG_VOICE_FREQL = 0xA0,
		REG_VOICE_FREQH = 0xB0,
		REG_VOICE_CNT   = 0xC0,
		REG_OP_WAVEFORM = 0xE0,
		
		REG_NEW         = 0x105,
	};

	OPLPlayer();
	virtual ~OPLPlayer();
	
	void setSampleRate(uint32_t rate);
	bool loadPatches(const char* path);
	
	void generate(float *data, unsigned numSamples);
	
	// misc. informational stuff
	const std::string& patchName(uint8_t num) { return m_patches[num].name; }
	
	// reset OPL (and midi file?)
	void reset();
	
	// MIDI events, called by the file format handler
	void midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
	void midiNoteOff(uint8_t channel, uint8_t note);
	void midiPitchControl(uint8_t channel, double pitch);
	void midiProgramChange(uint8_t channel, uint8_t patchNum);
	void midiControlChange(uint8_t channel, uint8_t control, uint8_t value);
	
private:
	void write(uint16_t addr, uint8_t data);
	
	// find a voice with the oldest note
	OPLVoice* findVoice();
	// find a voice that's playing a specific note on a specific channel
	OPLVoice* findVoice(uint8_t channel, uint8_t note);

	// update a property of all currently playing voices on a MIDI channel
	void updateChannelVoices(uint8_t channel, void(OPLPlayer::*func)(OPLVoice&));

	// update the patch parameters for a voice
	void updatePatch(OPLVoice& voice);

	// update the volume level for a voice
	void updateVolume(OPLVoice& voice);

	// update the pan position for a voice
	void updatePanning(OPLVoice& voice);

	// update the block and F-number for a voice (also key on/off)
	void updateFrequency(OPLVoice& voice);

	ymfm::ymf262 *m_opl3;
	double m_sample_step; // ratio of OPL sample rate to output sample rate (usually < 1.0)
	double m_sample_pos; // number of pending output samples (when >= 1.0, output one)
	
	MIDIChannel m_channels[16];
	OPLVoice m_voices[18];
	
	OPLPatch m_patches[256];
};

#endif // __PLAYER_H
