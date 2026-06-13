#pragma once
#include <vector>
#include "drumming/types.h"

namespace drumming {

void             midiCallback(double, std::vector<unsigned char>* msg, void*);
std::vector<MidiEv> pollMidi();

// Hot-plug: what the main loop should do with the MIDI port given the current
// connection state and the number of ports currently available.
enum class MidiPortAction { None, Open, Close };

MidiPortAction decideMidiAction(bool currentlyConnected, unsigned int portCount);

} // namespace drumming
