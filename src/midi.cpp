#include "drumming/midi.h"
#include <mutex>

namespace drumming {

static std::mutex          s_mtx;
static std::vector<MidiEv> s_pending;

void midiCallback(double, std::vector<unsigned char>* msg, void*) {
    if (msg->size() < 3) return;
    if ((((*msg)[0]) & 0xF0) == 0x90 && (*msg)[2] > 0) {
        std::lock_guard<std::mutex> lk(s_mtx);
        s_pending.push_back({(*msg)[1], (*msg)[2], std::chrono::steady_clock::now()});
    }
}

std::vector<MidiEv> pollMidi() {
    std::vector<MidiEv> evs;
    std::lock_guard<std::mutex> lk(s_mtx);
    evs.swap(s_pending);
    return evs;
}

} // namespace drumming
