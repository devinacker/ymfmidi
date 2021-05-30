#include "sequence_xmi.h"

#include <cmath>
#include <cstring>

#define READ_U16BE(data, pos) ((data[pos] << 8) | data[pos+1])
#define READ_U24BE(data, pos) ((data[pos] << 16) | (data[pos+1] << 8) | data[pos+2])
#define READ_U32BE(data, pos) ((data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3])

class XMITrack
{
public:
	XMITrack(const uint8_t *data, size_t size, SequenceXMI* sequence);
	~XMITrack();
	
	void reset();
	void advance(uint32_t time);
	uint32_t update(OPLPlayer& player);
	
	bool atEnd() const { return m_atEnd; }
	
private:
	uint32_t readVLQ();
	uint32_t readDelay();
	int32_t minDelay();

	SequenceXMI *m_sequence;
	uint8_t *m_data;
	uint32_t m_pos, m_size;
	int32_t m_delay;
	bool m_atEnd;
	
	struct XMINote
	{
		uint8_t channel, note;
		int32_t delay;
	};
	std::vector<XMINote> m_notes;
};

// ----------------------------------------------------------------------------
XMITrack::XMITrack(const uint8_t *data, size_t size, SequenceXMI *sequence)
{
	m_data = new uint8_t[size];
	m_size = size;
	memcpy(m_data, data, size);
	m_sequence = sequence;
	
	reset();
}

// ----------------------------------------------------------------------------
XMITrack::~XMITrack()
{
	delete[] m_data;
}

// ----------------------------------------------------------------------------
void XMITrack::reset()
{
	m_pos = m_delay = 0;
	m_atEnd = false;
	for (auto& note : m_notes)
		note.delay = 0;
}

// ----------------------------------------------------------------------------
void XMITrack::advance(uint32_t time)
{
	if (m_atEnd)
		return;
	
	m_delay -= time;
	for (auto& note : m_notes)
		note.delay -= time;
}

// ----------------------------------------------------------------------------
uint32_t XMITrack::readVLQ()
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
uint32_t XMITrack::readDelay()
{
	uint32_t delay = 0;
	uint8_t data = 0;

	if (m_pos >= m_size || (m_data[m_pos] & 0x80))
		return 0;

	do
	{
		data = m_data[m_pos];
		if (!(data & 0x80))
		{
			delay += data;
			m_pos++;
		}
	} while ((data == 0x7f) && (m_pos < m_size));
	
	return delay;
}

// ----------------------------------------------------------------------------
int32_t XMITrack::minDelay()
{
	int32_t delay = m_delay;
	for (auto& note : m_notes)
		delay = std::min(delay, note.delay);
	return delay;
}

// ----------------------------------------------------------------------------
uint32_t XMITrack::update(OPLPlayer& player)
{
	for (int i = 0; i < m_notes.size();)
	{
		if (m_notes[i].delay <= 0)
		{
			player.midiNoteOff(m_notes[i].channel, m_notes[i].note);
			m_notes[i] = m_notes.back();
			m_notes.pop_back();
		}	
		else
			i++;
	}
	
	while (m_delay <= 0)
	{
		uint8_t status;
		uint8_t data[2];
		uint8_t channel;
		uint32_t len;
		XMINote note;
		
		// make sure we have enough data left for one full event
		if (m_size - m_pos < 3)
		{
			m_atEnd = true;
			return 0;
		}
		
		status = m_data[m_pos++];
		data[0] = m_data[m_pos++];
		data[1] = 0;
		
		channel = status & 15;
		switch (status >> 4)
		{
		case 8:  // note off
		case 10: // polyphonic pressure
		case 11: // controller change
		case 14: // pitch bend
			data[1] = m_data[m_pos++];
			// fallthrough
		case 12: // program change
		case 13: // channel pressure (ignored)
			player.midiEvent(status, data[0], data[1]);
			break;
		
		case 9: // note on
			data[1] = m_data[m_pos++];
			player.midiNoteOn(channel, data[0], data[1]);
			
			note.channel = channel;
			note.note    = data[0];
			note.delay   = readVLQ();
			m_notes.push_back(note);
			break;
		
		case 15: // sysex / meta event
			if (status != 0xFF)
			{
				m_pos--;
				len = readVLQ();
				if (m_pos + len < m_size)
				{
					if (status == 0xf0)
						player.midiSysEx(m_data + m_pos, len);
					m_pos += len;
				}
				else
				{
					m_atEnd = true;
					return 0;
				}
				break;
			}
			
			len = readVLQ();
			
			// end-of-track marker (or data just ran out)
			if (data[0] == 0x2F || (m_pos + len >= m_size))
			{
				m_atEnd = true;
				return 0;
			}
			// tempo change
			if (data[0] == 0x51)
			{
				m_sequence->setUsecPerBeat(READ_U24BE(m_data, m_pos));
			}
			
			m_pos += len;
			break;
		}
		
		m_delay = readDelay();
	}
	
	return minDelay();
}

