// Ambika VST - patch / part data structures.
// Layout identical to the Ambika firmware (common/patch.h, controller/part.h),
// Copyright 2011 Emilie Gillet. GPL v3.
#pragma once

#include "avr_compat.h"

namespace ambika {

struct OscillatorSettings { uint8_t shape; uint8_t parameter; int8_t range; int8_t detune; };
struct FilterSettings { uint8_t cutoff; uint8_t resonance; uint8_t mode; };
struct EnvelopeLfoSettings {
  uint8_t attack, decay, sustain, release, shape, rate, padding, retrigger_mode;
};
struct Modulation { uint8_t source; uint8_t destination; int8_t amount; };
struct Modifier { uint8_t operands[2]; uint8_t op; };

enum OscillatorAlgorithm {
  WAVEFORM_NONE, WAVEFORM_SAW, WAVEFORM_SQUARE, WAVEFORM_TRIANGLE, WAVEFORM_SINE,
  WAVEFORM_CZ_SAW, WAVEFORM_CZ_SAW_LP, WAVEFORM_CZ_SAW_PK, WAVEFORM_CZ_SAW_BP,
  WAVEFORM_CZ_SAW_HP, WAVEFORM_CZ_PLS_LP, WAVEFORM_CZ_PLS_PK, WAVEFORM_CZ_PLS_BP,
  WAVEFORM_CZ_PLS_HP, WAVEFORM_CZ_TRI_LP, WAVEFORM_QUAD_SAW_PAD, WAVEFORM_FM,
  WAVEFORM_8BITLAND, WAVEFORM_DIRTY_PWM, WAVEFORM_FILTERED_NOISE, WAVEFORM_VOWEL,
  WAVEFORM_WAVETABLE_1, WAVEFORM_WAVETABLE_16 = WAVEFORM_WAVETABLE_1 - 1 + 16,
  WAVEFORM_WAVEQUENCE, WAVEFORM_LAST
};

enum SubOscillatorAlgorithm {
  WAVEFORM_SUB_OSC_SQUARE_1, WAVEFORM_SUB_OSC_TRIANGLE_1, WAVEFORM_SUB_OSC_PULSE_1,
  WAVEFORM_SUB_OSC_SQUARE_2, WAVEFORM_SUB_OSC_TRIANGLE_2, WAVEFORM_SUB_OSC_PULSE_2,
  WAVEFORM_SUB_OSC_CLICK, WAVEFORM_SUB_OSC_GLITCH, WAVEFORM_SUB_OSC_BLOW,
  WAVEFORM_SUB_OSC_METALLIC, WAVEFORM_SUB_OSC_POP, WAVEFORM_SUB_OSC_LAST
};

enum Operator { OP_SUM, OP_SYNC, OP_RING_MOD, OP_XOR, OP_FOLD, OP_BITS, OP_LAST };

enum ModifierOp {
  MODIFIER_NONE, MODIFIER_SUM, MODIFIER_PRODUCT, MODIFIER_ATTENUATE, MODIFIER_MAX,
  MODIFIER_MIN, MODIFIER_XOR, MODIFIER_GE, MODIFIER_LE, MODIFIER_QUANTIZE,
  MODIFIER_LAG_PROCESSOR, MODIFIER_LAST
};

enum LfoWave {
  LFO_WAVEFORM_TRIANGLE, LFO_WAVEFORM_SQUARE, LFO_WAVEFORM_S_H, LFO_WAVEFORM_RAMP,
  LFO_WAVEFORM_WAVE_1, LFO_WAVEFORM_LAST = LFO_WAVEFORM_WAVE_1 + 16
};

enum LfoSyncMode { LFO_SYNC_MODE_FREE, LFO_SYNC_MODE_SLAVE, LFO_SYNC_MODE_MASTER, LFO_SYNC_MODE_LAST };

enum ModulationSource {
  MOD_SRC_ENV_1, MOD_SRC_ENV_2, MOD_SRC_ENV_3,
  MOD_SRC_LFO_1, MOD_SRC_LFO_2, MOD_SRC_LFO_3, MOD_SRC_LFO_4,
  MOD_SRC_OP_1, MOD_SRC_OP_2, MOD_SRC_OP_3, MOD_SRC_OP_4,
  MOD_SRC_SEQ_1, MOD_SRC_SEQ_2, MOD_SRC_ARP_STEP,
  MOD_SRC_VELOCITY, MOD_SRC_AFTERTOUCH, MOD_SRC_PITCH_BEND, MOD_SRC_WHEEL,
  MOD_SRC_WHEEL_2, MOD_SRC_EXPRESSION,
  MOD_SRC_NOTE, MOD_SRC_GATE, MOD_SRC_NOISE, MOD_SRC_RANDOM,
  MOD_SRC_CONSTANT_256, MOD_SRC_CONSTANT_128, MOD_SRC_CONSTANT_64, MOD_SRC_CONSTANT_32,
  MOD_SRC_CONSTANT_16, MOD_SRC_CONSTANT_8, MOD_SRC_CONSTANT_4,
  MOD_SRC_LAST
};

enum ModulationDestination {
  MOD_DST_PARAMETER_1, MOD_DST_PARAMETER_2, MOD_DST_OSC_1, MOD_DST_OSC_2,
  MOD_DST_OSC_1_2_COARSE, MOD_DST_OSC_1_2_FINE,
  MOD_DST_MIX_BALANCE, MOD_DST_MIX_PARAM, MOD_DST_MIX_NOISE, MOD_DST_MIX_SUB_OSC,
  MOD_DST_MIX_FUZZ, MOD_DST_MIX_CRUSH,
  MOD_DST_FILTER_CUTOFF, MOD_DST_FILTER_RESONANCE,
  MOD_DST_ATTACK, MOD_DST_DECAY, MOD_DST_RELEASE, MOD_DST_LFO_4,
  MOD_DST_VCA,
  MOD_DST_LAST
};

enum FilterMode { FILTER_MODE_LP, FILTER_MODE_BP, FILTER_MODE_HP, FILTER_MODE_NOTCH };

static const uint8_t kNumSyncedLfoRates = 15;
static const uint8_t kNumEnvelopes = 3;
static const uint8_t kNumLfos = 3;
static const uint8_t kNumModulations = 14;
static const uint8_t kNumModifiers = 4;
static const uint8_t kNumOscillators = 2;

struct Patch {
  OscillatorSettings osc[kNumOscillators];   // 0-7
  uint8_t mix_balance;                        // 8
  uint8_t mix_op;
  uint8_t mix_parameter;
  uint8_t mix_sub_osc_shape;
  uint8_t mix_sub_osc;
  uint8_t mix_noise;
  uint8_t mix_fuzz;
  uint8_t mix_crush;                          // 15
  FilterSettings filter[2];                   // 16-21
  int8_t filter_env;                          // 22
  int8_t filter_lfo;                          // 23
  EnvelopeLfoSettings env_lfo[kNumEnvelopes]; // 24-47
  uint8_t voice_lfo_shape;                    // 48
  uint8_t voice_lfo_rate;                     // 49
  Modulation modulation[kNumModulations];     // 50-91
  Modifier modifier[kNumModifiers];           // 92-103
  uint8_t padding[8];                         // 104-111
};
static_assert(sizeof(Patch) == 112, "Patch layout must match the firmware");

enum ArpeggiatorDirection {
  ARPEGGIO_DIRECTION_UP, ARPEGGIO_DIRECTION_DOWN, ARPEGGIO_DIRECTION_UP_DOWN,
  ARPEGGIO_DIRECTION_AS_PLAYED, ARPEGGIO_DIRECTION_RANDOM, ARPEGGIO_DIRECTION_CHORD,
  ARPEGGIO_DIRECTION_LAST
};
enum ArpSequencerMode { ARP_SEQUENCER_MODE_STEP, ARP_SEQUENCER_MODE_ARPEGGIATOR, ARP_SEQUENCER_MODE_NOTE, ARP_SEQUENCER_MODE_LAST };
enum PolyphonyMode { MONO, POLY, UNISON_2X, CYCLIC, CHAIN, POLYPHONY_MODE_LAST };

static const uint8_t kNumSequences = 3;
static const uint8_t kNumVoices = 6;
static const uint8_t kNumArpeggiatorPatterns = 22;

struct NoteStep { uint8_t note, velocity, legato, gate; };

struct PartData {
  uint8_t volume;            // 0
  int8_t octave;             // 1
  int8_t tuning;             // 2
  uint8_t spread;            // 3
  uint8_t raga;              // 4
  uint8_t legato;            // 5
  uint8_t portamento_time;   // 6
  uint8_t arp_sequencer_mode;// 7
  uint8_t arp_direction;     // 8
  uint8_t arp_octave;        // 9
  uint8_t arp_pattern;       // 10
  uint8_t arp_divider;       // 11
  uint8_t sequence_length[kNumSequences];  // 12-14
  uint8_t polyphony_mode;    // 15
  //  0..15: step sequence 1, 16..31: step sequence 2
  // 32..63: (note | 0x80 if gate), (velocity | 0x80 if legato)
  uint8_t sequence_data[64];
  uint8_t padding[4];

  uint8_t step_value(uint8_t sequence, uint8_t step) const {
    return sequence_data[(step + (sequence << 4)) & 0x1f];
  }
  NoteStep note_step(uint8_t step) const {
    NoteStep n;
    uint8_t offset = (32 + (step << 1)) & 0x3f;
    n.note = sequence_data[offset];
    n.velocity = sequence_data[offset + 1];
    n.gate = n.note & 0x80;
    n.legato = n.velocity & 0x80;
    n.note &= 0x7f;
    n.velocity &= 0x7f;
    return n;
  }
};
static_assert(sizeof(PartData) == 84, "PartData layout must match the firmware");

// Part settings needed on the voicecard side.
struct VoicePartSettings { uint8_t volume; uint8_t legato; uint8_t portamento_time; };

void InitPatch(Patch* p);
void InitPartData(PartData* d);

}  // namespace ambika
