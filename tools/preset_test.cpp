// Renders every factory preset through the full plugin (parameters -> engine)
// and prints level statistics. Built as a JUCE console app.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../src/plugin/PluginProcessor.h"

int main() {
  juce::ScopedJuceInitialiser_GUI init;
  AmbikaProcessor proc;
  const double sr = 48000.0;
  const int block = 256;
  proc.setPlayConfigDetails(0, 2, sr, block);
  proc.prepareToPlay(sr, block);
  int failures = 0;
  for (int p = 0; p < proc.getNumPrograms(); ++p) {
    proc.setCurrentProgram(p);
    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;
    double peak = 0, sum = 0; long count = 0; bool bad = false;
    // Let parameters settle, then hold a chord for 1.5 s and release for 1 s.
    for (int b = 0; b < static_cast<int>(sr * 3.0 / block); ++b) {
      midi.clear();
      if (b == 4) for (int n : {48, 55, 60, 64}) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8)100), 0);
      if (b == 4 + static_cast<int>(sr * 1.5 / block)) for (int n : {48, 55, 60, 64}) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0);
      buf.clear();
      proc.processBlock(buf, midi);
      for (int i = 0; i < block; ++i) {
        float v = buf.getSample(0, i);
        if (!std::isfinite(v)) bad = true;
        peak = std::max(peak, (double)std::abs(v));
        sum += v * v; ++count;
      }
    }
    double rms = std::sqrt(sum / count);
    bool silent = rms < 0.005;
    bool loud = peak > 1.5;
    if (bad || silent || loud) ++failures;
    std::printf("%-18s peak %.3f rms %.3f %s%s%s\n", proc.getProgramName(p).toRawUTF8(), peak, rms,
                bad ? "NAN " : "", silent ? "SILENT " : "", loud ? "TOO LOUD" : "");
  }
  // State round trip.
  juce::MemoryBlock mb;
  proc.setCurrentProgram(5);
  proc.getStateInformation(mb);
  proc.setCurrentProgram(0);
  proc.setStateInformation(mb.getData(), (int)mb.getSize());
  auto* wave = proc.state().getRawParameterValue("osc1_shape");
  std::printf("state restore: osc1_shape=%d (expected 16) %s\n", (int)wave->load(), (int)wave->load() == 16 ? "OK" : "FAIL");
  if ((int)wave->load() != 16) ++failures;
  return failures;
}
