#include "drumming/types.h"

namespace drumming {

const Voice VOICES[] = {
    {49, "Crash",   -30.f, true,  {230, 220,  70}},
    {51, "Ride",    -18.f, true,  {230, 220,  70}},
    {42, "Hi-Hat",  - 6.f, true,  {230, 220,  70}},
    {48, "Hi Tom",    6.f, false, { 43, 187, 251}},
    {45, "Mid Tom",  18.f, false, { 43, 187, 251}},
    {38, "Snare",    30.f, false, {255, 130, 130}},
    {43, "Lo Tom",   42.f, false, { 43, 187, 251}},
    {36, "Bass",     66.f, false, {160, 100, 200}},
    {44, "HH Ped",   78.f, true,  {200, 180,  60}},
};
const int NUM_VOICES = (int)(sizeof(VOICES) / sizeof(VOICES[0]));

// Electronic kits report the hi-hat on several note numbers depending on the
// pedal position and strike zone. General MIDI uses 42 (closed) and 46 (open);
// Roland/Alesis kits add edge variants 22 (closed edge) and 26 (open edge).
// Collapse all of them onto the single Hi-Hat lane (note 42). Note 44
// (foot-close pedal) keeps its own "HH Ped" lane.
static int normalizeNote(int midiNote) {
    switch (midiNote) {
    case 22: // closed edge
    case 26: // open edge
    case 46: // open (head)
        return 42;
    default:
        return midiNote;
    }
}

int voiceIndex(int midiNote) {
    int note = normalizeNote(midiNote);
    for (int i = 0; i < NUM_VOICES; ++i)
        if (VOICES[i].midiNote == note) return i;
    return -1;
}

} // namespace drumming
