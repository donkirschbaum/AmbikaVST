// Ambika VST - voicecard engine.
// Port of the Ambika voicecard firmware, Copyright 2011-2012 Emilie Gillet.
// GPL v3.

#include "voicecard.h"

namespace ambika {

using namespace vc;

static inline uint8_t ReadSample(const uint8_t* table, uint16_t phase) {
  return table[phase >> 8];
}

static inline uint8_t InterpolateTwoTables(const uint8_t* a, const uint8_t* b,
                                           uint16_t phase, uint8_t ga, uint8_t gb) {
  return U8Mix(InterpolateSample(a, phase), InterpolateSample(b, phase), ga, gb);
}

static const uint8_t kNumZonesFullSampleRate = 6;
static const uint8_t kNumZonesHalfSampleRate = 5;

// ---------------------------------------------------------------------------
// Oscillator
// ---------------------------------------------------------------------------

#define UPDATE_PHASE \
  if (*sync_input_++) { phase.integral = 0; phase.fractional = 0; } \
  phase = U24AddC(phase, phase_increment_int); \
  *sync_output_++ = phase.carry;

#define UPDATE_PHASE_MORE_REGISTERS \
  if (*sync_input++) { phase.integral = 0; phase.fractional = 0; } \
  phase = U24AddC(phase, phase_increment_int); \
  *sync_output++ = phase.carry;

#define BEGIN_SAMPLE_LOOP \
  uint24c_t phase; \
  uint24_t phase_increment_int; \
  phase_increment_int.integral = phase_increment_.integral; \
  phase_increment_int.fractional = phase_increment_.fractional; \
  phase.integral = phase_.integral; \
  phase.fractional = phase_.fractional; \
  phase.carry = 0; \
  uint8_t size = kAudioBlockSize; \
  uint8_t* sync_input = sync_input_; \
  uint8_t* sync_output = sync_output_; \
  (void)sync_input; (void)sync_output; \
  while (size--) {

#define END_SAMPLE_LOOP \
  } \
  phase_.integral = phase.integral; \
  phase_.fractional = phase.fractional;

void Oscillator::Render(uint8_t shape, uint8_t note, uint24_t increment,
                        uint8_t* sync_input, uint8_t* sync_output,
                        uint8_t* buffer) {
  shape_ = shape;
  note_ = note;
  phase_increment_ = increment;
  sync_input_ = sync_input;
  sync_output_ = sync_output;
  if (shape_ == WAVEFORM_SQUARE) {
    if (parameter_ == 0) RenderSimpleWavetable(buffer);
    else RenderBandlimitedPwm(buffer);
    return;
  }
  if (shape_ >= WAVEFORM_WAVEQUENCE) { RenderWavequence(buffer); return; }
  if (shape_ >= WAVEFORM_WAVETABLE_1) { RenderInterpolatedWavetable(buffer); return; }
  switch (shape_) {
    case WAVEFORM_NONE: RenderSilence(buffer); break;
    case WAVEFORM_SAW:
    case WAVEFORM_TRIANGLE:
    case WAVEFORM_SINE: RenderSimpleWavetable(buffer); break;
    case WAVEFORM_CZ_SAW: RenderCzSaw(buffer); break;
    case WAVEFORM_CZ_SAW_LP: case WAVEFORM_CZ_SAW_PK:
    case WAVEFORM_CZ_SAW_BP: case WAVEFORM_CZ_SAW_HP: RenderCzResoSaw(buffer); break;
    case WAVEFORM_CZ_PLS_LP: case WAVEFORM_CZ_PLS_PK:
    case WAVEFORM_CZ_PLS_BP: case WAVEFORM_CZ_PLS_HP: RenderCzResoPulse(buffer); break;
    case WAVEFORM_CZ_TRI_LP: RenderCzResoTri(buffer); break;
    case WAVEFORM_QUAD_SAW_PAD: RenderQuadSawPad(buffer); break;
    case WAVEFORM_FM: RenderFm(buffer); break;
    case WAVEFORM_8BITLAND: Render8BitLand(buffer); break;
    case WAVEFORM_DIRTY_PWM: RenderDirtyPwm(buffer); break;
    case WAVEFORM_FILTERED_NOISE: RenderFilteredNoise(buffer); break;
    case WAVEFORM_VOWEL: RenderVowel(buffer); break;
    default: RenderSilence(buffer); break;
  }
}

void Oscillator::RenderSilence(uint8_t* buffer) {
  std::memset(buffer, 128, kAudioBlockSize);
}

void Oscillator::RenderBandlimitedPwm(uint8_t* buffer) {
  uint8_t balance_index = U8Swap4(note_);
  uint8_t gain_2 = balance_index & 0xf0;
  uint8_t gain_1 = ~gain_2;
  uint8_t wave_index = balance_index & 0xf;
  const uint8_t* wave_1 = waveform_table[WAV_RES_BANDLIMITED_SAW_1 + wave_index];
  wave_index = U8AddClip(wave_index, 1, kNumZonesHalfSampleRate);
  const uint8_t* wave_2 = waveform_table[WAV_RES_BANDLIMITED_SAW_1 + wave_index];

  uint16_t shift = static_cast<uint16_t>(parameter_ + 128) << 8;
  uint8_t scale = 192 - (parameter_ >> 1);
  if (note_ > 52) {
    scale = U8Mix(scale, 102, (note_ - 52) << 2);
    scale = U8Mix(scale, 102, (note_ - 52) << 2);
  }
  phase_increment_ = U24ShiftLeft(phase_increment_);
  BEGIN_SAMPLE_LOOP
    phase = U24AddC(phase, phase_increment_int);
    *sync_output_++ = phase.carry;
    *sync_output_++ = 0;
    if (sync_input_[0] || sync_input_[1]) { phase.integral = 0; phase.fractional = 0; }
    sync_input_ += 2;
    uint8_t a = InterpolateTwoTables(wave_1, wave_2, phase.integral, gain_1, gain_2);
    a = U8U8MulShift8(a, scale);
    uint8_t b = InterpolateTwoTables(wave_1, wave_2, phase.integral + shift, gain_1, gain_2);
    b = U8U8MulShift8(b, scale);
    a = a - b + 128;
    *buffer++ = a;
    *buffer++ = a;
    size--;
  END_SAMPLE_LOOP
}

void Oscillator::RenderSimpleWavetable(uint8_t* buffer) {
  uint8_t balance_index = U8Swap4(note_);
  uint8_t gain_2 = balance_index & 0xf0;
  uint8_t gain_1 = ~gain_2;
  uint8_t wave_1_index, wave_2_index;
  if (shape_ != WAVEFORM_SINE) {
    uint8_t wave_index = balance_index & 0xf;
    uint8_t base = shape_ == WAVEFORM_SAW ? WAV_RES_BANDLIMITED_SAW_0
        : (shape_ == WAVEFORM_SQUARE ? WAV_RES_BANDLIMITED_SQUARE_0 : WAV_RES_BANDLIMITED_TRIANGLE_0);
    wave_1_index = base + wave_index;
    wave_index = U8AddClip(wave_index, 1, kNumZonesFullSampleRate);
    wave_2_index = base + wave_index;
  } else {
    wave_1_index = WAV_RES_SINE;
    wave_2_index = WAV_RES_SINE;
  }
  const uint8_t* wave_1 = waveform_table[wave_1_index];
  const uint8_t* wave_2 = waveform_table[wave_2_index];
  if (shape_ != WAVEFORM_TRIANGLE) {
    BEGIN_SAMPLE_LOOP
      UPDATE_PHASE_MORE_REGISTERS
      uint8_t sample = InterpolateTwoTables(wave_1, wave_2, phase.integral, gain_1, gain_2);
      if (sample < parameter_) sample += parameter_ >> 1;
      *buffer++ = sample;
    END_SAMPLE_LOOP
  } else {
    BEGIN_SAMPLE_LOOP
      UPDATE_PHASE_MORE_REGISTERS
      uint8_t sample = InterpolateTwoTables(wave_1, wave_2, phase.integral, gain_1, gain_2);
      if (sample < parameter_) sample = parameter_;
      *buffer++ = sample;
    END_SAMPLE_LOOP
  }
}

void Oscillator::RenderCzSaw(uint8_t* buffer) {
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    uint8_t phi = phase.integral >> 8;
    uint8_t clipped_phi = phi < 0x20 ? phi << 3 : 0xff;
    *buffer++ = ReadSample(wav_res_sine, U8MixU16(phi, clipped_phi, parameter_ << 1));
  END_SAMPLE_LOOP
}

void Oscillator::RenderCzResoSaw(uint8_t* buffer) {
  uint16_t increment = phase_increment_.integral +
      ((phase_increment_.integral * uint32_t(parameter_)) >> 2);
  uint8_t type = shape_ - WAVEFORM_CZ_SAW_LP;
  uint16_t phase_2 = data_.secondary_phase;
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    if (phase.carry) phase_2 = lut_res_cz_phase_reset[type & 0x03];
    phase_2 += increment;
    uint8_t carrier = ReadSample(wav_res_sine, phase_2);
    uint8_t window = ~(phase.integral >> 8);
    if (type & 2) *buffer++ = S8U8MulShift8(carrier + 128, window) + 128;
    else *buffer++ = U8U8MulShift8(carrier, window);
  END_SAMPLE_LOOP
  data_.secondary_phase = phase_2;
}

void Oscillator::RenderCzResoPulse(uint8_t* buffer) {
  uint16_t increment = phase_increment_.integral +
      ((phase_increment_.integral * uint32_t(parameter_)) >> 2);
  uint8_t type = shape_ - WAVEFORM_CZ_SAW_LP;
  uint16_t phase_2 = data_.secondary_phase;
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    if (phase.carry) phase_2 = lut_res_cz_phase_reset[type & 0x03];
    phase_2 += increment;
    uint8_t carrier = ReadSample(wav_res_sine, phase_2);
    uint8_t window = 0;
    if (phase.integral < 0x4000) window = 255;
    else if (phase.integral < 0x8000) window = static_cast<uint8_t>(static_cast<uint16_t>(~(phase.integral - 0x4000)) >> 6);
    if (type == 5) { carrier >>= 1; carrier += 128; }
    if (type & 2) *buffer++ = S8U8MulShift8(carrier + 128, window) + 128;
    else *buffer++ = U8U8MulShift8(carrier, window);
  END_SAMPLE_LOOP
  data_.secondary_phase = phase_2;
}

void Oscillator::RenderCzResoTri(uint8_t* buffer) {
  uint16_t increment = phase_increment_.integral +
      ((phase_increment_.integral * uint32_t(parameter_)) >> 2);
  uint8_t type = shape_ - WAVEFORM_CZ_SAW_LP;
  uint16_t phase_2 = data_.secondary_phase;
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    if (phase.carry) phase_2 = lut_res_cz_phase_reset[type & 0x03];
    phase_2 += increment;
    uint8_t carrier = ReadSample(wav_res_sine, phase_2);
    uint8_t window = (phase.integral & 0x8000)
        ? static_cast<uint8_t>(~static_cast<uint8_t>(phase.integral >> 7))
        : static_cast<uint8_t>(phase.integral >> 7);
    if (type & 2) *buffer++ = S8U8MulShift8(carrier + 128, window) + 128;
    else *buffer++ = U8U8MulShift8(carrier, window);
  END_SAMPLE_LOOP
  data_.secondary_phase = phase_2;
}

void Oscillator::RenderFm(uint8_t* buffer) {
  uint8_t offset = fm_parameter_;
  if (offset < 24) offset = 0;
  else if (offset > 48) offset = 24;
  else offset = offset - 24;
  uint16_t multiplier = lut_res_fm_frequency_ratios[offset];
  uint16_t increment = static_cast<uint16_t>(
      (static_cast<int32_t>(phase_increment_.integral) * multiplier) >> 8);
  parameter_ <<= 1;
  uint16_t phase_2 = data_.secondary_phase;
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    phase_2 += increment;
    uint8_t modulator = InterpolateSample(wav_res_sine, phase_2);
    uint16_t modulation = static_cast<uint16_t>(modulator * parameter_);
    *buffer++ = InterpolateSample(wav_res_sine, static_cast<uint16_t>(phase.integral + modulation));
  END_SAMPLE_LOOP
  data_.secondary_phase = phase_2;
}

void Oscillator::Render8BitLand(uint8_t* buffer) {
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    uint8_t x = parameter_;
    *buffer++ = static_cast<uint8_t>((((phase.integral >> 8) ^ (x << 1)) & (~x)) + (x >> 1));
  END_SAMPLE_LOOP
}

void Oscillator::RenderVowel(uint8_t* buffer) {
  data_.vw.update = (data_.vw.update + 1) & 3;
  if (!data_.vw.update) {
    uint8_t offset_1 = U8ShiftRight4(parameter_);
    offset_1 = static_cast<uint8_t>(U8U8Mul(offset_1, 7));
    uint8_t offset_2 = offset_1 + 7;
    uint8_t balance = parameter_ & 15;
    for (uint8_t i = 0; i < 3; ++i) {
      data_.vw.formant_increment[i] = U8U4MixU12(
          wav_res_vowel_data[offset_1 + i], wav_res_vowel_data[offset_2 + i], balance);
      data_.vw.formant_increment[i] <<= 3;
    }
    // formant_amplitude[3] aliases noise_modulation.
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t amplitude_a = wav_res_vowel_data[offset_1 + 3 + i];
      uint8_t amplitude_b = wav_res_vowel_data[offset_2 + 3 + i];
      uint8_t v = U8U4MixU8(amplitude_a, amplitude_b, balance);
      if (i < 3) data_.vw.formant_amplitude[i] = v;
      else data_.vw.noise_modulation = v;
    }
  }
  int16_t phase_noise = S8S8Mul(static_cast<int8_t>(rng_->state_msb()),
                                static_cast<int8_t>(data_.vw.noise_modulation));
  BEGIN_SAMPLE_LOOP
    int8_t result = 0;
    uint8_t phaselet;
    data_.vw.formant_phase[0] += data_.vw.formant_increment[0];
    phaselet = (data_.vw.formant_phase[0] >> 8) & 0xf0;
    result = static_cast<int8_t>(wav_res_formant_sine[phaselet | data_.vw.formant_amplitude[0]]);
    data_.vw.formant_phase[1] += data_.vw.formant_increment[1];
    phaselet = (data_.vw.formant_phase[1] >> 8) & 0xf0;
    result += static_cast<int8_t>(wav_res_formant_sine[phaselet | data_.vw.formant_amplitude[1]]);
    data_.vw.formant_phase[2] += data_.vw.formant_increment[2];
    phaselet = (data_.vw.formant_phase[2] >> 8) & 0xf0;
    result += static_cast<int8_t>(wav_res_formant_square[phaselet | data_.vw.formant_amplitude[2]]);
    result = S8U8MulShift8(result, phase.integral >> 8);
    phase.integral -= phase_increment_int.integral;
    // AVR ints are 16 bits wide: the sum wraps and is compared unsigned.
    if (static_cast<uint16_t>(phase.integral + phase_noise) < phase_increment_int.integral) {
      data_.vw.formant_phase[0] = 0;
      data_.vw.formant_phase[1] = 0;
      data_.vw.formant_phase[2] = 0;
    }
    uint8_t x = S16ClipS8(4 * result) + 128;
    *buffer++ = x;
    *buffer++ = x;
    size--;
  END_SAMPLE_LOOP
}

