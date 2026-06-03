#pragma once
#include <vector>
#include "drumming/types.h"

namespace drumming {

void             midiCallback(double, std::vector<unsigned char>* msg, void*);
std::vector<MidiEv> pollMidi();

} // namespace drumming
