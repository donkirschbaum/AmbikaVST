// Ambika VST - analog section of a voicecard (DAC, VCF, VCA, output stage).
//
// The digital part of the voicecard is an exact port of the firmware; this
// part is a model. Three filter boards are emulated, following the Ambika
// voicecard schematics:
//   SMR4 : 4-pole low-pass, cascade of LM13700 OTA stages, OTA VCA.
//   4P   : 4-pole multimode (SSM2164 integrators), LP4 / BP / HP by pole mixing.
//   SVF  : 2-pole state variable (SSM2164), LP / BP / HP via 4052 multiplexer.
//
// The voicecard's 12-bit DAC runs at 39.2 kHz (zero-order hold). The analog
// models run at 2x that rate (78.4 kHz): each DAC sample is held for two
// model samples, which is exactly what the DAC's ZOH does.
//
// GPL v3.
#pragma once

#include <cmath>
#include <cstdint>

namespace ambika {

enum FilterBoard { BOARD_SMR4 = 0, BOARD_4P = 1, BOARD_SVF = 2, BOARD_LAST };

struct CardTolerances {
  float cutoff_ratio = 1.0f;   // multiplies cutoff frequency
  float resonance_gain = 1.0f; // multiplies resonance amount
  float vca_gain = 1.0f;
  float offset = 0.0f;         // DC offset at the output (removed by coupling cap)
};

class AnalogVoice {
 public:
  void Init(float model_sample_rate, const CardTolerances& tol, uint32_t noise_seed);
  void Reset();

  // Control voltages from the voicecard MCU (8-bit PWM / DAC values).
  void SetControls(uint8_t cutoff, uint8_t resonance, uint8_t vca, uint8_t mode,
                   uint8_t crush) {
    cutoff_target_ = cutoff;
    resonance_target_ = resonance;
    vca_target_ = vca;
    mode_ = mode;
    crush_ = crush ? crush : 1;
  }
  void set_board(int b) { board_ = b; }
  void set_noise(float n) { noise_amount_ = n; }
  void set_drive(float d) { drive_ = d; }

  // Process one block of 8-bit DAC samples (at 39.2 kHz). Writes 2*n samples.
  void Process(const uint8_t* in, int n, float* out);

  bool quiet() const { return quiet_; }

 private:
  float FilterSample(float x, float cutoff_cv, float res_cv);
  float CutoffToG(float cv);

  float fs_ = 78431.37f;
  CardTolerances tol_;
  int board_ = BOARD_SMR4;
  uint8_t mode_ = 0;
  uint8_t crush_ = 1;
  uint8_t crush_counter_ = 0;
  float dac_ = 0.0f;

  float cutoff_target_ = 0, resonance_target_ = 0, vca_target_ = 0;
  float cutoff_cv_ = 0, resonance_cv_ = 0, vca_cv_ = 0;
  float cv_coef_ = 0, vca_coef_ = 0;

  // Ladder states.
  float s_[4] = {0, 0, 0, 0};
  // SVF states.
  float ic1_ = 0, ic2_ = 0;

  // Output coupling high-pass.
  float hp_x_ = 0, hp_y_ = 0, hp_coef_ = 0.9995f;
  uint32_t noise_state_ = 1;
  float noise_amount_ = 0.0f;
  float drive_ = 1.0f;
  bool quiet_ = true;
  int quiet_count_ = 0;

  // Cached tan() for the cutoff (recomputed only when the CV moves).
  float last_cv_ = -1.0f, last_g_ = 0.0f;
};

// Windowed-sinc arbitrary ratio resampler (model rate -> host rate), stereo.
class Resampler {
 public:
  void Init(double in_rate, double out_rate);
  // Push one input frame.
  void Push(float l, float r) {
    buf_l_[write_ & kMask] = l;
    buf_r_[write_ & kMask] = r;
    ++write_;
  }
  // Number of input frames needed before the next output can be produced.
  bool CanPop() const { return static_cast<double>(write_) > pos_ + kHalf + 1; }
  void Pop(float* l, float* r);
 private:
  static const int kHalf = 16;          // taps on each side
  static const int kPhases = 256;
  static const int kSize = 4096;
  static const int kMask = kSize - 1;
  float table_[kPhases + 1][2 * kHalf];
  float buf_l_[kSize];
  float buf_r_[kSize];
  uint64_t write_ = 0;
  double pos_ = 0.0;     // read position, in input frames
  double step_ = 1.0;
};

}  // namespace ambika
