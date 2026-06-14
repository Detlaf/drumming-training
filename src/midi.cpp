#include "drumming/midi.h"
#include "drumming/constants.h"
#include <mutex>

namespace drumming {

static std::mutex          s_mtx;
static std::vector<MidiEv> s_pending;

void midiCallback(double, std::vector<unsigned char>* msg, void*) {
    if (msg->size() < 3) return;
    // Note-on with enough velocity to be a real strike. Velocity 0 is the
    // note-off convention; anything below MIN_HIT_VELOCITY is treated as
    // cross-talk (a ghost trigger from a neighbouring pad) and dropped.
    if ((((*msg)[0]) & 0xF0) == 0x90 && (*msg)[2] >= MIN_HIT_VELOCITY) {
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

MidiPortAction decideMidiAction(bool currentlyConnected, unsigned int portCount) {
    if (!currentlyConnected && portCount > 0) return MidiPortAction::Open;
    if (currentlyConnected  && portCount == 0) return MidiPortAction::Close;
    return MidiPortAction::None;
}

} // namespace drumming
