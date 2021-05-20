#include "sequence_mid.h"

#include <cmath>
#include <cstring>

#define READ_U16BE(data, pos) ((data[pos] << 8) | data[pos+1])
#define READ_U24BE(data, pos) ((data[pos] << 16) | (data[pos+1] << 8) | data[pos+2])
#define READ_U32BE(data, pos) ((data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3])

class MIDTrack
{
public:
	MIDTrack(FILE *file, uint32_t size, SequenceMID* sequence);
	~MIDTrack();
	
	void reset();
	void advance(uint32_t time);
	uint32_t update(OPLPlayer& player);
	
	bool atEnd() const { return m_atEnd; }
	
private:
	uint32_t readVLQ();

	SequenceMID *m_sequence;
	uint8_t *m_data;
	uint32_t m_pos, m_size;
	int32_t m_delay;
	bool m_initDelay; // false if nothing has happened yet and we should just read the initial delay
	bool m_atEnd;
	uint8_t m_status; // for MIDI running status
};

// ----------------------------------------------------------------------------
MIDTrack::MIDTrack(FILE *file, uint32_t size, SequenceMID *sequence)
{
	m_data = new uint8_t[size];
	m_size = fread(m_data, 1, size, file);
	m_sequence = sequence;
	
	reset();
}

// ----------------------------------------------------------------------------
MIDTrack::~MIDTrack()
{
	delete[] m_data;
}

// ----------------------------------------------------------------------------
void MIDTrack::reset()
{
	m_pos = m_delay = 0;
	m_initDelay = true;
	m_atEnd = false;
	m_status = 0x00;
}

// ----------------------------------------------------------------------------
void MIDTrack::advance(uint32_t time)
{
	if (m_atEnd)
		return;
	
	m_delay -= time;
}

// ----------------------------------------------------------------------------
uint32_t MIDTrack::readVLQ()
{
	uint32_t vlq = 0;
	uint8_t data = 0;

	do
	{
		data = m_data[m_pos++];
		vlq <<= 7;
		vlq |= (data & 0x7f);
	} while ((data & 0x80) && (m_pos < m_size));
	
	return vlq;
}

// ----------------------------------------------------------------------------
uint32_t MIDTrack::update(OPLPlayer& player)
{
	if (m_initDelay)
	{
		m_initDelay = false;
		m_delay = readVLQ();
		return m_delay;
	}
	
	while (m_delay <= 0)
	{
		uint8_t data[2];
		uint8_t channel;
		int16_t pitch;
		uint32_t len;
		
		// make sure we have enough data left for one full event
		if (m_size - m_pos < 3)
		{
			m_atEnd = true;
			return UINT_MAX;
		}
		
		data[0] = m_data[m_pos++];
		if (data[0] & 0x80)
		{
			m_status = data[0];
			data[0] = m_data[m_pos++];
		}
		
		channel = m_status & 15;
		switch (m_status >> 4)
		{
		case 8: // note off
			player.midiNoteOff(channel, data[0]);
			// ignore velocity
			m_pos++;
			break;
		
		case 9: // note on
			player.midiNoteOn(channel, data[0], m_data[m_pos++]);
			break;
		
		case 10: // polyphonic pressure (ignored)
			m_pos++;
			break;
		
		case 11: // controller change
			player.midiControlChange(channel, data[0], m_data[m_pos++]);
			break;
		
		case 12: // program change
			player.midiProgramChange(channel, data[0]);
			break;
		
		case 13: // channel pressure (ignored)
			break;
		
		case 14: // pitch bend
			data[1] = m_data[m_pos++];
			pitch = (int16_t)(data[0] | (data[1] << 7)) - 8192;
			player.midiPitchControl(channel, pitch / 8192.0);
			break;
		
		case 15: // sysex / meta event
			if (m_status != 0xFF)
			{
				m_pos--;
				len = readVLQ();
				if (m_pos + len < m_size)
				{
					m_pos += len;
				}
				else
				{
					m_atEnd = true;
					return UINT_MAX;
				}
				break;
			}
			
			len = readVLQ();
			
			// end-of-track marker (or data just ran out)
			if (data[0] == 0x2F || (m_pos + len >= m_size))
			{
				m_atEnd = true;
				return UINT_MAX;
			}
			// tempo change
			if (data[0] == 0x51)
			{
				m_sequence->setUsecPerBeat(READ_U24BE(m_data, m_pos));
			}
			
			m_pos += len;
			break;
		}
		
		m_delay += readVLQ();
	}

	return m_delay;
}

// ----------------------------------------------------------------------------
SequenceMID::SequenceMID(FILE *file)
	: Sequence(file)
{
	uint8_t bytes[10] = {0};
	
	fseek(file, 4, SEEK_SET);
	fread(bytes, 1, 10, file);
	
	uint32_t len = READ_U32BE(bytes, 0);
	
	m_type = READ_U16BE(bytes, 4);
	uint16_t numTracks = READ_U16BE(bytes, 6);
	m_ticksPerBeat = READ_U16BE(bytes, 8);
	
	fseek(file, len + 8, SEEK_SET);
	
	for (unsigned i = 0; i < numTracks; i++)
	{
		memset(bytes, 0, 10);
		fread(bytes, 1, 8, file);
		
		if (memcmp(bytes, "MTrk", 4))
			break;
		
		len = READ_U32BE(bytes, 4);
		m_tracks.push_back(new MIDTrack(file, len, this));
	}
}

// ----------------------------------------------------------------------------
SequenceMID::~SequenceMID()
{
	for (auto track : m_tracks)
		delete track;
}

// ----------------------------------------------------------------------------
bool SequenceMID::isValid(FILE *file)
{
	uint8_t bytes[10] = {0};
	
	fseek(file, 0, SEEK_SET);
	fread(bytes, 1, 10, file);
	
	if (memcmp(bytes, "MThd", 4))
		return false;
	
	uint32_t len = READ_U32BE(bytes, 4);
	if (len < 6) return false;
	
	uint16_t type = READ_U16BE(bytes, 8);
	if (type > 2) return false;
	
	return true;
}

// ----------------------------------------------------------------------------
void SequenceMID::reset()
{
	setUsecPerBeat(500000);
	
	for (auto& track : m_tracks)
		track->reset();
}

// ----------------------------------------------------------------------------
void SequenceMID::setUsecPerBeat(uint32_t usec)
{
	double usecPerTick = (double)usec / m_ticksPerBeat;
	m_ticksPerSec = 1000000 / usecPerTick;
}

// ----------------------------------------------------------------------------
uint32_t SequenceMID::update(OPLPlayer& player)
{
	uint32_t tickDelay = UINT_MAX;
	
	bool tracksAtEnd = true;

	for (auto track : m_tracks)
	{
		tickDelay = std::min(tickDelay, track->update(player));
		tracksAtEnd &= track->atEnd();
	}
	
	if (tracksAtEnd)
	{
		reset();
		m_atEnd = true;
		return 0;
	}
	
	m_atEnd = false;
	
	for (auto track : m_tracks)
		track->advance(tickDelay);
	
	double samplesPerTick = player.sampleRate() / m_ticksPerSec;	
	return round(tickDelay * samplesPerTick);
}