// ----------------------------------------------------------------------------
SequenceXMI::SequenceXMI(const uint8_t *data, size_t size)
	: Sequence()
{
	uint32_t chunkSize;
	while ((chunkSize = readRootChunk(data, size)) != 0)
	{
		data += chunkSize;
		size -= chunkSize;
	}
	
	setDefaults();
}

// ----------------------------------------------------------------------------
SequenceXMI::~SequenceXMI()
{
	for (auto track : m_tracks)
		delete track;
}

// ----------------------------------------------------------------------------
uint32_t SequenceXMI::readRootChunk(const uint8_t *data, size_t size)
{
	// need at least a root chunk and one subchunk (and its contents)
	if (size > 12 + 8)
	{
		// length of the root chunk
		uint32_t rootLen = READ_U32BE(data, 4);
		rootLen = (rootLen + 1) & ~1;
		// offset to the current sub-chunk
		uint32_t offset = 12;
		// offset to the data after the root chunk
		uint32_t rootEnd = std::min(rootLen + 8, (uint32_t)size);
		
		if (!memcmp(data, "FORM", 4))
		{
			uint32_t chunkLen;
		
			while (offset < rootEnd)
			{
				const uint8_t *bytes = data + offset;
				
				chunkLen = READ_U32BE(bytes, 4);
				chunkLen = (chunkLen + 1) & ~1;
				
				// move to next subchunk
				offset += chunkLen + 8;
				if (offset > rootEnd)
				{
					// try to handle a malformed/truncated chunk
					chunkLen -= (offset - rootEnd);
					offset = rootEnd;
				}
				
				if (!memcmp(bytes, "EVNT", 4))
					m_tracks.push_back(new XMITrack(bytes + 8, chunkLen, this));
			}
		}
		else if (!memcmp(data, "CAT ", 4))
		{
			while (offset < rootEnd)
			{
				offset += readRootChunk(data + offset, size - offset);
			}
		}
		
		return rootEnd;
	}
	
	return 0;
}

// ----------------------------------------------------------------------------
bool SequenceXMI::isValid(const uint8_t *data, size_t size)
{
	// need at least 2 root chunks and one EVNT chunk header
	if (size < 12)
		return false;
	
	if (memcmp(data,     "FORM", 4))
		return false;
	if (memcmp(data + 8, "XDIR", 4))
		return false;
	
	return true;
}

// ----------------------------------------------------------------------------
void SequenceXMI::reset()
{
	Sequence::reset();
	setDefaults();
	
	for (auto& track : m_tracks)
		track->reset();
}

// ----------------------------------------------------------------------------
void SequenceXMI::setDefaults()
{
	setUsecPerBeat(500000);
}

// ----------------------------------------------------------------------------
void SequenceXMI::setUsecPerBeat(uint32_t usec)
{
	double usecPerTick = (double)usec / ((usec * 3) / 25000);
	m_ticksPerSec = 1000000 / usecPerTick;
}

// ----------------------------------------------------------------------------
unsigned SequenceXMI::numSongs() const
{
	return m_tracks.size();
}

// ----------------------------------------------------------------------------
uint32_t SequenceXMI::update(OPLPlayer& player)
{
	bool atEnd = true;
	uint32_t tickDelay = 0;
	
	if (m_songNum < m_tracks.size())
	{
		tickDelay = m_tracks[m_songNum]->update(player);
		atEnd = m_tracks[m_songNum]->atEnd();
	}
	
	if (atEnd)
	{
		reset();
		m_atEnd = true;
		return 0;
	}
	
	m_atEnd = false;
	m_tracks[m_songNum]->advance(tickDelay);
	
	double samplesPerTick = player.sampleRate() / m_ticksPerSec;	
	return round(tickDelay * samplesPerTick);
}

