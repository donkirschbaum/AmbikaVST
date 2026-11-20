// Ambika VST - analog section models. GPL v3.

#include "analog.h"

#include <algorithm>

namespace ambika {

static inline float FastTanh(float x) {
  if (x > 3.0f) return 1.0f;
  if (x < -3.0f) return -1.0f;
  float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void AnalogVoice::Init(float model_sample_rate, const CardTolerances& tol,
                       uint32_t noise_seed) {
  fs_ = model_sample_rate;
  tol_ = tol;
  noise_state_ = noise_seed ? noise_seed : 1;
  // PWM CV outputs go through RC low-pass filters on the voicecard.
  cv_coef_ = 1.0f - std::exp(-1.0f / (0.0012f * fs_));
  vca_coef_ = 1.0f - std::exp(-1.0f / (0.0006f * fs_));
  // Output coupling capacitor: ~7 Hz high-pass.
  hp_coef_ = std::exp(-2.0f * 3.14159265f * 7.0f / fs_);
  last_cv_ = -1.0f;
  Reset();
}

void AnalogVoice::Reset() {
  for (float& s : s_) s = 0.0f;
  ic1_ = ic2_ = 0.0f;
  hp_x_ = hp_y_ = 0.0f;
  dac_ = 0.0f;
  crush_counter_ = 0;
  cutoff_cv_ = cutoff_target_;
  resonance_cv_ = resonance_target_;
  vca_cv_ = 0.0f;
}

float AnalogVoice::CutoffToG(float cv) {
  if (std::fabs(cv - last_cv_) < 0.01f) return last_g_;
  last_cv_ = cv;
  // Exponential converter: 2 CV steps per semitone (the firmware adds the
  // note pitch in 1/128 semitones and drops 6 bits), i.e. 1:1 key tracking.
  // CV 0 -> ~11 Hz, CV 255 -> ~17.5 kHz.
  float f = 8.1758f * 1.35f * tol_.cutoff_ratio * std::exp2(cv * (1.0f / 24.0f));
  f = std::min(f, 0.42f * fs_);
  last_g_ = std::tan(3.14159265f * f / fs_);
  return last_g_;
}

float AnalogVoice::FilterSample(float x, float cutoff_cv, float res_cv) {
  const float g = CutoffToG(cutoff_cv);
  const float r = std::min(res_cv * (1.0f / 255.0f) * tol_.resonance_gain, 1.0f);

  if (board_ == BOARD_SVF) {
    // Zero-delay-feedback state variable filter (Simper), with the band-pass
    // integrator saturating like the SSM2164 cell does at high resonance.
    float k = 2.0f * (1.0f - 1.06f * r);
    k = std::max(k, -0.1f);
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = x - ic2_;
    const float v1 = a1 * ic1_ + a2 * v3;
    const float v2 = ic2_ + a2 * ic1_ + a3 * v3;
    ic1_ = 2.0f * v1 - ic1_;
    ic2_ = 2.0f * v2 - ic2_;
    ic1_ = 1.6f * FastTanh(ic1_ * (1.0f / 1.6f));
    ic2_ = 1.8f * FastTanh(ic2_ * (1.0f / 1.8f));
    switch (mode_) {
      case 1: return v1;                      // BP
      case 2: return x - k * v1 - v2;         // HP
      case 3: return x - k * v1;              // notch
      default: return v2;                     // LP
    }
  }

  // 4-pole cascade with global feedback, solved with zero delay feedback.
  const float G = g / (1.0f + g);
  const float b = 1.0f / (1.0f + g);
  const bool ota = board_ == BOARD_SMR4;
  const float k = r * (ota ? 4.4f : 4.6f);
  const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
  const float sigma = b * (G3 * s_[0] + G2 * s_[1] + G * s_[2] + s_[3]);
  float u = (x - k * sigma) / (1.0f + k * G4);
  // Input differential pair.
  u = FastTanh(u * 0.9f) * (1.0f / 0.9f);

  float y[4];
  float in = u;
  for (int i = 0; i < 4; ++i) {
    // The LM13700 stages each saturate a little (OTA input stage).
    const float xi = ota ? 1.25f * FastTanh(in * 0.8f) : in;
    const float v = (xi - s_[i]) * G;
    y[i] = v + s_[i];
    s_[i] = y[i] + v;
    in = y[i];
  }
  const float comp = 1.0f + 0.45f * k;  // partial pass-band loss compensation
  if (ota) return y[3] * comp;          // SMR4 is low-pass only
  switch (mode_) {
    case 1: return 2.0f * (y[1] - 2.0f * y[2] + y[3]);                         // BP
    case 2: return (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3]);      // HP
    case 3: return (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + 2.0f * y[3]); // notch
    default: return y[3] * comp;                                               // LP4
  }
}

void AnalogVoice::Process(const uint8_t* in, int n, float* out) {
  const float noise_scale = noise_amount_ * 6.0e-5f;
  for (int i = 0; i < n; ++i) {
    // DAC with "crush": the MCU only writes every crush-th sample.
    if (++crush_counter_ >= crush_) {
      crush_counter_ = 0;
      dac_ = (static_cast<float>(in[i]) - 128.0f) * (1.0f / 128.0f);
    }
    const float x = dac_ * drive_ + tol_.offset;
    for (int j = 0; j < 2; ++j) {
      cutoff_cv_ += cv_coef_ * (cutoff_target_ - cutoff_cv_);
      resonance_cv_ += cv_coef_ * (resonance_target_ - resonance_cv_);
      vca_cv_ += vca_coef_ * (vca_target_ - vca_cv_);
      float y = FilterSample(x, cutoff_cv_, resonance_cv_);
      // VCA (linear response: the firmware writes vca * 16 to the 12-bit DAC).
      float gain = vca_cv_ * (1.0f / 255.0f) * tol_.vca_gain;
      y *= gain;
      if (board_ == BOARD_SVF) {
        y = y / (1.0f + 0.08f * std::fabs(y));          // SSM2164, clean
      } else {
        y = 1.4f * FastTanh(y * (1.0f / 1.4f));        // OTA VCA
      }
      // Hiss of the analog stages.
      noise_state_ = noise_state_ * 1664525u + 1013904223u;
      y += (static_cast<float>(noise_state_ >> 9) * (1.0f / 4194304.0f) - 1.0f) * noise_scale;
      // Output coupling capacitor.
      const float hp = hp_coef_ * (hp_y_ + y - hp_x_);
      hp_x_ = y;
      hp_y_ = hp;
      *out++ = hp;
    }
  }
  // Denormal / runaway protection.
  for (float& s : s_) if (!std::isfinite(s) || std::fabs(s) < 1e-20f) s = 0.0f;
  if (!std::isfinite(ic1_) || std::fabs(ic1_) < 1e-20f) ic1_ = 0.0f;
  if (!std::isfinite(ic2_) || std::fabs(ic2_) < 1e-20f) ic2_ = 0.0f;
  if (!std::isfinite(hp_y_)) hp_y_ = hp_x_ = 0.0f;
}

// ---------------------------------------------------------------------------
void Resampler::Init(double in_rate, double out_rate) {
  step_ = in_rate / out_rate;
  double cutoff_hz = std::min(0.45 * out_rate, 20500.0);
  double fc = cutoff_hz / in_rate;  // cycles per input sample
  const double pi = 3.14159265358979323846;
  for (int p = 0; p <= kPhases; ++p) {
    double frac = static_cast<double>(p) / kPhases;
    double sum = 0.0;
    double row[2 * kHalf];
    for (int j = 0; j < 2 * kHalf; ++j) {
      double d = (j - kHalf + 1) - frac;
      double sinc = d == 0.0 ? 1.0 : std::sin(2.0 * pi * fc * d) / (2.0 * pi * fc * d);
      double w = d / kHalf;
      // Blackman-Harris window over [-kHalf, kHalf].
      double t = 0.5 * (w + 1.0);
      double win = (std::fabs(w) >= 1.0) ? 0.0
          : 0.35875 - 0.48829 * std::cos(2 * pi * t) + 0.14128 * std::cos(4 * pi * t)
            - 0.01168 * std::cos(6 * pi * t);
      row[j] = 2.0 * fc * sinc * win;
      sum += row[j];
    }
    for (int j = 0; j < 2 * kHalf; ++j) table_[p][j] = static_cast<float>(row[j] / sum);
  }
  for (int i = 0; i < kSize; ++i) buf_l_[i] = buf_r_[i] = 0.0f;
  write_ = 0;
  pos_ = kHalf;
}

void Resampler::Pop(float* l, float* r) {
  int64_t n0 = static_cast<int64_t>(pos_);
  double frac = pos_ - static_cast<double>(n0);
  double fp = frac * kPhases;
  int p = static_cast<int>(fp);
  float a = static_cast<float>(fp - p);
  const float* h0 = table_[p];
  const float* h1 = table_[p + 1];
  float sl = 0.0f, sr = 0.0f;
  int64_t base = n0 - kHalf + 1;
  for (int j = 0; j < 2 * kHalf; ++j) {
    float h = h0[j] + a * (h1[j] - h0[j]);
    int idx = static_cast<int>((base + j) & kMask);
    sl += h * buf_l_[idx];
    sr += h * buf_r_[idx];
  }
  *l = sl;
  *r = sr;
  pos_ += step_;
}

}  // namespace ambika
