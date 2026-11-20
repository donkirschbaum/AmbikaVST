// Ambika VST - audio processor. GPL v3.
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../engine/engine.h"
#include "Parameters.h"

class AmbikaProcessor : public juce::AudioProcessor, private juce::AsyncUpdater {
 public:
  AmbikaProcessor();
  ~AmbikaProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return JucePlugin_Name; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 2.0; }

  int getNumPrograms() override;
  int getCurrentProgram() override { return currentProgram; }
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // Editor helpers.
  juce::AudioProcessorValueTreeState& state() { return apvts; }
  void loadPreset(int index);
  juce::String getProgramBank(int index) const;
  // Loads raw Ambika memory: 112 bytes of patch and 84 bytes of part data.
  void loadProgramBytes(const uint8_t* patch, const uint8_t* part);
  void randomizePatch();
  uint8_t getSequenceByte(int i) const { return sequence[static_cast<size_t>(i)].load(); }
  void setSequenceByte(int i, uint8_t v) { sequence[static_cast<size_t>(i)].store(v); }
  bool savePatchFile(const juce::File& f);
  bool loadPatchFile(const juce::File& f);

  // Monitoring.
  std::array<std::atomic<uint8_t>, 6> voiceLevel;
  std::array<std::atomic<uint8_t>, 3> lfoValue;
  std::array<std::atomic<uint8_t>, 3> seqStep;
  std::atomic<int> lastMidiNote{-1};

 private:
  void handleAsyncUpdate() override;
  void syncParametersToEngine();
  void handleMidi(const juce::MidiMessage& m, bool hostClockRunning);
  void resetToDefaults();
  int paramValue(int index) const;

  juce::AudioProcessorValueTreeState apvts;
  std::vector<std::atomic<float>*> rawParams;   // same order as paramDefs()
  std::array<std::atomic<uint8_t>, 64> sequence;

  ambika::Engine engine;
  ambika::GrooveClock grooveClock;
  ambika::Patch lastPatch;
  ambika::PartData lastData;
  bool engineReady = false;
  bool hostWasPlaying = false;
  double internalPpq = 0.0;
  int currentProgram = 0;

  // MIDI CC -> parameter changes, applied on the message thread.
  juce::AbstractFifo ccFifo{256};
  std::array<std::pair<int, int>, 256> ccQueue;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbikaProcessor)
};
