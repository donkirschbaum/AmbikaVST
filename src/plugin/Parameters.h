// Ambika VST - parameter definitions (ranges and display strings follow the
// Ambika firmware's controller/parameter.cc and resources). GPL v3.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ambika_plugin {

enum class Level { Patch, Part, Plugin };

enum class Unit {
  Raw, Int, Waveform, Operator, SubWaveform, FilterMode, LfoShape, VoiceLfoShape,
  LfoRate, LfoSync, ModSource, ModDest, ModifierOp, Raga, ArpMode, ArpDirection,
  ArpGrid, PolyMode, Boolean, Board, ClockSource, Groove, MidiChannel, Percent,
  Decibel, Bpm
};

struct ParamDef {
  const char* id;
  const char* shortName;   // 4-5 chars, LCD style
  const char* longName;    // host automation name
  Level level;
  int offset;              // byte offset in Patch / PartData (or -1)
  int minValue;
  int maxValue;
  int defaultValue;
  Unit unit;
  int midiCC;              // Ambika CC mapping, -1 if none
};

const std::vector<ParamDef>& paramDefs();
const ParamDef* findParam(const juce::String& id);
juce::String formatValue(const ParamDef& def, int value);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Plugin-level parameter ids.
namespace pid {
static constexpr const char* board = "board";
static constexpr const char* voices = "voices";
static constexpr const char* output = "output";
static constexpr const char* noise = "noise";
static constexpr const char* spread = "stereo";
static constexpr const char* drive = "drive";
static constexpr const char* clockSource = "clock_src";
static constexpr const char* bpm = "bpm";
static constexpr const char* grooveTemplate = "groove";
static constexpr const char* grooveAmount = "groove_amt";
static constexpr const char* latch = "latch";
static constexpr const char* midiChannel = "midi_ch";
}  // namespace pid

}  // namespace ambika_plugin
