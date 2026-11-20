// Ambika VST - one Ambika voicecard (digital part).
//
// Port of voicecard/{oscillator,sub_oscillator,transient_generator,envelope,
// voice}.{h,cc} and common/lfo.h from the Ambika firmware,
// Copyright 2011-2012 Emilie Gillet. GPL v3.
//
// The static singletons of the firmware (one MCU = one voice) are turned into
// instance members so that six cards can live side by side.

#pragma once

#include "avr_compat.h"
#include "patch.h"
#include "resources.h"

namespace ambika {

static const uint8_t kAudioBlockSize = 40;   // == kControlRate
// 20 MHz / 510 (phase-correct PWM, prescaler 1).
static const double kVoicecardSampleRate = 20000000.0 / 510.0;  // 39215.686 Hz

static const int16_t kHighestNote = 120 * 128;
static const int16_t kOctave = 12 * 128;
static const int16_t kPitchTableStart = 116 * 128;

// ----------------------------------------------------------------------------
class Lfo {
 public:
  void Init() { phase_ = 0; phase_increment_ = 0; looped_ = 0; value_ = 0; }
  // wavetable == nullptr disables the wavetable LFO shapes (voicecard side).
  uint8_t Render(uint8_t shape, Rng* rng, const uint8_t* wavetable) {
    phase_ += phase_increment_;
    looped_ = phase_ < phase_increment_;
    uint8_t value;
    switch (shape) {
      case LFO_WAVEFORM_RAMP: value = phase_ >> 8; break;
      case LFO_WAVEFORM_S_H:
        if (looped_) value_ = rng->GetByte();
        value = value_;
        break;
      case LFO_WAVEFORM_TRIANGLE:
        value = (phase_ & 0x8000) ? static_cast<uint8_t>(phase_ >> 7)
                                  : static_cast<uint8_t>(~static_cast<uint8_t>(phase_ >> 7));
        break;
      case LFO_WAVEFORM_SQUARE: value = (phase_ & 0x8000) ? 255 : 0; break;
      default:
        if (wavetable) {
          uint8_t shape_offset = shape - LFO_WAVEFORM_WAVE_1;
          uint16_t offset = (static_cast<uint16_t>(shape_offset) << 8) + shape_offset;
          value = InterpolateSample(wavetable + offset, phase_);
        } else {
          value = (phase_ & 0x8000) ? static_cast<uint8_t>(phase_ >> 7)
                                    : static_cast<uint8_t>(~static_cast<uint8_t>(phase_ >> 7));
        }
        break;
    }
    return value;
  }
  void set_phase(uint16_t phase) { looped_ = phase <= phase_; phase_ = phase; }
  void set_phase_increment(uint16_t inc) { phase_increment_ = inc; }
  uint8_t looped() const { return looped_; }
 private:
  uint16_t phase_increment_ = 0;
  uint16_t phase_ = 0;
  uint8_t looped_ = 0;
  uint8_t value_ = 0;
};

// ----------------------------------------------------------------------------
enum EnvelopeStage { ATTACK = 0, DECAY = 1, SUSTAIN = 2, RELEASE = 3, DEAD = 4, NUM_SEGMENTS };

class Envelope {
 public:
  void Init() {
    std::memset(this, 0, sizeof(*this));
    stage_target_[ATTACK] = 255;
    stage_target_[RELEASE] = 0;
    stage_target_[DEAD] = 0;
    stage_phase_increment_[SUSTAIN] = 0;
    stage_phase_increment_[DEAD] = 0;
    stage_ = DEAD;
  }
  uint8_t stage() const { return stage_; }
  void Trigger(uint8_t stage) {
    if (stage == DEAD) value_ = 0;
    a_ = value_ >> 8;
    b_ = stage_target_[stage];
    stage_ = stage;
    phase_ = 0;
    phase_increment_ = stage_phase_increment_[stage];
  }
  void Update(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release) {
    stage_phase_increment_[ATTACK] = vc::lut_res_env_portamento_increments[attack];
    stage_phase_increment_[DECAY] = vc::lut_res_env_portamento_increments[decay];
    stage_phase_increment_[RELEASE] = vc::lut_res_env_portamento_increments[release];
    stage_target_[DECAY] = sustain << 1;
    stage_target_[SUSTAIN] = stage_target_[DECAY];
  }
  uint8_t Render() {
    phase_ += phase_increment_;
    if (phase_ < phase_increment_) {
      value_ = U8MixU16(a_, b_, 255);
      Trigger(++stage_);
    }
    if (phase_increment_) {
      uint8_t step = InterpolateSample(vc::wav_res_env_expo, phase_);
      value_ = U8MixU16(a_, b_, step);
    }
    return value_ >> 8;
  }
 private:
  uint16_t stage_phase_increment_[NUM_SEGMENTS];
  uint8_t stage_target_[NUM_SEGMENTS];
  uint8_t stage_;
  uint8_t a_, b_;
  uint16_t phase_increment_;
  uint16_t phase_;
  uint16_t value_;
};

// ----------------------------------------------------------------------------
struct VowelSynthesizerState {
  uint16_t formant_increment[3];
  uint16_t formant_phase[3];
  uint8_t formant_amplitude[3];
  uint8_t noise_modulation;
  uint8_t update;
};
struct FilteredNoiseState { uint8_t lp_noise_sample; uint16_t rng_state; uint16_t rng_reset_value; };
struct QuadSawPadState { uint16_t phase[3]; };
union OscillatorState {
  VowelSynthesizerState vw;
  FilteredNoiseState no;
  QuadSawPadState qs;
  uint16_t secondary_phase;
};

class Oscillator {
 public:
  void Init(Rng* rng) {
    std::memset(&data_, 0, sizeof(data_));
    phase_.integral = 0; phase_.fractional = 0;
    rng_ = rng;
    parameter_ = fm_parameter_ = note_ = shape_ = 0;
  }
  void Reset() { data_.no.rng_reset_value = rng_->GetByte() + 1; }
  void Render(uint8_t shape, uint8_t note, uint24_t increment,
              uint8_t* sync_input, uint8_t* sync_output, uint8_t* buffer);
  void set_parameter(uint8_t p) { parameter_ = p; }
  void set_fm_parameter(uint8_t p) { fm_parameter_ = p; }

