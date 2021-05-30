#include "sequence_mid.h"

#include <cmath>
#include <cstring>

#define READ_U16BE(data, pos) ((data[pos] << 8) | data[pos+1])
#define READ_U24BE(data, pos) ((data[pos] << 16) | (data[pos+1] << 8) | data[pos+2])
#define READ_U32BE(data, pos) ((data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3])

class MIDTrack
{
public:
	MIDTrack(const uint8_t *data, size_t size, SequenceMID* sequence);
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
MIDTrack::MIDTrack(const uint8_t *data, size_t size, SequenceMID *sequence)
{
	m_data = new uint8_t[size];
	m_size = size;
	memcpy(m_data, data, size);
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
	}
	
	while (m_delay <= 0)
	{
		uint8_t data[2];
		uint32_t len;
		
		// make sure we have enough data left for one full event
		if (m_size - m_pos < 3)
		{
			m_atEnd = true;
			return UINT_MAX;
		}
		
		data[0] = m_data[m_pos++];
		data[1] = 0;
		if (data[0] & 0x80)
		{
			m_status = data[0];
			data[0] = m_data[m_pos++];
		}
		
		switch (m_status >> 4)
		{
		case 8:  // note off
		case 9:  // note on
		case 10: // polyphonic pressure
		case 11: // controller change
		case 14: // pitch bend
			data[1] = m_data[m_pos++];
			// fallthrough
		case 12: // program change
		case 13: // channel pressure (ignored)
			player.midiEvent(m_status, data[0], data[1]);
			break;
		
		case 15: // sysex / meta event
			if (m_status != 0xFF)
			{
				m_pos--;
				len = readVLQ();
				if (m_pos + len < m_size)
				{
					if (m_status == 0xf0)
						player.midiSysEx(m_data + m_pos, len);
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
SequenceMID::SequenceMID(const uint8_t *data, size_t size)
	: Sequence()
{
	readTracks(data, size);
	setDefaults();
}

// ----------------------------------------------------------------------------
SequenceMID::~SequenceMID()
{
	for (auto track : m_tracks)
		delete track;
}

// ----------------------------------------------------------------------------
bool SequenceMID::isValid(const uint8_t *data, size_t size)
{
	if (size < 12)
		return false;
	
	if (!memcmp(data, "MThd", 4))
	{
		uint32_t len = READ_U32BE(data, 4);
		if (len < 6) return false;
		
		uint16_t type = READ_U16BE(data, 8);
		if (type > 2) return false;
		
		return true;
	}
	else if (!memcmp(data, "RIFF", 4)
	      && !memcmp(data + 8, "RMID", 4))
	{
		return true;
	}

	return false;
}

// ----------------------------------------------------------------------------
void SequenceMID::readTracks(const uint8_t *data, size_t size)
{
	// need at least the MIDI header + one track header
	if (size < 23)
		return;
	
	if (!memcmp(data, "RIFF", 4))
	{
		uint32_t offset = 12;
		while (offset + 8 < size)
		{
			const uint8_t *bytes = data + offset;
			uint32_t chunkLen = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
			chunkLen = (chunkLen + 1) & ~1;
			
			// move to next subchunk
			offset += chunkLen + 8;
			if (offset > size)
			{
				// try to handle a malformed/truncated chunk
				chunkLen -= (offset - size);
				offset = size;
			}
			
			if (!memcmp(bytes, "data", 4))
			{
				if (isValid(bytes + 8, chunkLen))
					readTracks(bytes + 8, chunkLen);
				break;
			}
		}
	}
	else
	{
		uint32_t len = READ_U32BE(data, 4);
		
		m_type = READ_U16BE(data, 8);
		uint16_t numTracks = READ_U16BE(data, 10);
		m_ticksPerBeat = READ_U16BE(data, 12);
		
		uint32_t offset = len + 8;
		for (unsigned i = 0; i < numTracks; i++)
		{
			if (offset + 8 >= size)
				break;
			
			const uint8_t *bytes = data + offset;
			if (memcmp(bytes, "MTrk", 4))
				break;
			
			len = READ_U32BE(bytes, 4);
			offset += len + 8;
			if (offset > size)
			{
				// try to handle a malformed/truncated chunk
				len -= (offset - size);
				offset = size;
			}
			
			m_tracks.push_back(new MIDTrack(bytes + 8, len, this));
		}
	}
}

// ----------------------------------------------------------------------------
void SequenceMID::reset()
{
	Sequence::reset();
	setDefaults();
	
	for (auto& track : m_tracks)
		track->reset();
}

// ----------------------------------------------------------------------------
void SequenceMID::setDefaults()
{
	setUsecPerBeat(500000);
}

// ----------------------------------------------------------------------------
void SequenceMID::setUsecPerBeat(uint32_t usec)
{
	double usecPerTick = (double)usec / m_ticksPerBeat;
	m_ticksPerSec = 1000000 / usecPerTick;
}

// ----------------------------------------------------------------------------
unsigned SequenceMID::numSongs() const
{
	if (m_type != 2)
		return 1;
	else
		return m_tracks.size();
}

// ----------------------------------------------------------------------------
uint32_t SequenceMID::update(OPLPlayer& player)
{
	uint32_t tickDelay = UINT_MAX;
	
	bool tracksAtEnd = true;

	if (m_type != 2)
	{
		for (auto track : m_tracks)
		{
			if (!track->atEnd())
				tickDelay = std::min(tickDelay, track->update(player));
			tracksAtEnd &= track->atEnd();
		}
	}
	else if (m_songNum < m_tracks.size())
	{
		tickDelay   = m_tracks[m_songNum]->update(player);
		tracksAtEnd = m_tracks[m_songNum]->atEnd();
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
