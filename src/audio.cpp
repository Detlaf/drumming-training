#include "audio.h"
#include <QAudioSink>
#include <QMediaDevices>
#include <cmath>
#include <cstdint>

namespace drumming {

QByteArray makeClick(float freqHz, float durMs, int sr) {
    int n = (int)(sr * durMs / 1000.f);
    QByteArray pcm;
    pcm.resize(n * (int)sizeof(std::int16_t));
    auto* s = reinterpret_cast<std::int16_t*>(pcm.data());
    for (int i = 0; i < n; ++i) {
        float t   = (float)i / sr;
        float env = std::exp(-t * 180.f);
        s[i] = (std::int16_t)(28000 * env * std::sin(2.f * 3.14159265f * freqHz * t));
    }
    return pcm;
}

Metronome::Metronome() {
    format_.setSampleRate(44100);
    format_.setChannelCount(1);
    format_.setSampleFormat(QAudioFormat::Int16);

    hiClick_ = makeClick(1100.f);
    loClick_ = makeClick(700.f);

    sink_ = std::make_unique<QAudioSink>(QMediaDevices::defaultAudioOutput(), format_);
}

Metronome::~Metronome() = default;

void Metronome::play(const QByteArray& pcm) {
    // Restart the sink against a rewound buffer. Clicks are ~18 ms and beats are
    // far longer, so a single shared buffer never overlaps with itself.
    sink_->stop();
    buffer_.close();
    buffer_.setData(pcm);
    buffer_.open(QIODevice::ReadOnly);
    sink_->start(&buffer_);
}

void Metronome::playHi() { play(hiClick_); }
void Metronome::playLo() { play(loClick_); }

} // namespace drumming
