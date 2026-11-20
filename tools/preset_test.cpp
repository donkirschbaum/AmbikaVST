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
  int failures = 0, nanCount = 0, silentCount = 0, loudCount = 0;
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
    bool silent = rms < 0.002;
    bool loud = peak > 1.5;
    if (bad || loud) ++failures;  // silence can be legitimate (drum parts, seq-only)
    if (bad || silent || loud || p < 16)
      std::printf("%4d %-40s peak %.3f rms %.3f %s%s%s\n", p, proc.getProgramName(p).toRawUTF8(), peak, rms,
                  bad ? "NAN " : "", silent ? "SILENT " : "", loud ? "TOO LOUD" : "");
    nanCount += bad; silentCount += silent; loudCount += loud;
  }
  std::printf("programs: %d  nan: %d  silent: %d  too loud: %d\n", proc.getNumPrograms(), nanCount, silentCount, loudCount);
  // Original Ambika .PRO file import.
  if (const char* pro = std::getenv("AMBIKA_TEST_PRO")) {
    bool ok = proc.loadPatchFile(juce::File(pro));
    std::printf(".PRO import: %s (osc1_shape=%d)\n", ok ? "OK" : "FAIL", (int)proc.state().getRawParameterValue("osc1_shape")->load());
    if (!ok) ++failures;
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
