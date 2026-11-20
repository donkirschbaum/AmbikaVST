// Ambika VST - top level: one Ambika part driving up to 6 voicecards.
// GPL v3.
#pragma once

#include "analog.h"
#include "part.h"
#include "voicecard.h"

namespace ambika {

// 24 ppqn clock with the Ambika groove templates (swing, shuffle, push, lag,
// human, monkey) applied over a 16-step / 4-beat pattern.
class GrooveClock {
 public:
  void SetGroove(uint8_t groove_template, uint8_t amount);
  // Finds the ticks falling in [ppq_a, ppq_b). Returns their count.
  int TicksIn(double ppq_a, double ppq_b, double* ticks, int max_ticks) const;
 private:
  double tick_time_[97];  // in quarter notes, within a 4-beat pattern
  uint8_t template_ = 0xff, amount_ = 0xff;
};

struct EngineSettings {
  int board = BOARD_SMR4;
  int num_voices = 6;
  float output_gain = 0.5f;
  float noise = 0.3f;
  float stereo_spread = 0.0f;
  float drive = 1.0f;
  int clock_release = 4;   // "latch" setting of the Ambika multi
};

class Engine {
 public:
  void Init(double host_sample_rate);
  void SetSampleRate(double host_sample_rate);
  void ApplySettings(const EngineSettings& s);

  Patch* mutable_patch() { return part_.mutable_patch(); }
  PartData* mutable_data() { return part_.mutable_data(); }
  void Touch() { part_.Touch(); }

  // MIDI.
  void NoteOn(uint8_t note, uint8_t velocity, bool internal_clock_may_start);
  void NoteOff(uint8_t note) { part_.NoteOff(note); }
  void PitchBend(uint16_t value14) { part_.SetModulation(MOD_SRC_PITCH_BEND, U14ShiftRight6(value14)); }
  void ControlChange(uint8_t cc, uint8_t value);
  void ChannelPressure(uint8_t v) { part_.SetModulation(MOD_SRC_AFTERTOUCH, v << 1); }
  void PolyPressure(uint8_t note, uint8_t v) { part_.Aftertouch(note, v << 1); }
  void AllNotesOff() { part_.AllNotesOff(); }
  void AllSoundOff();

  // Clock / transport (Multi::Start, Stop, Clock).
  void Start();
  void Stop();
  void ClockTick(bool internal);
  bool running() const { return running_; }

  void Render(float* left, float* right, int n);

  // Monitoring for the UI.
  uint8_t voice_gate(int i) const { return voices_[i].gate(); }
  uint8_t voice_level(int i) const { return voices_[i].vca(); }
  uint8_t lfo_value(int i) const { return part_.lfo_value(i); }
  uint8_t step(int i) const { return part_.step(i); }

 private:
  void RenderVoicecardBlock();

  Part part_;
  Voicecard voices_[kNumVoices];
  AnalogVoice analog_[kNumVoices];
  Resampler resampler_;
  EngineSettings settings_;
  uint8_t previous_vca_[kNumVoices];
  uint8_t lfo_refresh_cycle_ = 0;
  bool running_ = false;
  uint16_t idle_ticks_ = 0;
  float pan_l_[kNumVoices], pan_r_[kNumVoices];
  double host_rate_ = 44100.0;
};

}  // namespace ambika