void Oscillator::RenderDirtyPwm(uint8_t* buffer) {
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    *buffer++ = (phase.integral >> 8) < 127 + parameter_ ? 0 : 255;
  END_SAMPLE_LOOP
}

void Oscillator::RenderQuadSawPad(uint8_t* buffer) {
  uint16_t phase_spread = static_cast<uint16_t>(
      (static_cast<uint32_t>(phase_increment_.integral) * parameter_) >> 13);
  ++phase_spread;
  uint16_t phase_increment = phase_increment_.integral;
  uint16_t increments[3];
  for (uint8_t i = 0; i < 3; ++i) {
    phase_increment += phase_spread;
    increments[i] = phase_increment;
  }
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    data_.qs.phase[0] += increments[0];
    data_.qs.phase[1] += increments[1];
    data_.qs.phase[2] += increments[2];
    uint8_t value = (phase.integral >> 10);
    value += (data_.qs.phase[0] >> 10);
    value += (data_.qs.phase[1] >> 10);
    value += (data_.qs.phase[2] >> 10);
    *buffer++ = value;
  END_SAMPLE_LOOP
}

void Oscillator::RenderFilteredNoise(uint8_t* buffer) {
  uint16_t rng_state = data_.no.rng_state;
  if (rng_state == 0) ++rng_state;
  uint8_t filter_coefficient = parameter_ << 2;
  if (filter_coefficient <= 4) filter_coefficient = 4;
  BEGIN_SAMPLE_LOOP
    if (*sync_input_++) rng_state = data_.no.rng_reset_value;
    rng_state = (rng_state >> 1) ^ (-(rng_state & 1) & 0xb400);
    uint8_t noise_sample = rng_state >> 8;
    data_.no.lp_noise_sample = U8Mix(data_.no.lp_noise_sample, noise_sample, filter_coefficient);
    if (parameter_ >= 64) *buffer++ = noise_sample - data_.no.lp_noise_sample - 128;
    else *buffer++ = data_.no.lp_noise_sample;
  END_SAMPLE_LOOP
  data_.no.rng_state = rng_state;
}

