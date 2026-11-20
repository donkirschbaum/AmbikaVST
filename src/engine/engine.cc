// Ambika VST - top level engine. GPL v3.

#include "engine.h"

#include <cmath>

#include "resources.h"

namespace ambika {

static const double kModelRate = kVoicecardSampleRate * 2.0;

// ------------------------------------------------------------ groove clock --
void GrooveClock::SetGroove(uint8_t groove_template, uint8_t amount) {
  if (groove_template == template_ && amount == amount_) return;
  template_ = groove_template;
  amount_ = amount;
  const int16_t* lut = reinterpret_cast<const int16_t*>(
      ctl::lookup_table_table[ctl::LUT_RES_GROOVE_SWING + (groove_template % 6)]);
  // Same arithmetic as Multi::ComputeInternalClockOverflowsTable, expressed
  // in relative durations: base * (1 + lut * amount / 65536).
  double durations[16];
  double total = 0.0;
  for (int i = 0; i < 16; ++i) {
    durations[i] = 1.0 + static_cast<double>(lut[i]) * amount / 65536.0;
    if (durations[i] < 0.1) durations[i] = 0.1;
    total += durations[i] * 6.0;
  }
  double t = 0.0;
  for (int n = 0; n < 96; ++n) {
    tick_time_[n] = t * 4.0 / total;
    t += durations[n / 6];
  }
  tick_time_[96] = 4.0;
}

int GrooveClock::TicksIn(double a, double b, double* ticks, int max_ticks) const {
  int count = 0;
  if (b <= a) return 0;
  int64_t first = static_cast<int64_t>(std::floor(a / 4.0));
  int64_t last = static_cast<int64_t>(std::floor(b / 4.0));
  for (int64_t p = first; p <= last; ++p) {
    double base = static_cast<double>(p) * 4.0;
    for (int n = 0; n < 96; ++n) {
      double t = base + tick_time_[n];
      if (t >= a && t < b && count < max_ticks) ticks[count++] = t;
    }
  }
  return count;
}

// ----------------------------------------------------------------- engine --
void Engine::Init(double host_sample_rate) {
  for (int i = 0; i < kNumVoices; ++i) {
    voices_[i].Init(static_cast<uint16_t>(0x1234 + i * 0x2f1));
    previous_vca_[i] = 0;
  }
  part_.Init(voices_, kNumVoices, 0xACE1);
  // Let the voicecards "boot": the hardware processes blocks long before the
  // first note arrives, so envelope rates etc. are always initialized.
  uint8_t dummy[kAudioBlockSize];
  for (int i = 0; i < kNumVoices; ++i) voices_[i].ProcessBlock(dummy);
  SetSampleRate(host_sample_rate);
  ApplySettings(settings_);
}

void Engine::SetSampleRate(double host_sample_rate) {
  host_rate_ = host_sample_rate;
  // Each voicecard has slightly different analog components.
  uint32_t r = 0x9E3779B9u;
  for (int i = 0; i < kNumVoices; ++i) {
    auto rnd = [&r]() {
      r ^= r << 13; r ^= r >> 17; r ^= r << 5;
      return (static_cast<float>(r & 0xffff) / 32768.0f) - 1.0f;  // -1..1
    };
    CardTolerances t;
    t.cutoff_ratio = std::exp2(rnd() * 0.06f);  // +/- 0.7 semitone
    t.resonance_gain = 1.0f + rnd() * 0.04f;
    t.vca_gain = 1.0f + rnd() * 0.03f;
    t.offset = rnd() * 0.01f;
    analog_[i].Init(static_cast<float>(kModelRate), t, 1234567u + i * 7919u);
  }
  resampler_.Init(kModelRate, host_sample_rate);
}

void Engine::ApplySettings(const EngineSettings& s) {
  if (s.num_voices != settings_.num_voices) part_.SetNumVoices(static_cast<uint8_t>(s.num_voices));
  settings_ = s;
  for (int i = 0; i < kNumVoices; ++i) {
    analog_[i].set_board(s.board);
    analog_[i].set_noise(s.noise);
    analog_[i].set_drive(s.drive);
    // Alternate voices left / right.
    float pos = (i & 1 ? 1.0f : -1.0f) * (0.4f + 0.12f * (i >> 1)) * s.stereo_spread;
    float angle = (pos + 1.0f) * 0.25f * 3.14159265f;
    pan_l_[i] = std::cos(angle) * 1.41421356f;
    pan_r_[i] = std::sin(angle) * 1.41421356f;
  }
}

void Engine::NoteOn(uint8_t note, uint8_t velocity, bool internal_clock_may_start) {
  if (!running_ && internal_clock_may_start) Start();
  part_.NoteOn(note, velocity);
}

void Engine::ControlChange(uint8_t cc, uint8_t value) {
  switch (cc) {
    case 1: part_.SetModulation(MOD_SRC_WHEEL, value << 1); break;
    case 2: part_.SetModulation(MOD_SRC_WHEEL_2, value << 1); break;
    case 4: part_.SetModulation(MOD_SRC_EXPRESSION, value << 1); break;
    case 64: part_.SustainPedal(value >= 64); break;
    case 120: AllSoundOff(); break;
    case 121: part_.ResetAllControllers(); break;
    case 123: part_.AllNotesOff(); break;
    default: break;
  }
}

void Engine::AllSoundOff() {
  part_.AllSoundOff();
  for (auto& a : analog_) a.Reset();
}

void Engine::Start() {
  idle_ticks_ = 0;
  part_.Start();
  running_ = true;
}

void Engine::Stop() {
  part_.Stop();
  running_ = false;
}

void Engine::ClockTick(bool internal) {
  if (!running_) return;
  part_.Clock();
  if (internal) {
    if (part_.num_pressed_keys() == 0) ++idle_ticks_;
    else idle_ticks_ = 0;
    if (idle_ticks_ > settings_.clock_release * 24) Stop();
  }
}

void Engine::RenderVoicecardBlock() {
  // Controller side: LFOs are computed at the control rate.
  ++lfo_refresh_cycle_;
  part_.UpdateLfos(lfo_refresh_cycle_ & 1);

  float mix_l[kAudioBlockSize * 2];
  float mix_r[kAudioBlockSize * 2];
  for (int j = 0; j < kAudioBlockSize * 2; ++j) mix_l[j] = mix_r[j] = 0.0f;

  uint8_t samples[kAudioBlockSize];
  float voice_out[kAudioBlockSize * 2];
  for (int i = 0; i < settings_.num_voices; ++i) {
    voices_[i].ProcessBlock(samples);
    // The VCA DAC is refreshed with the value computed one block earlier.
    analog_[i].SetControls(voices_[i].cutoff(), voices_[i].resonance(),
                           previous_vca_[i], voices_[i].filter_mode(),
                           voices_[i].crush());
    previous_vca_[i] = voices_[i].vca();
    analog_[i].Process(samples, kAudioBlockSize, voice_out);
    for (int j = 0; j < kAudioBlockSize * 2; ++j) {
      mix_l[j] += voice_out[j] * pan_l_[i];
      mix_r[j] += voice_out[j] * pan_r_[i];
    }
  }
  // Mix bus: the summing op-amp saturates softly instead of clipping hard.
  const float g = settings_.output_gain;
  auto bus = [](float x) {
    const float a = std::fabs(x);
    if (a < 0.75f) return x;
    const float over = (a - 0.75f) / 0.35f;
    const float y = 0.75f + 0.35f * over / std::sqrt(1.0f + over * over);
    return x < 0.0f ? -y : y;
  };
  for (int j = 0; j < kAudioBlockSize * 2; ++j) resampler_.Push(bus(mix_l[j] * g), bus(mix_r[j] * g));
}

void Engine::Render(float* left, float* right, int n) {
  for (int i = 0; i < n; ++i) {
    while (!resampler_.CanPop()) RenderVoicecardBlock();
    resampler_.Pop(&left[i], &right[i]);
  }
}

}  // namespace ambika
