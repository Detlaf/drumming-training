#pragma once
#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <memory>

class QAudioSink;

namespace drumming {

// Synthesize a click as raw 16-bit mono PCM @ 44100 Hz: an exponential-envelope
// sine burst (ported from the former SFML makeClick).
QByteArray makeClick(float freqHz, float durMs = 18.f, int sr = 44100);

// Metronome plays two precomputed clicks through a Qt Multimedia QAudioSink:
// a hi click (1100 Hz) for downbeats and a lo click (700 Hz) for other beats.
// Lives in the drum_viz target only — keeps Qt out of drumming_core.
class Metronome {
public:
    Metronome();
    ~Metronome();

    void playHi();
    void playLo();

private:
    void play(const QByteArray& pcm);

    QAudioFormat format_;
    QByteArray hiClick_;
    QByteArray loClick_;
    QBuffer buffer_;
    std::unique_ptr<QAudioSink> sink_;
};

} // namespace drumming