void Oscillator::RenderInterpolatedWavetable(uint8_t* buffer) {
  const uint8_t* def = wav_res_wavetables + U8U8Mul(shape_ - WAVEFORM_WAVETABLE_1, 18);
  uint8_t num_steps = def[0];
  uint16_t pointer = U8U8Mul(parameter_ << 1, num_steps);
  uint16_t wave_index_1 = def[1 + (pointer >> 8)];
  uint16_t wave_index_2 = def[2 + (pointer >> 8)];
  uint8_t gain = pointer & 0xff;
  const uint8_t* wave_1 = wav_res_waves + U8U8Mul(static_cast<uint8_t>(wave_index_1), 129);
  const uint8_t* wave_2 = wav_res_waves + U8U8Mul(static_cast<uint8_t>(wave_index_2), 129);
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE_MORE_REGISTERS
    *buffer++ = InterpolateTwoTables(wave_1, wave_2, phase.integral >> 1, ~gain, gain);
  END_SAMPLE_LOOP
}

void Oscillator::RenderWavequence(uint8_t* buffer) {
  const uint8_t* wave = wav_res_waves + U8U8Mul(parameter_, 129);
  BEGIN_SAMPLE_LOOP
    UPDATE_PHASE
    *buffer++ = InterpolateSample(wave, phase.integral >> 1);
  END_SAMPLE_LOOP
}

