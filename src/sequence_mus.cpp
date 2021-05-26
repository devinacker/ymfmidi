#include "sequence_mus.h"

#include <cmath>
#include <cstring>

// ----------------------------------------------------------------------------
SequenceMUS::SequenceMUS(FILE *file, int offset)
	: Sequence(file)
{
	memset(m_data, 0xFF, sizeof(m_data));
	uint8_t header[4] = {0};
	
	fseek(file, offset + 4, SEEK_SET);
	if (fread(header, 1, 4, file) == 4)
	{
		uint16_t length = header[0] | (header[1] << 8);
		uint16_t pos    = header[2] | (header[3] << 8);
		
		fseek(file, offset + pos, SEEK_SET);
		(void)fread(m_data, 1, length, file);
	}
	setDefaults();
}

// ----------------------------------------------------------------------------
bool SequenceMUS::isValid(FILE *file, int offset)
{
	uint8_t bytes[4];
	fseek(file, offset, SEEK_SET);
	if (fread(bytes, 1, 4, file) != 4)
		return false;
	return !memcmp(bytes, "MUS\x1a", 4);
}

// ----------------------------------------------------------------------------
void SequenceMUS::reset()
{
	Sequence::reset();
	setDefaults();
}

// ----------------------------------------------------------------------------
void SequenceMUS::setDefaults()
{
	m_pos = 0;
	memset(m_lastVol, 0x7f, sizeof(m_lastVol));
}

// ----------------------------------------------------------------------------
uint32_t SequenceMUS::update(OPLPlayer& player)
{
	uint8_t event, channel, data, param;
	
	m_atEnd = false;
	
	do
	{
		event = m_data[m_pos++];
		channel = event & 0xf;
		
		// map MUS channels to MIDI channels
		// (don't bother with the primary/secondary channel thing unless we need to)
		if (channel == 15) // percussion
			channel = 9;
		else if (channel >= 9)
			channel++;
		
		switch ((event >> 4) & 0x7)
		{
		case 0: // note off
			player.midiNoteOff(channel, m_data[m_pos++]);
			break;
			
		case 1: // note on
			data = m_data[m_pos++];
			if (data & 0x80)
				m_lastVol[channel] = m_data[m_pos++];
			player.midiNoteOn(channel, data, m_lastVol[channel]);
			break;
		
		case 2: // pitch bend
			player.midiPitchControl(channel, (m_data[m_pos++] / 128.0) - 1.0);
			break;
			
		case 3: // system event (channel mode messages)
			data = m_data[m_pos++] & 0x7f;
			switch (data)
			{
			case 10: player.midiControlChange(channel, 120, 0); break; // all sounds off
			case 11: player.midiControlChange(channel, 123, 0); break; // all notes off
			case 12: player.midiControlChange(channel, 126, 0); break; // mono on
			case 13: player.midiControlChange(channel, 127, 0); break; // poly on
			case 14: player.midiControlChange(channel, 121, 0); break; // reset all controllers
			default: break;
			}
			break;
		
		case 4: // controller
			data  = m_data[m_pos++] & 0x7f;
			param = m_data[m_pos++];
			switch (data)
			{
			case 0: player.midiProgramChange(channel, param); break;
			case 1: player.midiControlChange(channel, 0,  param); break; // bank select
			case 2: player.midiControlChange(channel, 1,  param); break; // mod wheel
			case 3: player.midiControlChange(channel, 7,  param); break; // volume
			case 4: player.midiControlChange(channel, 10, param); break; // pan
			case 5: player.midiControlChange(channel, 11, param); break; // expression
			case 6: player.midiControlChange(channel, 91, param); break; // reverb
			case 7: player.midiControlChange(channel, 93, param); break; // chorus
			case 8: player.midiControlChange(channel, 64, param); break; // sustain pedal
			case 9: player.midiControlChange(channel, 67, param); break; // soft pedal
			default: break;
			}
			break;
		
		case 5: // end of measure
			break;
		
		case 6: // end of track
			reset();
			m_atEnd = true;
			return 0;
		
		case 7: // unused
			m_pos++;
			break;
		}
	
	} while (!(event & 0x80));
	
	// read delay in ticks, convert to # of samples
	uint32_t tickDelay = 0;
	do
	{
		event = m_data[m_pos++];
		tickDelay <<= 7;
		tickDelay |= (event & 0x7f);
	} while (event & 0x80);
	
	double samplesPerTick = player.sampleRate() / 140.0;
	return round(tickDelay * samplesPerTick);
}
