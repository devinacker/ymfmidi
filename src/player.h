#include <ymfm_opl.h>

struct MIDIChannel
{
	uint8_t volume = 127;
	double pitch = 0.0;
};

struct OPLVoice
{
	MIDIChannel *channel = nullptr;
	
	uint8_t num = 0;
	uint8_t op = 0; // base operator number, set based on voice num.
	
	bool on = false;
	uint8_t note = 0;
	uint8_t velocity = 0;
	
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
	
	void generate(float *data, unsigned numSamples);
	
	// reset OPL (and midi file?)
	void reset();
	
	// MIDI events, called by the file format handler
	void midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
	void midiNoteOff(uint8_t channel, uint8_t note);
	
private:
	void write(uint16_t addr, uint8_t data);
	
	// find a voice with the oldest note
	OPLVoice* findVoice();
	// find a voice that's playing a specific note on a specific channel
	OPLVoice* findVoice(uint8_t channel, uint8_t note);

	// update the block and F-number for a voice
	void updateFrequency(OPLVoice* voice);

	ymfm::ymf262 *m_opl3;
	double m_sample_step; // ratio of OPL sample rate to output sample rate (usually < 1.0)
	double m_sample_pos; // number of pending output samples (when >= 1.0, output one)
	
	MIDIChannel m_channels[16];
	OPLVoice m_voices[18];
};