 private:
  void RenderSilence(uint8_t* buffer);
  void RenderBandlimitedPwm(uint8_t* buffer);
  void RenderSimpleWavetable(uint8_t* buffer);
  void RenderCzSaw(uint8_t* buffer);
  void RenderCzResoSaw(uint8_t* buffer);
  void RenderCzResoPulse(uint8_t* buffer);
  void RenderCzResoTri(uint8_t* buffer);
  void RenderFm(uint8_t* buffer);
  void Render8BitLand(uint8_t* buffer);
  void RenderVowel(uint8_t* buffer);
  void RenderDirtyPwm(uint8_t* buffer);
  void RenderQuadSawPad(uint8_t* buffer);
  void RenderFilteredNoise(uint8_t* buffer);
  void RenderInterpolatedWavetable(uint8_t* buffer);
  void RenderWavequence(uint8_t* buffer);

  uint24_t phase_;
  uint24_t phase_increment_;
  uint8_t shape_;
  uint8_t parameter_;
  uint8_t fm_parameter_;
  uint8_t note_;
  OscillatorState data_;
  uint8_t* sync_input_;
  uint8_t* sync_output_;
  Rng* rng_;
};

// ----------------------------------------------------------------------------
class SubOscillator {
 public:
  void Init() { phase_ = {0, 0}; phase_increment_ = {0, 0}; }
  void set_increment(uint24_t inc) { phase_increment_ = inc; }
  void Render(uint8_t shape, uint8_t* buffer, uint8_t amount);
 private:
  uint24_t phase_;
  uint24_t phase_increment_;
};

class TransientGenerator {
 public:
  void Init() { rng_state_ = decimate_ = gain_ = counter_ = 0; }
  void Render(uint8_t shape, uint8_t* buffer, uint8_t amount);
  void Trigger() { counter_ = 255; }
 private:
  uint8_t RenderClick();
  uint8_t RenderGlitch();
  uint8_t RenderBlow();
  uint8_t RenderMetallic();
  uint8_t RenderPop();
  uint8_t rng_state_, decimate_, gain_, counter_;
};

// ----------------------------------------------------------------------------
// The voicecard: receives commands, renders blocks of 40 8-bit samples at
// 39.2 kHz and exposes the analog control voltages (cutoff, resonance, VCA).
class Voicecard {
 public:
  void Init(uint16_t seed);
  void Trigger(uint16_t note, uint8_t velocity, uint8_t legato);
  void Release();
  void Kill() { TriggerEnvelope(DEAD); }
  void TriggerEnvelope(uint8_t stage);
  void TriggerEnvelope(uint8_t index, uint8_t stage) { envelope_[index].Trigger(stage); }
  void ResetAllControllers();

  // Renders kAudioBlockSize samples into out.
  void ProcessBlock(uint8_t* out);

  uint8_t cutoff() const { return static_cast<uint8_t>(modulation_destinations_[MOD_DST_FILTER_CUTOFF]); }
  uint8_t resonance() const { return static_cast<uint8_t>(modulation_destinations_[MOD_DST_FILTER_RESONANCE]); }
  uint8_t vca() const { return static_cast<uint8_t>(modulation_destinations_[MOD_DST_VCA]); }
  uint8_t crush() const { return static_cast<uint8_t>(modulation_destinations_[MOD_DST_MIX_CRUSH]); }
  uint8_t filter_mode() const { return patch_.filter[0].mode; }
  bool gate() const { return gate_ != 0; }
  bool silent() const { return envelope_[0].stage() == DEAD && envelope_[1].stage() == DEAD && envelope_[2].stage() == DEAD; }

  void set_modulation_source(uint8_t i, uint8_t value) { modulation_sources_[i] = value; }
  uint8_t modulation_source(uint8_t i) const { return modulation_sources_[i]; }
  Patch* mutable_patch() { return &patch_; }
  VoicePartSettings* mutable_part() { return &part_; }

 private:
  void LoadSources();
  void ProcessModulationMatrix();
  void UpdateDestinations();
  void RenderOscillators();

  Patch patch_;
  VoicePartSettings part_;
  Rng rng_;
  Oscillator osc_1_, osc_2_;
  SubOscillator sub_osc_;
  TransientGenerator transient_generator_;
  Envelope envelope_[kNumEnvelopes];
  uint8_t gate_;
  Lfo voice_lfo_;
  uint8_t modulation_sources_[MOD_SRC_LAST];
  int8_t modulation_destinations_[MOD_DST_LAST];
  int16_t dst_[MOD_DST_LAST];
  uint8_t ops_[9];
  int16_t pitch_increment_, pitch_target_, pitch_value_;
  uint8_t buffer_[kAudioBlockSize];
  uint8_t osc2_buffer_[kAudioBlockSize];
  uint8_t sync_state_[kAudioBlockSize];
  uint8_t no_sync_[kAudioBlockSize];
  uint8_t dummy_sync_state_[kAudioBlockSize];
};

}  // namespace ambika
