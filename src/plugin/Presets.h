// Ambika VST - factory presets. Each preset is a list of changes applied on
// top of the Ambika init patch. GPL v3.
#pragma once

#include <utility>
#include <vector>

namespace ambika_plugin {

struct Preset {
  const char* name;
  std::vector<std::pair<const char*, int>> values;
};

// Programs imported from Ambika .PRO / .MUL / .syx files (patch + part data
// as hex). See PRESETS.md for sources and credits.
struct BankProgram {
  const char* bank;
  const char* name;
  const char* hex;  // 196 bytes = 112 patch + 84 part, hex encoded
};

inline const BankProgram* bankPrograms(int* count) {
  static const BankProgram programs[] = {
#include "FactoryBank.inc"
  };
  *count = static_cast<int>(sizeof(programs) / sizeof(programs[0]));
  return programs;
}

inline const std::vector<Preset>& factoryPresets() {
  static const std::vector<Preset> presets = {
    {"Init", {}},
    {"Fat Brass", {
      {"osc1_shape", 1}, {"osc2_shape", 1}, {"osc2_range", 0}, {"osc2_tune", 9},
      {"cutoff", 46}, {"resonance", 8}, {"filter_env", 44},
      {"env2_attack", 18}, {"env2_decay", 52}, {"env2_sustain", 70}, {"env2_release", 40},
      {"env3_attack", 10}, {"env3_sustain", 115}, {"env3_release", 42},
      {"mod3_amt", 2}, {"mod4_amt", 2}, {"lfo1_rate", 15 + 70}}},
    {"Sync Lead", {
      {"osc1_shape", 1}, {"osc2_shape", 1}, {"mix_op", 1}, {"osc2_range", 12}, {"osc2_tune", 0},
      {"mix_balance", 50}, {"mod2_src", 0}, {"mod2_dst", 3}, {"mod2_amt", 36},
      {"env1_decay", 50}, {"env1_sustain", 30}, {"cutoff", 92}, {"resonance", 14},
      {"poly_mode", 0}, {"portamento", 18}, {"legato", 1}, {"env3_sustain", 120}, {"volume", 85}}},
    {"CZ Reso Bass", {
      {"osc1_shape", 6}, {"osc1_param", 20}, {"osc2_shape", 0}, {"mix_balance", 0},
      {"mod1_amt", 48}, {"env1_decay", 34}, {"env1_sustain", 0},
      {"sub_shape", 0}, {"sub_level", 40}, {"cutoff", 58}, {"resonance", 22}, {"filter_env", 30},
      {"env2_decay", 38}, {"env3_decay", 50}, {"env3_sustain", 90}, {"env3_release", 20},
      {"poly_mode", 0}, {"octave", -1}, {"volume", 95}}},
    {"Vowel Choir", {
      {"osc1_shape", 20}, {"osc1_param", 40}, {"osc2_shape", 23}, {"osc2_param", 40}, {"osc2_range", 0},
      {"osc2_tune", 4}, {"mod5_amt", 26}, {"lfo2_rate", 15 + 18}, {"mod6_amt", 14},
      {"cutoff", 100}, {"filter_env", 0}, {"env3_attack", 52}, {"env3_release", 72},
      {"stereo", 80}, {"spread", 4}, {"board", 2}}},
    {"FM Bells", {
      {"osc1_shape", 16}, {"osc1_param", 34}, {"osc1_range", 7}, {"osc2_shape", 36}, {"osc2_range", 12},
      {"osc2_tune", 0}, {"mix_balance", 22}, {"mod1_amt", 40}, {"env1_decay", 62}, {"env1_sustain", 0},
      {"env3_decay", 82}, {"env3_sustain", 0}, {"env3_release", 76}, {"cutoff", 127}, {"filter_env", 0},
      {"board", 2}, {"stereo", 60}, {"volume", 100}}},
    {"8-Bit Arp", {
      {"osc1_shape", 17}, {"osc1_param", 42}, {"osc2_shape", 2}, {"osc2_param", 0}, {"osc2_range", 12},
      {"arp_mode", 1}, {"arp_dir", 2}, {"arp_range", 2}, {"arp_grid", 10},
      {"env3_decay", 32}, {"env3_sustain", 40}, {"env3_release", 24}, {"crush", 3}, {"cutoff", 110}}},
    {"Tampura Pad", {
      {"osc1_shape", 24}, {"osc1_param", 20}, {"osc2_shape", 15}, {"osc2_param", 40}, {"osc2_range", 0},
      {"osc2_tune", 0}, {"mod3_dst", 0}, {"mod3_amt", 22}, {"lfo1_rate", 15 + 14},
      {"env3_attack", 70}, {"env3_release", 84}, {"cutoff", 80}, {"filter_env", 10},
      {"stereo", 100}, {"spread", 2}, {"board", 1}}},
    {"Ring Mod Metal", {
      {"osc1_shape", 2}, {"osc2_shape", 4}, {"osc2_range", 19}, {"osc2_tune", 3}, {"mix_op", 2},
      {"mix_amount", 63}, {"env3_decay", 62}, {"env3_sustain", 0}, {"env3_release", 52},
      {"cutoff", 120}, {"filter_env", 0}}},
    {"Noise Sweep", {
      {"osc1_shape", 19}, {"osc1_param", 30}, {"osc2_shape", 0}, {"mix_balance", 0},
      {"filter_mode", 1}, {"resonance", 52}, {"cutoff", 60}, {"filter_lfo", 40}, {"lfo2_rate", 15 + 12},
      {"filter_env", 0}, {"env3_attack", 40}, {"env3_release", 70}, {"board", 1}}},
    {"Unison Monster", {
      {"osc1_shape", 1}, {"osc2_shape", 1}, {"osc2_range", 0}, {"osc2_tune", 11},
      {"sub_shape", 3}, {"sub_level", 30}, {"fuzz", 18}, {"cutoff", 54}, {"resonance", 30},
      {"filter_env", 40}, {"poly_mode", 0}, {"spread", 6}, {"stereo", 100}, {"portamento", 10},
      {"env3_sustain", 120}}},
    {"Sequenced Pluck", {
      {"osc1_shape", 5}, {"osc1_param", 30}, {"osc2_shape", 0}, {"mix_balance", 0},
      {"mod1_amt", 40}, {"env1_decay", 36}, {"env1_sustain", 0}, {"arp_mode", 2},
      {"env3_decay", 36}, {"env3_sustain", 0}, {"env3_release", 30}, {"mod9_amt", 26},
      {"cutoff", 70}, {"resonance", 26}}},
    {"Digital Organ", {
      {"osc1_shape", 30}, {"osc1_param", 50}, {"osc2_shape", 29}, {"osc2_param", 30}, {"osc2_range", 12},
      {"osc2_tune", 0}, {"mix_balance", 20}, {"env3_decay", 0}, {"env3_sustain", 127},
      {"env3_release", 16}, {"cutoff", 110}, {"filter_env", 0}, {"board", 2}}},
    {"Wavquence Glitch", {
      {"osc1_shape", 37}, {"osc2_shape", 0}, {"mix_balance", 0}, {"mod9_dst", 0}, {"mod9_amt", 63},
      {"sub_shape", 7}, {"sub_level", 40}, {"arp_grid", 12}, {"cutoff", 100}, {"filter_env", 10}}},
    {"PWM Strings", {
      {"osc1_shape", 18}, {"osc1_param", 20}, {"osc2_shape", 18}, {"osc2_param", 40}, {"osc2_range", 0},
      {"osc2_tune", 7}, {"mod5_amt", 18}, {"mod6_amt", 18}, {"lfo2_rate", 15 + 30},
      {"env3_attack", 44}, {"env3_release", 64}, {"cutoff", 70}, {"filter_env", 8}, {"stereo", 60}}},
    {"Raga Drone", {
      {"raga", 6}, {"osc1_shape", 33}, {"osc1_param", 40}, {"osc2_shape", 34}, {"osc2_param", 60},
      {"osc2_range", 0}, {"osc2_tune", 5}, {"mod7_amt", 30}, {"lfo3_rate", 15 + 8},
      {"env3_attack", 80}, {"env3_release", 100}, {"cutoff", 84}, {"resonance", 18}, {"board", 1},
      {"stereo", 70}}},
  };
  return presets;
}

}  // namespace ambika_plugin