// ---------------------------------------------------------------------------
// Sub oscillator & transient generator
// ---------------------------------------------------------------------------

void SubOscillator::Render(uint8_t shape, uint8_t* buffer, uint8_t amount) {
  uint24_t increment = phase_increment_;
  if (shape >= 3) { increment = U24ShiftRight(increment); shape -= 3; }
  uint8_t size = kAudioBlockSize;
  uint8_t pulse_width = shape == 0 ? 0x80 : 0x40;
  uint8_t sub_gain = amount;
  uint8_t mix_gain = ~sub_gain;
  while (size--) {
    phase_ = U24Add(phase_, increment);
    uint8_t v;
    if (shape != 1) {
      v = static_cast<uint8_t>(phase_.integral >> 8) < pulse_width ? 0 : 255;
    } else {
      uint8_t tri = phase_.integral >> 7;
      v = phase_.integral & 0x8000 ? tri : static_cast<uint8_t>(~tri);
    }
    *buffer = U8Mix(*buffer, v, mix_gain, sub_gain);
    ++buffer;
  }
}

uint8_t TransientGenerator::RenderClick() { gain_ = counter_; --counter_; return counter_ < 32 ? 255 : 0; }
uint8_t TransientGenerator::RenderGlitch() {
  gain_ = counter_; --counter_;
  rng_state_ = rng_state_ * 73 + counter_;
  return rng_state_;
}
uint8_t TransientGenerator::RenderBlow() {
  decimate_ += 2;
  if (decimate_ >= 16) {
    decimate_ -= 17;
    rng_state_ = rng_state_ * 73 + counter_;
    if (decimate_ == 0) {
      --counter_;
      gain_ = (counter_ & 0x80) ? static_cast<uint8_t>(~counter_) : counter_;
    }
  }
  return rng_state_;
}
uint8_t TransientGenerator::RenderMetallic() {
  --counter_;
  gain_ = counter_ >= 64 ? 255 : static_cast<uint8_t>(counter_ << 2);
  return static_cast<uint8_t>(counter_ * 57);
}
uint8_t TransientGenerator::RenderPop() { --counter_; gain_ = counter_ > 0 ? 255 : 0; return 0; }

