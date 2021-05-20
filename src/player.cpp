#include "player.h"
#include "sequence.h"

static const unsigned voice_num[18] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
	0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108
};

static const unsigned oper_num[18] = {
	0x0, 0x1, 0x2, 0x8, 0x9, 0xA, 0x10, 0x11, 0x12,
	0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112
};

// ----------------------------------------------------------------------------
OPLPlayer::OPLPlayer(int numChips)
	: ymfm::ymfm_interface()
{
	m_numChips = numChips;
	m_opl3.resize(numChips);
	for (auto& opl : m_opl3)
		opl = new ymfm::ymf262(*this);
		
	m_voices.resize(numChips * 18);
	m_sequence = nullptr;
	
	m_samplePos = 0.0;
	m_samplesLeft = 0;
	setSampleRate(44100);
	setGain(1.0);
	
	reset();
}

// ----------------------------------------------------------------------------
OPLPlayer::~OPLPlayer()
{
	for (auto& opl : m_opl3)
		delete opl;
	delete m_sequence;
}

// ----------------------------------------------------------------------------
void OPLPlayer::setSampleRate(uint32_t rate)
{
	uint32_t rateOPL = m_opl3[0]->sample_rate(masterClock);
	m_sampleStep = (double)rate / rateOPL;
	m_sampleRate = rate;
	
//	printf("OPL sample rate = %u / output sample rate = %u / step %02f\n", rateOPL, rate, m_sampleStep);
}

// ----------------------------------------------------------------------------
void OPLPlayer::setGain(double gain)
{
	m_sampleScale = 32768.0 / gain;
}

// ----------------------------------------------------------------------------
bool OPLPlayer::loadSequence(const char* path)
{
	delete m_sequence;
	m_sequence = Sequence::load(path);
	
	return m_sequence != nullptr;
}

// ----------------------------------------------------------------------------
bool OPLPlayer::loadPatches(const char* path)
{
	return OPLPatch::load(path, m_patches);
}

