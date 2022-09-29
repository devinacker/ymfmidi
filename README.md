# ymfmidi

ymfmidi is a MIDI player based on the OPL3 emulation core from [ymfm](https://github.com/aaronsgiles/ymfm).

# Support

### Features

* Supports both 4-operator and 2-operator instruments
* Supports some Roland GS, Yamaha XG, and GM Level 2 features (e.g. multiple instrument banks and percussion channels)
* Can emulate multiple chips at once to increase polyphony
* Can output to WAV files
* Can play files containing multiple songs (XMI, MIDI format 2)
* Supported sequence formats:
    * `.hmi`, `.hmp` HMI Sound Operating System
    * `.mid` Standard MIDI files (format 0 or 1)
    * `.mus` DMX sound system / Doom engine
    * `.rmi` Microsoft RIFF MIDI
    * `.xmi` Miles Sound System / Audio Interface Library
* Supported instrument patch formats:
    * `.ad`, `.opl` Miles Sound System / Audio Interface Library
    * `.op2` DMX sound system / Doom engine
    * `.tmb` Apogee Sound System
    * `.wopl` Wohlstand OPL3 editor

More sequence and instrument file formats will probably be supported in the future.


# Usage

ymfmidi can be used as a standalone program (with SDL2), or incorporated into other software to provide easy OPL3-based MIDI playback.

For standalone usage instructions, run the player with no arguments. The player uses patches from [DMXOPL](https://github.com/sneakernets/DMXOPL) by default, but an alternate patch set can be specified as a command-line argument.

To incorporate ymfmidi into your own programs, include everything in the `src` and `ymfm` directories (except for `src/main.cpp` and `src/console.*`), preferably in its own subdirectory somewhere. After that, somewhere in your code:
* `#include "player.h"`
* Create an instance of `OPLPlayer`, optionally specifying a type of chip (the default is `OPLPlayer::ChipOPL3`) and number of chips to emulate (the default is 1)
* Call the `loadSequence` and `loadPatches` methods to load music and instrument data from a path, an existing `FILE*`, or a buffer in memory
* (Optional) Call the `setLoop`, `setSampleRate`, `setGain`, and `setFilter` methods to set up playback parameters
* Periodically call one of the `generate` methods to output audio in either signed 16-bit or floating-point format
* (Optional) Call the `reset` method to restart playback at the beginning

A proper static lib build method will be available sooner or later.

### Real-time MIDI control

In addition to loading a MIDI file, it's also possible to send MIDI messages to an `OPLPlayer` instance in real time using some of its public methods.

To send a standard two- or three-byte MIDI message:

```
	void midiEvent(uint8_t status, uint8_t data0, uint8_t data1 = 0);
```

Helper methods are also available for the most common messages:

```
	void midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
	void midiNoteOff(uint8_t channel, uint8_t note);
	void midiPitchControl(uint8_t channel, double pitch); // range is -1.0 to 1.0
	void midiProgramChange(uint8_t channel, uint8_t patchNum);
	void midiControlChange(uint8_t channel, uint8_t control, uint8_t value);
	
	void midiSetBendRange(uint8_t channel, uint8_t semitones);
```

System Exclusive messages can also be sent directly to the player (primarily for enabling supported GS or XG features):

```
	void midiSysEx(const uint8_t *data, uint32_t length);
```

It is not required to include the opening `0xF0` byte that normally precedes a sysex event; this is due mainly to the way that these events are stored in MIDI files. If `data` includes this opening byte, it should also be included in `length`, but will otherwise be ignored.

# License

ymfmidi and the underlying ymfm library are both released under the 3-clause BSD license.

