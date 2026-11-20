// Ambika VST - declarations of the lookup tables taken from the firmware.
// Original data (c) Emilie Gillet, GPL v3.
#pragma once

#include "avr_compat.h"

namespace ambika {
namespace vc {
extern const uint16_t lut_res_lfo_increments[];
extern const uint16_t lut_res_env_portamento_increments[];
extern const uint16_t lut_res_oscillator_increments[];
extern const uint16_t lut_res_fm_frequency_ratios[];
extern const uint16_t lut_res_vca_linearization[];
extern const uint16_t lut_res_cz_phase_reset[];
extern const uint8_t wav_res_formant_sine[];
extern const uint8_t wav_res_formant_square[];
extern const uint8_t wav_res_sine[];
extern const uint8_t wav_res_vowel_data[];
extern const uint8_t wav_res_distortion[];
extern const uint8_t wav_res_env_expo[];
extern const uint8_t wav_res_waves[];
extern const uint8_t wav_res_wavetables[];
extern const uint8_t* const waveform_table[];

enum {
  WAV_RES_SINE = 2,
  WAV_RES_BANDLIMITED_SQUARE_0 = 3,
  WAV_RES_BANDLIMITED_SAW_0 = 10,
  WAV_RES_BANDLIMITED_SAW_1 = 11,
  WAV_RES_BANDLIMITED_TRIANGLE_0 = 17,
};
}  // namespace vc

namespace ctl {
extern const uint16_t lut_res_lfo_increments[];
extern const uint16_t lut_res_arpeggiator_patterns[];
extern const uint16_t* const lookup_table_table[];
extern const uint8_t wav_res_lfo_waveforms[];

enum {
  LUT_RES_SCALE_JUST = 1,
  LUT_RES_GROOVE_SWING = 34,
};
}  // namespace ctl
}  // namespace ambika