void TransientGenerator::Render(uint8_t shape, uint8_t* buffer, uint8_t amount) {
  if (shape < WAVEFORM_SUB_OSC_CLICK) return;
  if (shape > WAVEFORM_SUB_OSC_POP) shape = WAVEFORM_SUB_OSC_POP;
  uint8_t size = kAudioBlockSize;
  while (counter_ && size--) {
    uint8_t value;
    switch (shape) {
      case WAVEFORM_SUB_OSC_CLICK: value = RenderClick(); break;
      case WAVEFORM_SUB_OSC_GLITCH: value = RenderGlitch(); break;
      case WAVEFORM_SUB_OSC_BLOW: value = RenderBlow(); break;
      case WAVEFORM_SUB_OSC_METALLIC: value = RenderMetallic(); break;
      default: value = RenderPop(); break;
    }
    uint8_t amplitude = U8U8MulShift8(gain_, amount);
    *buffer = U8Mix(*buffer, value, amplitude);
    ++buffer;
  }
}

// ---------------------------------------------------------------------------
// Voicecard
// ---------------------------------------------------------------------------

static void LoadVoicecardInitPatch(Patch* p) {
  // The voicecard's own boot patch (voicecard/voice.cc). It is overwritten by
  // the controller as soon as a part is assigned.
  static const uint8_t init[112] = {
    WAVEFORM_NONE, 0, 0, 0, WAVEFORM_NONE, 0, 0, 0,
    32, OP_SUM, 0, WAVEFORM_SUB_OSC_SQUARE_1, 0, 0, 0, 0,
    127, 0, 0, 0, 0, 0, 63, 0,
    0, 40, 20, 60, 0, 0, 0, 0,
    0, 40, 20, 60, 0, 0, 0, 0,
    0, 40, 20, 60, 0, 0, 0, 0,
    LFO_WAVEFORM_TRIANGLE, 16,
    MOD_SRC_LFO_1, MOD_DST_OSC_1, 0, MOD_SRC_ENV_1, MOD_DST_OSC_2, 0,
    MOD_SRC_LFO_1, MOD_DST_OSC_1, 0, MOD_SRC_ENV_1, MOD_DST_OSC_2, 0,
    MOD_SRC_ENV_1, MOD_DST_PARAMETER_1, 0, MOD_SRC_LFO_1, MOD_DST_PARAMETER_2, 0,
    MOD_SRC_LFO_2, MOD_DST_MIX_BALANCE, 0, MOD_SRC_LFO_4, MOD_DST_PARAMETER_1, 63,
    MOD_SRC_SEQ_1, MOD_DST_PARAMETER_1, 0, MOD_SRC_SEQ_2, MOD_DST_PARAMETER_2, 0,
    MOD_SRC_ENV_2, MOD_DST_VCA, 32, MOD_SRC_VELOCITY, MOD_DST_VCA, 0,
    MOD_SRC_PITCH_BEND, MOD_DST_OSC_1_2_COARSE, 0, MOD_SRC_LFO_1, MOD_DST_OSC_1_2_COARSE, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
  };
  std::memcpy(p, init, sizeof(Patch));
}

void Voicecard::Init(uint16_t seed) {
  rng_.Seed(seed);
  osc_1_.Init(&rng_);
  osc_2_.Init(&rng_);
  sub_osc_.Init();
  transient_generator_.Init();
  voice_lfo_.Init();
  std::memset(modulation_sources_, 0, sizeof(modulation_sources_));
  std::memset(modulation_destinations_, 0, sizeof(modulation_destinations_));
  std::memset(dst_, 0, sizeof(dst_));
  std::memset(ops_, 0, sizeof(ops_));
  std::memset(buffer_, 128, sizeof(buffer_));
  std::memset(osc2_buffer_, 128, sizeof(osc2_buffer_));
  std::memset(sync_state_, 0, sizeof(sync_state_));
  std::memset(dummy_sync_state_, 0, sizeof(dummy_sync_state_));
  pitch_value_ = 0;
  pitch_target_ = 0;
  pitch_increment_ = 0;
  gate_ = 0;
  for (uint8_t i = 0; i < kNumEnvelopes; ++i) envelope_[i].Init();
  std::memset(no_sync_, 0, kAudioBlockSize);
  LoadVoicecardInitPatch(&patch_);
  ResetAllControllers();
  part_.volume = 127;
  part_.portamento_time = 0;
  part_.legato = 0;
  Kill();
}

