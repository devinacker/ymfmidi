#include "player.h"

static const unsigned voice_num[18] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
	0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108
};

static const unsigned oper_num[18] = {
	0x0, 0x1, 0x2, 0x8, 0x9, 0xA, 0x10, 0x11, 0x12,
	0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112
};

// ----------------------------------------------------------------------------
OPLPlayer::OPLPlayer()
	: ymfm::ymfm_interface()
{
	m_opl3 = new ymfm::ymf262(*this);
	
	m_sample_step = 1.0;
	m_sample_pos = 0.0;
		
	reset();
}

// ----------------------------------------------------------------------------
OPLPlayer::~OPLPlayer()
{
	delete m_opl3;
}

// ----------------------------------------------------------------------------
void OPLPlayer::setSampleRate(uint32_t rate)
{
	uint32_t rateOPL = m_opl3->sample_rate(masterClock);
	m_sample_step = (double)rate / rateOPL;
	
	printf("OPL sample rate = %u / output sample rate = %u / step %02f\n", rateOPL, rate, m_sample_step);
}

// ----------------------------------------------------------------------------
void OPLPlayer::generate(float *data, unsigned numSamples)
{
	ymfm::ymf262::output_data output;

	while (numSamples)
	{
		for (; m_sample_pos < 1.0; m_sample_pos += m_sample_step)
			m_opl3->generate(&output);
		
		// TODO: update MIDI playback here if needed
		// just update voice duration for now
		for (int i = 0; i < 18; i++)
		{
			if (m_voices[i].duration < UINT_MAX)
				m_voices[i].duration++;
		}
		
		*data++ = output.data[0] / 32768.0;
		*data++ = output.data[1] / 32768.0;
		
		numSamples--;
		m_sample_pos -= 1.0;
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::reset()
{
	m_opl3->reset();
	
	// enable OPL3 stuff
	write(REG_TEST, 1 << 5);
	write(REG_NEW, 1);
	
	// reset MIDI channel and OPL voice status
	for (int i = 0; i < 16; i++)
	{
		m_channels[i] = MIDIChannel();
	}
	for (int i = 0; i < 18; i++)
	{
		m_voices[i] = OPLVoice();
		m_voices[i].num = voice_num[i];
		m_voices[i].op = oper_num[i];
	}
	
	// make up some dumb temporary patch for now
	// (stolen from https://github.com/DhrBaksteen/ArduinoOPL2/blob/master/indepth.md)
	for (int i = 0; i < 18; i++)
	{
		OPLVoice &voice = m_voices[i];
		
		// 0x20: vibrato, sustain, multiplier
		write(REG_OP_MODE + voice.op,     0x61);
		write(REG_OP_MODE + voice.op + 3, 0x21);
		// 0x40: volume
		write(REG_OP_LEVEL + voice.op,     0x16);
		write(REG_OP_LEVEL + voice.op + 3, 0x05);
		// 0x40: attack/decay
		write(REG_OP_AD + voice.op,     0x72);
		write(REG_OP_AD + voice.op + 3, 0x7f);
		// 0x60: sustain/release
		write(REG_OP_SR + voice.op,     0x22);
		write(REG_OP_SR + voice.op + 3, 0x04);
		// 0xc0: output/feedback/mode
	//	write(REG_VOICE_CNT + voice.num, 0x3c);
		// do some temp panning bullshit
		// (this will be handled by the actual MIDI pan control later...)
		switch (i % 3)
		{
		case 0:
			write(REG_VOICE_CNT + voice.num, 0x1c);
			break;
			
		case 1:
			write(REG_VOICE_CNT + voice.num, 0x3c);
			break;
			
		case 2:
			write(REG_VOICE_CNT + voice.num, 0x2c);
			break;
		}
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::write(uint16_t addr, uint8_t data)
{
	if (addr < 0x100)
		m_opl3->write_address((uint8_t)addr);
	else
		m_opl3->write_address_hi((uint8_t)addr);
	m_opl3->write_data(data);
}

// ----------------------------------------------------------------------------
OPLVoice* OPLPlayer::findVoice()
{
	OPLVoice *voice = m_voices;
	uint32_t duration = 0;
	int n = 0;
	
	for (int i = 0; i < 18; i++)
	{
		if (m_voices[i].duration > duration)
		{
			n = i;
			voice = &m_voices[i];
			duration = m_voices[i].duration;
		}
	}
	
//	printf("findVoice: %u\n", n);
	return voice;
}

// ----------------------------------------------------------------------------
OPLVoice* OPLPlayer::findVoice(uint8_t channel, uint8_t note)
{
	channel &= 15;
	for (int i = 0; i < 18; i++)
	{
		if (m_voices[i].channel == &m_channels[channel]
		    && m_voices[i].note == note)
		{
		//	printf("findVoice: %u\n", i);
			return &m_voices[i];
		}
	}
	
	return nullptr;
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateFrequency(OPLVoice* voice)
{
	static const uint16_t noteFreq[12] = {
		0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287, 0x2ae
	};
	static const uint16_t noteBendRange = noteFreq[0] / 6; // ~2 semitones
	
	uint8_t octave = (voice->note / 12) & 7;
	uint8_t note = voice->note % 12;
	
	voice->freq = noteFreq[note] + (voice->channel->pitch * noteBendRange) + (octave << 10);
	
//	printf("voice->freq: %u\n", voice->freq);
	write(REG_VOICE_FREQL + voice->num, voice->freq & 0xff);
	write(REG_VOICE_FREQH + voice->num, (voice->freq >> 8) | (voice->on ? (1 << 5) : 0));
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
	if (!velocity)
	{
		midiNoteOff(channel, note);
		return;
	}

//	printf("midiNoteOn: chn %u, note %u\n", channel, note);
	OPLVoice *voice = findVoice(channel, note);
	if (!voice) voice = findVoice();
	
	write(REG_VOICE_FREQH + voice->num, voice->freq >> 8);
	
	voice->channel = &m_channels[channel & 15];
	voice->on = true;
	voice->note = note;
	voice->velocity = velocity;
	voice->duration = 0;
	
	updateFrequency(voice);
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiNoteOff(uint8_t channel, uint8_t note)
{
//	printf("midiNoteOff: chn %u, note %u\n", channel, note);
	OPLVoice *voice = findVoice(channel, note);
	if (!voice) return;
	
	voice->on = false;
	
	write(REG_VOICE_FREQH + voice->num, voice->freq >> 8);
}
