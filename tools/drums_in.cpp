#include <map>
#include <string>
#include <iostream>
#include <cstdlib>
#include <signal.h>
#include "RtMidi.h"
#include <thread>
#include <chrono>

const std::map<int, std::string> DRUM_MAP = {
    {35, "Bass Drum 2"},
    {36, "Bass Drum 1"},
    {38, "Snare"},
    {41, "Low Floor Tom"},
    {42, "Closed Hi-Hat"},
    {43, "Low Tom"},
    {44, "Hi-Hat Pedal"},
    {45, "Mid Tom"},
    {46, "Open Hi-Hat"},
    {48, "Hi Tom"},
    {49, "Crash Cymbal 1"},
    {51, "Ride Cymbal"},
};

void midiCallback(double stamp, std::vector<unsigned char> *msg, void *userData ) {
    int status = (*msg)[0];
    int note   = (*msg)[1];
    int vel    = (*msg)[2];

    bool noteOn = (status & 0xF0) == 0x90 && vel > 0;
    if (noteOn && DRUM_MAP.count(note)) {
        std::cout << DRUM_MAP.at(note) << " (vel=" << vel << ")\n";
    }
}

int main()
{
  RtMidiIn *midiin = new RtMidiIn();
 
  // Check available ports.
  unsigned int nPorts = midiin->getPortCount();
  if ( nPorts == 0 ) {
    std::cout << "No ports available!\n";
    goto cleanup;
  }
 
  midiin->openPort( 0 );
 
  // Set our callback function.  This should be done immediately after
  // opening the port to avoid having incoming messages written to the
  // queue.
  midiin->setCallback( &midiCallback );
 
  // Don't ignore sysex, timing, or active sensing messages.
  midiin->ignoreTypes( false, false, false );
 
  std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
  char input;
  std::cin.get(input);
 
  // Clean up
 cleanup:
  delete midiin;
 
  return 0;
}