void Voicecard::ResetAllControllers() {
  modulation_sources_[MOD_SRC_PITCH_BEND] = 128;
  modulation_sources_[MOD_SRC_AFTERTOUCH] = 0;
  modulation_sources_[MOD_SRC_WHEEL] = 0;
  modulation_sources_[MOD_SRC_WHEEL_2] = 0;
  modulation_sources_[MOD_SRC_EXPRESSION] = 0;
  modulation_sources_[MOD_SRC_CONSTANT_4] = 4;
  modulation_sources_[MOD_SRC_CONSTANT_8] = 8;
  modulation_sources_[MOD_SRC_CONSTANT_16] = 16;
  modulation_sources_[MOD_SRC_CONSTANT_32] = 32;
  modulation_sources_[MOD_SRC_CONSTANT_64] = 64;
  modulation_sources_[MOD_SRC_CONSTANT_128] = 128;
  modulation_sources_[MOD_SRC_CONSTANT_256] = 255;
}

void Voicecard::TriggerEnvelope(uint8_t stage) {
  for (uint8_t i = 0; i < kNumEnvelopes; ++i) TriggerEnvelope(i, stage);
}

void Voicecard::Trigger(uint16_t note, uint8_t velocity, uint8_t legato) {
  pitch_target_ = note;
  if (!part_.legato || !legato) {
    gate_ = 255;
    TriggerEnvelope(ATTACK);
    transient_generator_.Trigger();
    modulation_sources_[MOD_SRC_VELOCITY] = velocity;
    modulation_sources_[MOD_SRC_RANDOM] = rng_.state_msb();
    osc_2_.Reset();
  }
  if (pitch_value_ == 0 || (part_.legato && !legato)) pitch_value_ = pitch_target_;
  int16_t delta = pitch_target_ - pitch_value_;
  int32_t increment = lut_res_env_portamento_increments[part_.portamento_time];
  pitch_increment_ = static_cast<int16_t>((delta * increment) >> 16);
  if (pitch_increment_ == 0) pitch_increment_ = delta < 0 ? -1 : 1;
}

void Voicecard::Release() {
  gate_ = 0;
  TriggerEnvelope(RELEASE);
}

void Voicecard::LoadSources() {
  modulation_sources_[MOD_SRC_NOISE] = rng_.GetByte();
  modulation_sources_[MOD_SRC_ENV_1] = envelope_[0].Render();
  modulation_sources_[MOD_SRC_ENV_2] = envelope_[1].Render();
  modulation_sources_[MOD_SRC_ENV_3] = envelope_[2].Render();
  modulation_sources_[MOD_SRC_NOTE] = U14ShiftRight6(pitch_value_);
  modulation_sources_[MOD_SRC_GATE] = gate_;
  modulation_sources_[MOD_SRC_LFO_4] = voice_lfo_.Render(patch_.voice_lfo_shape, &rng_, nullptr);

  for (uint8_t i = 0; i < kNumModifiers; ++i) {
    if (!patch_.modifier[i].op) continue;
    uint8_t x = modulation_sources_[patch_.modifier[i].operands[0] % MOD_SRC_LAST];
    uint8_t y = modulation_sources_[patch_.modifier[i].operands[1] % MOD_SRC_LAST];
    uint8_t op = patch_.modifier[i].op;
    if (op <= MODIFIER_LE) {
      if (x > y) { ops_[4] = x; ops_[7] = 255; ops_[5] = y; ops_[8] = 0; }
      else { ops_[4] = y; ops_[7] = 0; ops_[5] = x; ops_[8] = 255; }
      ops_[1] = (x >> 1) + (y >> 1);
      ops_[2] = U8U8MulShift8(x, y);
      ops_[3] = S8U8MulShift8(x + 128, y) + 128;
      ops_[6] = x ^ y;
      modulation_sources_[MOD_SRC_OP_1 + i] = ops_[op];
    } else if (op == MODIFIER_QUANTIZE) {
      uint8_t mask = 0;
      while (y >>= 1) { mask >>= 1; mask |= 0x80; }
      modulation_sources_[MOD_SRC_OP_1 + i] = x & mask;
    } else if (op == MODIFIER_LAG_PROCESSOR) {
      y >>= 2;
      ++y;
      uint16_t v = U8U8Mul(256 - y, modulation_sources_[MOD_SRC_OP_1 + i]);
      v += U8U8Mul(y, x);
      modulation_sources_[MOD_SRC_OP_1 + i] = v >> 8;
    }
  }

  modulation_destinations_[MOD_DST_VCA] = static_cast<int8_t>(part_.volume << 1);

  dst_[MOD_DST_OSC_1] = dst_[MOD_DST_OSC_2] = 8192;
  dst_[MOD_DST_OSC_1_2_COARSE] = dst_[MOD_DST_OSC_1_2_FINE] = 8192;
  dst_[MOD_DST_PARAMETER_1] = U8U8Mul(patch_.osc[0].parameter, 128);
  dst_[MOD_DST_PARAMETER_2] = U8U8Mul(patch_.osc[1].parameter, 128);
  dst_[MOD_DST_MIX_BALANCE] = patch_.mix_balance << 8;
  dst_[MOD_DST_MIX_PARAM] = patch_.mix_parameter << 8;
  dst_[MOD_DST_MIX_FUZZ] = patch_.mix_fuzz << 8;
  dst_[MOD_DST_MIX_CRUSH] = patch_.mix_crush << 8;
  dst_[MOD_DST_MIX_NOISE] = patch_.mix_noise << 8;
  dst_[MOD_DST_MIX_SUB_OSC] = patch_.mix_sub_osc << 8;
  uint16_t cutoff = U8U8Mul(patch_.filter[0].cutoff, 128);
  dst_[MOD_DST_FILTER_CUTOFF] = S16ClipU14(cutoff + pitch_value_ - 8192);
  dst_[MOD_DST_FILTER_RESONANCE] = patch_.filter[0].resonance << 8;
  dst_[MOD_DST_ATTACK] = 8192;
  dst_[MOD_DST_DECAY] = 8192;
  dst_[MOD_DST_RELEASE] = 8192;
  dst_[MOD_DST_LFO_4] = U8U8Mul(patch_.voice_lfo_rate, 128);
}

