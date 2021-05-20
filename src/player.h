#ifndef __PLAYER_H
#define __PLAYER_H

#include <ymfm_opl.h>
#include <climits>
#include <vector>

#include "patches.h"

class Sequence;

struct MIDIChannel
{
	uint8_t num = 0;

	uint8_t patchNum = 0;
	uint8_t volume = 127;
	uint8_t pan = 64;
	double pitch = 0.0;
};

struct OPLVoice
{
	ymfm::ymf262 *chip = nullptr;
	const MIDIChannel *channel = nullptr;
	const OPLPatch *patch = nullptr;
	const PatchVoice *patchVoice = nullptr;
	
	uint16_t num = 0;
	uint16_t op = 0; // base operator number, set based on voice num.
	
	bool on = false;
	bool justChanged = false; // true after note on/off, false after generating at least 1 sample
	uint8_t note = 0;
	uint8_t velocity = 0;
	
	// block and F number, calculated from note and channel pitch
	uint16_t freq = 0;
	
	// how long has this note been playing (incremented each midi update)
	uint32_t duration = UINT_MAX;
};

class OPLPlayer : public ymfm::ymfm_interface
{
public:
	OPLPlayer(int numChips = 1);
	virtual ~OPLPlayer();
	
	void setLoop(bool loop) { m_looping = loop; }
	void setSampleRate(uint32_t rate);
	void setGain(double gain);
	
	bool loadSequence(const char* path, int offset = 0);
	bool loadSequence(FILE *file, int offset = 0);
	
	bool loadPatches(const char* path, int offset = 0);
	bool loadPatches(FILE *file, int offset = 0);
	
	void generate(float *data, unsigned numSamples);
	void generate(int16_t *data, unsigned numSamples);
	
	// reset OPL and midi file
	void reset();
	// reached end of song?
	bool atEnd() const;
	
	// debug
	void displayClear();
	void displayChannels();
	void displayVoices();
	
	// misc. informational stuff
	uint32_t sampleRate() const { return m_sampleRate; }
	const std::string& patchName(uint8_t num) { return m_patches[num].name; }
	
	// MIDI events, called by the file format handler
	void midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
	void midiNoteOff(uint8_t channel, uint8_t note);
	void midiPitchControl(uint8_t channel, double pitch);
	void midiProgramChange(uint8_t channel, uint8_t patchNum);
	void midiControlChange(uint8_t channel, uint8_t control, uint8_t value);
	
private:
	static const unsigned masterClock = 14318181;

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

	void updateMIDI();

	void write(ymfm::ymf262* chip, uint16_t addr, uint8_t data);
	
	// find a voice with the oldest note
	OPLVoice* findVoice();
	// find a voice that's playing a specific note on a specific channel
	OPLVoice* findVoice(uint8_t channel, uint8_t note, bool on);

	// find the patch to use for a specific MIDI channel and note
	const OPLPatch* findPatch(uint8_t channel, uint8_t note) const;

	// update a property of all currently playing voices on a MIDI channel
	void updateChannelVoices(uint8_t channel, void(OPLPlayer::*func)(OPLVoice&));

	// update the patch parameters for a voice
	void updatePatch(OPLVoice& voice, const OPLPatch *newPatch, uint8_t numVoice = 0);

	// update the volume level for a voice
	void updateVolume(OPLVoice& voice);

	// update the pan position for a voice
	void updatePanning(OPLVoice& voice);

	// update the block and F-number for a voice (also key on/off)
	void updateFrequency(OPLVoice& voice);

	std::vector<ymfm::ymf262*> m_opl3;
	unsigned m_numChips;
	uint32_t m_sampleRate; // output sample rate (default 44.1k)
	double m_sampleGain;
	double m_sampleScale; // convert 16-bit samples to float (includes gain value)
	double m_sampleStep; // ratio of OPL sample rate to output sample rate (usually < 1.0)
	double m_samplePos; // number of pending output samples (when >= 1.0, output one)
	uint32_t m_samplesLeft; // remaining samples until next midi event
	bool m_looping;
	
	MIDIChannel m_channels[16];
	std::vector<OPLVoice> m_voices;
	
	Sequence *m_sequence;
	OPLPatch m_patches[256];
};

#endif // __PLAYER_H