// ----------------------------------------------------------------------------
void OPLPlayer::generate(float *data, unsigned numSamples)
{
	ymfm::ymf262::output_data output[m_numChips];

	while (numSamples)
	{
		while (!m_samplesLeft && m_sequence)
		{	
			// time to update midi playback
			m_samplesLeft = m_sequence->update(*this);
			for (auto& voice : m_voices)
			{
				voice.duration++;
				voice.justChanged = false;
			}
		}
	
		while (m_samplePos < 1.0)
		{
			for (unsigned i = 0; i < m_numChips; i++)
				m_opl3[i]->generate(&output[i]);
			m_samplePos += m_sampleStep;
		}
		
		while (m_samplePos >= 1.0 && numSamples > 0)
		{
			for (unsigned i = 0; i < m_numChips; i++)
			{
				data[0] += output[i].data[0] / m_sampleScale;
				data[1] += output[i].data[1] / m_sampleScale;
			}
			data += 2;
			numSamples--;
			m_samplePos -= 1.0;
			if (m_samplesLeft)
				m_samplesLeft--;
		}
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::displayClear()
{
	for (int i = 0; i < 18; i++)
		printf("%79s\n", "");
}

// ----------------------------------------------------------------------------
void OPLPlayer::displayChannels()
{
	unsigned numVoices[16] = {0};
	unsigned totalVoices = 0;
	for (auto& voice : m_voices)
	{
		if (voice.channel && (voice.on || voice.justChanged))
		{
			numVoices[voice.channel->num]++;
			totalVoices++;
		}
	}
	
	printf("Chn | Patch Name                       | Vol | Pan | Active Voices: %u/%-6llu\n", totalVoices, m_voices.size());
	printf("----+----------------------------------+-----+-----+---------------------------\n");
	for (int i = 0; i < 16; i++)
	{
		const auto& channel = m_channels[i];
	
		printf("%3u | %-32s | %3u | %3u | ", i + 1, 
			(i == 9) ? "Percussion" : findPatch(i, 0)->name.c_str(),
			channel.volume, channel.pan);
		
		if (m_voices.size() < 100)
		{
			printf("%2u ", numVoices[i]);
			for (int j = 0; j < 23; j++)
				printf("%c", j < numVoices[i] ? '*' : ' ');
		}
		else
		{
			printf("%3u ", numVoices[i]);
			for (int j = 0; j < 22; j++)
				printf("%c", j < numVoices[i] ? '*' : ' ');
		}
		printf("\n");
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::displayVoices()
{
	for (int i = 0; i < 18; i++)
	{
		if (m_numChips == 1)
		{
			printf("voice %2u: ", i + 1);
			if (m_voices[i].channel)
			{
				printf("channel %2u, note %3u %c %-32s",
					m_voices[i].channel->num + 1, m_voices[i].note,
					m_voices[i].on ? '*' : ' ',
					m_voices[i].patch ? m_voices[i].patch->name.c_str() : "");
			}
			else
			{
				printf("%69s", "");
			}
		}
		else if (m_numChips == 2)
		{
			for (int j = i; j < m_voices.size(); j += 18)
			{
				printf("voice %2u: ", j + 1);
				if (m_voices[j].channel)
				{
					printf("channel %2u, note %3u %c",
						m_voices[j].channel->num + 1, m_voices[j].note,
						m_voices[j].on ? '*' : ' ');
				}
				else
				{
					printf("%22s", "");
				}
				
				if (j < 18)
					printf("        | ");
			}
		}
		else if (m_numChips <= 4)
		{
			for (int j = i; j < m_voices.size(); j += 18)
			{
				printf("%2u: ", j + 1);
				if (m_voices[j].channel)
				{
					printf("channel %2u %c",
						m_voices[j].channel->num + 1,
						m_voices[j].on ? '*' : ' ');
				}
				else
				{
					printf("%12s", "");
				}
				
				if (j < m_voices.size() - 18)
					printf(" | ");
			}
		}
		else if (m_numChips <= 8)
		{
			for (int j = i; j < m_voices.size(); j += 18)
			{
				printf("%3u: %c ", j + 1, m_voices[j].on ? '*' : ' ');
				
				if (j < m_voices.size() - 18)
					printf(" | ");
			}
		}
		
		printf("\n");
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::reset()
{
	for (auto& opl : m_opl3)
	{
		opl->reset();
		// enable OPL3 stuff
		write(opl, REG_TEST, 1 << 5);
		write(opl, REG_NEW, 1);
	}
		
	// reset MIDI channel and OPL voice status
	for (int i = 0; i < 16; i++)
	{
		m_channels[i] = MIDIChannel();
		m_channels[i].num = i;
	}
	for (int i = 0; i < m_voices.size(); i++)
	{
		m_voices[i] = OPLVoice();
	//	m_voices[i].channel = m_channels;
		m_voices[i].chip = m_opl3[i / 18];
		m_voices[i].num = voice_num[i % 18];
		m_voices[i].op = oper_num[i % 18];
	}
	
	if (m_sequence)
		m_sequence->reset();
	m_samplesLeft = 0;
}

// ----------------------------------------------------------------------------
void OPLPlayer::write(ymfm::ymf262* chip, uint16_t addr, uint8_t data)
{
	if (addr < 0x100)
		chip->write_address((uint8_t)addr);
	else
		chip->write_address_hi((uint8_t)addr);
	chip->write_data(data);
}

// ----------------------------------------------------------------------------
OPLVoice* OPLPlayer::findVoice()
{
	OPLVoice *found = nullptr;
	uint32_t duration = 0;
	
	// try to find the "oldest" voice, prioritizing released notes
	// (or voices that haven't ever been used yet)
	for (auto& voice : m_voices)
	{
		if (!voice.channel)
			return &voice;
	
		if (!voice.on && !voice.justChanged
			&& voice.duration > duration)
		{
			found = &voice;
			duration = voice.duration;
		}
	}
	
	if (found) return found;
	// if we didn't find one yet, just try to find an old one
	// even if it should still be playing.
	
	for (auto& voice : m_voices)
	{
		if (voice.duration > duration)
		{
			found = &voice;
			duration = voice.duration;
		}
	}
	
	return found;
}

// ----------------------------------------------------------------------------
OPLVoice* OPLPlayer::findVoice(uint8_t channel, uint8_t note, bool on)
{
	channel &= 15;
	for (auto& voice : m_voices)
	{
		if (voice.on == on && !voice.justChanged
			&& voice.channel == &m_channels[channel]
			&& voice.note == note)
		{
			return &voice;
		}
	}
	
	return nullptr;
}

// ----------------------------------------------------------------------------
const OPLPatch* OPLPlayer::findPatch(uint8_t channel, uint8_t note) const
{
	if (channel == 9)
	{
		return &m_patches[128 | note];
	}
	else
	{
		return &m_patches[m_channels[channel & 15].patchNum & 0x7f];
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateChannelVoices(uint8_t channel, void(OPLPlayer::*func)(OPLVoice&))
{
	for (auto& voice : m_voices)
	{
		if (voice.channel == &m_channels[channel & 15])
			(this->*func)(voice);
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::updatePatch(OPLVoice& voice, const OPLPatch *newPatch, uint8_t numVoice)
{	
	// assign the MIDI channel's current patch (or the current drum patch) to this voice

	const PatchVoice& patchVoice = newPatch->voice[numVoice];
	
	if (voice.patchVoice != &patchVoice)
	{
		voice.patch = newPatch;
		voice.patchVoice = &patchVoice;
		
		// 0x20: vibrato, sustain, multiplier
		write(voice.chip, REG_OP_MODE + voice.op,     patchVoice.op_mode[0]);
		write(voice.chip, REG_OP_MODE + voice.op + 3, patchVoice.op_mode[1]);
		// 0x60: attack/decay
		write(voice.chip, REG_OP_AD + voice.op,     patchVoice.op_ad[0]);
		write(voice.chip, REG_OP_AD + voice.op + 3, patchVoice.op_ad[1]);
		// 0x80: sustain/release
		write(voice.chip, REG_OP_SR + voice.op,     patchVoice.op_sr[0]);
		write(voice.chip, REG_OP_SR + voice.op + 3, patchVoice.op_sr[1]);
		// 0xe0: waveform
		write(voice.chip, REG_OP_WAVEFORM + voice.op,     patchVoice.op_wave[0]);
		write(voice.chip, REG_OP_WAVEFORM + voice.op + 3, patchVoice.op_wave[1]);
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateVolume(OPLVoice& voice)
{
	// lookup table shamelessly stolen from Nuke.YKT
	static const uint8_t opl_volume_map[32] =
	{
		80, 63, 40, 36, 32, 28, 23, 21,
		19, 17, 15, 14, 13, 12, 11, 10,
		 9,  8,  7,  6,  5,  5,  4,  4,
		 3,  3,  2,  2,  1,  1,  0,  0
	};

	uint8_t atten = opl_volume_map[(voice.velocity * voice.channel->volume) >> 9];
	uint8_t level;
	
	auto patchVoice = voice.patchVoice;
	
	// 0x40: key scale / volume
	if (patchVoice->conn & 1)
		level = std::min(0x3f, patchVoice->op_level[0] + atten);
	else
		level = patchVoice->op_level[0];
	write(voice.chip, REG_OP_LEVEL + voice.op,     level | patchVoice->op_ksr[0]);
		
	level = std::min(0x3f, patchVoice->op_level[1] + atten);
	write(voice.chip, REG_OP_LEVEL + voice.op + 3, level | patchVoice->op_ksr[1]);
}

// ----------------------------------------------------------------------------
void OPLPlayer::updatePanning(OPLVoice& voice)
{
	// 0xc0: output/feedback/mode
	uint8_t pan = 0x30;
	if (voice.channel->pan < 32)
		pan = 0x10;
	else if (voice.channel->pan >= 96)
		pan = 0x20;
	
	write(voice.chip, REG_VOICE_CNT + voice.num, voice.patchVoice->conn | pan);
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateFrequency(OPLVoice& voice)
{
	static const uint16_t noteFreq[12] = {
		// calculated from A440
		345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651
	};
	static const double noteBendUp   = 0.1224620;  // ~2 semitones
	static const double noteBendDown = 0.1091013;
	
	if (!voice.patch || !voice.channel) return;
	
	uint8_t note = ((voice.channel->num != 9) ? voice.note : voice.patch->fixedNote);
	note += voice.patchVoice->tune;
	uint8_t octave = (note / 12) & 7;
	note %= 12;
	
	voice.freq = noteFreq[note];
	const double detune = (voice.channel->pitch + voice.patchVoice->finetune);
	if (detune > 0)
		voice.freq += voice.freq * noteBendUp * detune;
	else if (detune < 0)
		voice.freq += voice.freq * noteBendDown * detune;
	voice.freq += (octave << 10);
	
//	printf("voice.freq: %u\n", voice.freq);
	write(voice.chip, REG_VOICE_FREQL + voice.num, voice.freq & 0xff);
	write(voice.chip, REG_VOICE_FREQH + voice.num, (voice.freq >> 8) | (voice.on ? (1 << 5) : 0));
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
	if (!velocity)
	{
		midiNoteOff(channel, note);
		return;
	}

	note &= 0x7f;
	velocity &= 0x7f;

//	printf("midiNoteOn: chn %u, note %u\n", channel, note);
	const OPLPatch *newPatch = findPatch(channel, note);
	if (!newPatch) return;
	
	const int numVoices = (newPatch->fourOp ? 2 : 1);

	for (int i = 0; i < numVoices; i++)
	{
		OPLVoice *voice = findVoice(channel, note, false);
		if (!voice) voice = findVoice();
		if (!voice) continue; // ??
		
		// update the note parameters for this voice
		voice->channel = &m_channels[channel & 15];
		voice->on = voice->justChanged = true;
		voice->note = note;
		voice->velocity = ymfm::clamp((int)velocity + newPatch->velocity, 0, 127);
		// set the second voice's duration to 1 so it can get dropped if we need it to
		voice->duration = i;
		
		updatePatch(*voice, newPatch, i);
		updateVolume(*voice);
		updatePanning(*voice);
		updateFrequency(*voice);
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiNoteOff(uint8_t channel, uint8_t note)
{
	note &= 0x7f;
	
//	printf("midiNoteOff: chn %u, note %u\n", channel, note);
	OPLVoice *voice;
	while ((voice = findVoice(channel, note, true)) != nullptr)
	{
		voice->justChanged = voice->on;
		voice->on = false;

		write(voice->chip, REG_VOICE_FREQH + voice->num, voice->freq >> 8);
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiPitchControl(uint8_t channel, double pitch)
{
//	printf("midiPitchControl: chn %u, val %.02f\n", channel, pitch);
	m_channels[channel & 15].pitch = pitch;
	updateChannelVoices(channel, &OPLPlayer::updateFrequency);
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiProgramChange(uint8_t channel, uint8_t patchNum)
{
	m_channels[channel & 15].patchNum = patchNum & 0x7f;
	// patch change will take effect on the next note for this channel
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
	channel &= 15;
	control &= 0x7f;
	value   &= 0x7f;
	
//	printf("midiControlChange: chn %u, ctrl %u, val %u\n", channel, control, value);
	switch (control)
	{
	case 7:
		m_channels[channel].volume = value;
		updateChannelVoices(channel, &OPLPlayer::updateVolume);
		break;
	
	case 10:
		m_channels[channel].pan = value;
		updateChannelVoices(channel, &OPLPlayer::updatePanning);
		break;
	}
}
