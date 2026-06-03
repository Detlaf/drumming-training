#include <SFML/Graphics.hpp>
#include "RtMidi.h"
#include <map>
#include <string>
#include <mutex>
#include <chrono>


// Shared between MIDI and rendering threads
struct DrumHit {
    std::chrono::steady_clock::time_point time;
    int velocity;
};

std::mutex hitsMutex;
std::map<int, DrumHit> activeHits; // <MIDI note number: DrumHit>

// A drum kit component
struct DrumPad {
    int note;
    std::string name;
    sf::Vector2f pos;
    float radius;
    sf::Color color;
};

// Layout
const std::vector<DrumPad> KIT = {
    {49, "Crash",      {150, 110}, 45, sf::Color(180, 80,  80)},
    {42, "Hi-Hat",     {280, 120}, 40, sf::Color(200, 180, 60)},
    {51, "Ride",       {630, 115}, 50, sf::Color(200, 180, 60)},
    {38, "Snare",      {230, 270}, 45, sf::Color(80,  130, 200)},
    {48, "Hi Tom",     {430, 230}, 38, sf::Color(80,  180, 120)},
    {45, "Mid Tom",    {545, 250}, 38, sf::Color(80,  180, 120)},
    {43, "Low Tom",    {510, 360}, 42, sf::Color(80,  180, 120)},
    {41, "Floor Tom",  {620, 370}, 45, sf::Color(80,  180, 120)},
    {36, "Bass Drum",  {360, 430}, 65, sf::Color(160, 100, 200)},
    {35, "Bass Drum2", {360, 430}, 65, sf::Color(160, 100, 200)}, // same pad
    {44, "HH Pedal",   {160, 430}, 30, sf::Color(200, 180, 60)},
    {46, "Open HH",    {280, 120}, 40, sf::Color(230, 210, 90)},  // same pos as closed HH
};

void midiCallback(double /*stamp*/, std::vector<unsigned char>* msg, void* /*userData*/) {
    if (msg->size() < 3) return; // not enough values in the message
    int status = (*msg)[0];
    int note = (*msg)[1];
    int vel = (*msg)[2];

    bool noteOn = (status & 0xF0) == 0x90 && vel > 0;

    std::lock_guard<std::mutex> lock(hitsMutex);
    if (noteOn) {
        activeHits[note] = {std::chrono::steady_clock::now(), vel};
    }
}

// Rendering loop
int main() {
    RtMidiIn midiin;
    if (midiin.getPortCount() == 0) {
        std::cerr << "No MIDI ports found\n";
        return 1;
    }
    midiin.openPort(0);
    midiin.setCallback(&midiCallback);
    midiin.ignoreTypes(false, false, false);

    // Window setup
    sf::RenderWindow window(sf::VideoMode({800u, 600u}), "Drum Kit");
    window.setFramerateLimit(60);

    sf::Font font;
    bool hasFont = font.openFromFile("/System/Library/Fonts/Helvetica.ttc");

    const float FADE_MS = 400.f;

    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent())
            if (event->is<sf::Event::Closed>()) window.close();

        window.clear(sf::Color(30, 30, 30));

        auto now = std::chrono::steady_clock::now();

        for (const auto& pad : KIT) {
            // Determine brightness from the latest hit
            float brightness = 0.15f; // resting state - dim

            {
                std::lock_guard<std::mutex> lock(hitsMutex);
                auto it = activeHits.find(pad.note);
                if (it != activeHits.end()) {
                    float elapsed = std::chrono::duration<float, std::milli>(
                        now - it->second.time).count();
                    float t = std::max(0.f, 1.f - elapsed / FADE_MS);
                    brightness = 0.15f + 0.85f * t * (it->second.velocity / 127.f);
                }
            }

            // Scale the pad's color by brightness
            sf::Color c(
                (uint8_t)(pad.color.r * brightness),
                (uint8_t)(pad.color.g * brightness),
                (uint8_t)(pad.color.b * brightness)
            );

            sf::CircleShape shape(pad.radius);
            shape.setOrigin({pad.radius, pad.radius});
            shape.setPosition(pad.pos);
            shape.setFillColor(c);
            shape.setOutlineThickness(2);
            shape.setOutlineColor(sf::Color(200, 200, 200, 80));
            window.draw(shape);

            // Label
            if (hasFont) {
                sf::Text label(font, pad.name, 11);
                label.setFillColor(sf::Color(220, 220, 220, 180));
                label.setPosition({pad.pos.x - pad.radius, pad.pos.y + pad.radius + 4});
                window.draw(label);
            }
        }

        window.display();
    }
    return 0;
}