void Voicecard::ProcessModulationMatrix() {
  for (uint8_t i = 0; i < kNumModulations; ++i) {
    int8_t amount = patch_.modulation[i].amount;
    if (i == kNumModulations - 1) {
      amount = S8U8MulShift8(amount, modulation_sources_[MOD_SRC_WHEEL]);
    }
    uint8_t source = patch_.modulation[i].source % MOD_SRC_LAST;
    uint8_t destination = patch_.modulation[i].destination % MOD_DST_LAST;
    uint8_t source_value = modulation_sources_[source];
    if (destination != MOD_DST_VCA) {
      int16_t modulation = dst_[destination];
      if ((source >= MOD_SRC_LFO_1 && source <= MOD_SRC_LFO_4) ||
          source == MOD_SRC_PITCH_BEND || source == MOD_SRC_NOTE) {
        modulation += S8S8Mul(amount, static_cast<int8_t>(source_value + 128));
      } else {
        modulation += S8U8Mul(amount, source_value);
      }
      dst_[destination] = S16ClipU14(modulation);
    } else {
      if (amount < 0) { amount = -amount; source_value = 255 - source_value; }
      if (amount != 63) source_value = U8Mix(255, source_value, amount << 2);
      modulation_destinations_[MOD_DST_VCA] = static_cast<int8_t>(U8U8MulShift8(
          static_cast<uint8_t>(modulation_destinations_[MOD_DST_VCA]), source_value));
    }
  }
}

void Voicecard::UpdateDestinations() {
  uint16_t cutoff = dst_[MOD_DST_FILTER_CUTOFF];
  cutoff = S16ClipU14(cutoff + S8U8Mul(patch_.filter_env, modulation_sources_[MOD_SRC_ENV_2]));
  cutoff = S16ClipU14(cutoff + S8S8Mul(patch_.filter_lfo,
                                       static_cast<int8_t>(modulation_sources_[MOD_SRC_LFO_2] + 128)));
  modulation_destinations_[MOD_DST_FILTER_CUTOFF] = static_cast<int8_t>(U14ShiftRight6(cutoff));
  modulation_destinations_[MOD_DST_FILTER_RESONANCE] = static_cast<int8_t>(
      U14ShiftRight6(dst_[MOD_DST_FILTER_RESONANCE]));
  modulation_destinations_[MOD_DST_MIX_CRUSH] = static_cast<int8_t>((dst_[MOD_DST_MIX_CRUSH] >> 8) + 1);

  osc_1_.set_parameter(U15ShiftRight7(dst_[MOD_DST_PARAMETER_1]));
  osc_1_.set_fm_parameter(patch_.osc[0].range + 36);
  osc_2_.set_parameter(U15ShiftRight7(dst_[MOD_DST_PARAMETER_2]));
  osc_2_.set_fm_parameter(patch_.osc[1].range + 36);

  int8_t attack_mod = U15ShiftRight7(dst_[MOD_DST_ATTACK]) - 64;
  int8_t decay_mod = U15ShiftRight7(dst_[MOD_DST_DECAY]) - 64;
  int8_t release_mod = U15ShiftRight7(dst_[MOD_DST_RELEASE]) - 64;
  for (int i = 0; i < kNumEnvelopes; ++i) {
    int16_t a = Clip(patch_.env_lfo[i].attack + attack_mod, 0, 127);
    int16_t d = Clip(patch_.env_lfo[i].decay + decay_mod, 0, 127);
    int16_t r = Clip(patch_.env_lfo[i].release + release_mod, 0, 127);
    envelope_[i].Update(static_cast<uint8_t>(a), static_cast<uint8_t>(d),
                        patch_.env_lfo[i].sustain, static_cast<uint8_t>(r));
  }
  voice_lfo_.set_phase_increment(lut_res_lfo_increments[U14ShiftRight6(dst_[MOD_DST_LFO_4]) >> 1]);
}

