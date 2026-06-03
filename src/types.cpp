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

const KitPad KIT_PADS[] = {
    {49, "Crash",   {150,  50}, 45, {227, 248,   8}},
    {42, "Hi-Hat",  {280,  60}, 40, {227, 248,   8}},
    {51, "Ride",    {630,  55}, 50, {227, 248,   8}},
    {38, "Snare",   {230, 210}, 45, { 43, 187, 251}},
    {48, "Hi Tom",  {430, 170}, 38, { 43, 187, 251}},
    {45, "Mid Tom", {545, 190}, 38, { 43, 187, 251}},
    {43, "Lo Tom",  {510, 300}, 42, { 43, 187, 251}},
    {36, "Bass",    {360, 370}, 65, {160, 100, 200}},
    {44, "HH Ped",  {160, 370}, 30, {200, 180,  60}},
};
const int NUM_KIT_PADS = (int)(sizeof(KIT_PADS) / sizeof(KIT_PADS[0]));

int voiceIndex(int midiNote) {
    for (int i = 0; i < NUM_VOICES; ++i)
        if (VOICES[i].midiNote == midiNote) return i;
    return -1;
}

} // namespace drumming