void Voicecard::RenderOscillators() {
  int16_t base_pitch = pitch_value_ + pitch_increment_;
  if ((pitch_increment_ > 0) ^ (base_pitch < pitch_target_)) {
    base_pitch = pitch_target_;
    pitch_increment_ = 0;
  }
  pitch_value_ = base_pitch;

  base_pitch += (dst_[MOD_DST_OSC_1_2_COARSE] - 8192) >> 4;
  base_pitch += (dst_[MOD_DST_OSC_1_2_FINE] - 8192) >> 7;

  for (uint8_t i = 0; i < kNumOscillators; ++i) {
    int16_t pitch = base_pitch;
    if (patch_.osc[i].shape != WAVEFORM_FM) pitch += S8U8Mul(patch_.osc[i].range, 128);
    pitch += patch_.osc[i].detune;
    pitch += (dst_[MOD_DST_OSC_1 + i] - 8192) >> 2;
    if (pitch >= kHighestNote) pitch = kHighestNote;
    if (pitch < 0) pitch = 0;

    int16_t ref_pitch = pitch - kPitchTableStart;
    uint8_t num_shifts = 0;
    while (ref_pitch < 0) { ref_pitch += kOctave; ++num_shifts; }
    uint24_t increment;
    increment.integral = lut_res_oscillator_increments[ref_pitch >> 1];
    increment.fractional = 0;
    while (num_shifts--) increment = U24ShiftRight(increment);

    int8_t midi_note = static_cast<int8_t>(U15ShiftRight7(pitch) - 12);
    if (midi_note < 0) midi_note = 0;
    if (i == 0) {
      sub_osc_.set_increment(U24ShiftRight(increment));
      osc_1_.Render(patch_.osc[0].shape % WAVEFORM_LAST, midi_note, increment,
                    no_sync_, sync_state_, buffer_);
    } else {
      osc_2_.Render(patch_.osc[1].shape % WAVEFORM_LAST, midi_note, increment,
                    patch_.mix_op == OP_SYNC ? sync_state_ : no_sync_,
                    dummy_sync_state_, osc2_buffer_);
    }
  }
}

void Voicecard::ProcessBlock(uint8_t* out) {
  LoadSources();
  ProcessModulationMatrix();
  UpdateDestinations();

  if (vca() < 2) {
    std::memset(out, 128, kAudioBlockSize);
    // The hardware keeps updating the RNG through the noise source only.
    return;
  }

  RenderOscillators();
  uint8_t op = patch_.mix_op;
  uint8_t osc_2_gain = U14ShiftRight6(dst_[MOD_DST_MIX_BALANCE]);
  uint8_t osc_1_gain = ~osc_2_gain;
  uint8_t wet_gain = U14ShiftRight6(dst_[MOD_DST_MIX_PARAM]);
  uint8_t dry_gain = ~wet_gain;

  switch (op) {
    case OP_RING_MOD:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        uint8_t mix = U8Mix(buffer_[i], osc2_buffer_[i], osc_1_gain, osc_2_gain);
        uint8_t ring = S8S8MulShift8(static_cast<int8_t>(buffer_[i] + 128),
                                     static_cast<int8_t>(osc2_buffer_[i] + 128)) + 128;
        buffer_[i] = U8Mix(mix, ring, dry_gain, wet_gain);
      }
      break;
    case OP_XOR:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        uint8_t mix = U8Mix(buffer_[i], osc2_buffer_[i], osc_1_gain, osc_2_gain);
        uint8_t xord = buffer_[i] ^ osc2_buffer_[i];
        buffer_[i] = U8Mix(mix, xord, dry_gain, wet_gain);
      }
      break;
    case OP_FOLD:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        uint8_t mix = U8Mix(buffer_[i], osc2_buffer_[i], osc_1_gain, osc_2_gain);
        buffer_[i] = U8Mix(mix, static_cast<uint8_t>(mix + 128), dry_gain, wet_gain);
      }
      break;
    case OP_BITS: {
      wet_gain >>= 5;
      wet_gain = 255 - ((1 << wet_gain) - 1);
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        buffer_[i] = U8Mix(buffer_[i], osc2_buffer_[i], osc_1_gain, osc_2_gain) & wet_gain;
      }
      break;
    }
    default:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        buffer_[i] = U8Mix(buffer_[i], osc2_buffer_[i], osc_1_gain, osc_2_gain);
      }
      break;
  }

  uint8_t sub_gain = U15ShiftRight7(dst_[MOD_DST_MIX_SUB_OSC]);
  if (patch_.mix_sub_osc_shape < WAVEFORM_SUB_OSC_CLICK) {
    sub_osc_.Render(patch_.mix_sub_osc_shape, buffer_, sub_gain);
  } else {
    sub_gain <<= 1;
    transient_generator_.Render(patch_.mix_sub_osc_shape, buffer_, sub_gain);
  }

  uint8_t noise = rng_.state_msb();
  uint8_t noise_gain = U15ShiftRight7(dst_[MOD_DST_MIX_NOISE]);
  uint8_t signal_gain = ~noise_gain;
  wet_gain = U14ShiftRight6(dst_[MOD_DST_MIX_FUZZ]);
  dry_gain = ~wet_gain;
  for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
    noise = (noise * 73) + 1;
    uint8_t s = U8Mix(buffer_[i], noise, signal_gain, noise_gain);
    out[i] = U8Mix(s, wav_res_distortion[s], dry_gain, wet_gain);
  }
}

}  // namespace ambika
