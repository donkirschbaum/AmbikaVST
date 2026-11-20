// Ambika VST - parameter table. GPL v3.

#include "Parameters.h"

#include "../engine/patch.h"

namespace ambika_plugin {

using namespace ambika;

static const char* const kWaveforms[] = {
  "none", "saw", "square", "triangle", "sine", "zsaw", "lpzsaw", "pkzsaw", "bpzsaw",
  "hpzsaw", "lpzpulse", "pkzpulse", "bpzpulse", "hpzpulse", "ztriangle", "pad", "fm",
  "8bits", "pwm", "noise", "vowel", "male", "female", "choir", "tampura", "bowed",
  "cello", "vibes", "slap", "epiano", "organ", "waves", "digital", "drone 1",
  "drone 2", "metallic", "bell", "wavquence"
};
static const char* const kOperators[] = { "off", "sync", "ringmod", "xor", "fold", "bits" };
static const char* const kSubWaveforms[] = {
  "squ1", "tri1", "pul1", "squ2", "tri2", "pul2", "click", "glitch", "blow", "metal", "pop"
};
static const char* const kFilterModes[] = { "lp", "bp", "hp" };
static const char* const kLfoShapes[] = {
  "tri", "sqr", "s&h", "ramp", "hrm2", "hrm3", "hrm5", "grg1", "grg2", "bat1", "bat2",
  "spk1", "spk2", "lsaw", "lsqr", "rsaw", "rsqr", "stp1", "stp2", "stp3"
};
static const char* const kLfoSync[] = { "free", "slave", "master" };
static const char* const kLfoSyncedRates[] = {
  "1/1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "1/6", "1/8", "1/12", "1/16", "1/24",
  "1/32", "1/48", "1/96"
};
static const char* const kModSources[] = {
  "env1", "env2", "env3", "lfo1", "lfo2", "lfo3", "lfo4", "mod1", "mod2", "mod3", "mod4",
  "seq1", "seq2", "arp", "velo", "aftr", "bend", "mwhl", "whl2", "pdal", "note", "gate",
  "nois", "rand", "=256", "=128", "=64", "=32", "=16", "=8", "=4"
};
static const char* const kModDests[] = {
  "prm1", "prm2", "osc1", "osc2", "o1+2", "vibr", "mix", "xmod", "nois", "sub", "fuzz",
  "crsh", "freq", "reso", "attk", "deca", "rele", "lfo4", "vca"
};
static const char* const kModifierOps[] = {
  "off", "add", "prod", "attn", "max", "min", "xor", ">=", "<=", "qtz", "lag"
};
static const char* const kRagas[] = {
  "equal", "just", "pythagorean", "1/4 eb", "1/4 e", "1/4 ea", "bhairav", "gunakri",
  "marwa", "shree", "purvi", "bilawal", "yaman", "kafi", "bhimpalasree", "darbari",
  "bageshree", "rageshree", "khamaj", "mi'mal", "parameshwari", "rangeshwari",
  "gangeshwari", "kameshwari", "pa. kafi", "natbhairav", "m.kauns", "bairagi", "b.todi",
  "chandradeep", "kaushik todi", "jogeshwari"
};
static const char* const kArpModes[] = { "step seq", "arpeggio", "sequencer" };
static const char* const kArpDirections[] = { "up", "down", "up&down", "played", "random", "chord" };
static const char* const kPolyModes[] = { "mono", "poly", "2x unison", "cyclic", "chain" };
static const char* const kBoards[] = { "SMR4", "4P", "SVF" };
static const char* const kClockSources[] = { "internal", "host" };
static const char* const kGrooves[] = { "swing", "shuffle", "push", "lag", "human", "monkey" };

#define PATCH(off) Level::Patch, (off)
#define PART(off) Level::Part, (off)
#define PLUG Level::Plugin, -1

static std::vector<ParamDef> buildDefs() {
  std::vector<ParamDef> d;
  // Oscillators.
  for (int i = 0; i < 2; ++i) {
    int b = i * 4;
    auto id = [i](const char* s) { return strdup((juce::String("osc") + juce::String(i + 1) + "_" + s).toRawUTF8()); };
    auto nm = [i](const char* s) { return strdup((juce::String("Osc ") + juce::String(i + 1) + " " + s).toRawUTF8()); };
    d.push_back({id("shape"), "wave", nm("Waveform"), PATCH(b + 0), 0, WAVEFORM_LAST - 1, i == 0 ? WAVEFORM_SAW : WAVEFORM_SQUARE, Unit::Waveform, i == 0 ? 16 : 18});
    d.push_back({id("param"), "prm", nm("Parameter"), PATCH(b + 1), 0, 127, i == 0 ? 0 : 32, Unit::Raw, i == 0 ? 17 : 19});
    d.push_back({id("range"), "rng", nm("Range"), PATCH(b + 2), -24, 24, i == 0 ? 0 : -12, Unit::Int, i == 0 ? 14 : 20});
    d.push_back({id("tune"), "tun", nm("Tune"), PATCH(b + 3), -64, 64, i == 0 ? 0 : 12, Unit::Int, i == 0 ? 15 : 21});
  }
  // Mixer.
  d.push_back({"mix_balance", "mix", "Osc Mix", PATCH(8), 0, 63, 32, Unit::Int, 22});
  d.push_back({"mix_op", "xmod", "Crossmod", PATCH(9), 0, OP_LAST - 1, 0, Unit::Operator, 23});
  d.push_back({"mix_amount", "amnt", "Crossmod Amount", PATCH(10), 0, 63, 31, Unit::Int, 24});
  d.push_back({"sub_shape", "wave", "Sub Osc Waveform", PATCH(11), 0, WAVEFORM_SUB_OSC_LAST - 1, 0, Unit::SubWaveform, 25});
  d.push_back({"sub_level", "sub", "Sub Osc Level", PATCH(12), 0, 63, 0, Unit::Int, 26});
  d.push_back({"noise_level", "nois", "Noise", PATCH(13), 0, 63, 0, Unit::Int, 27});
  d.push_back({"fuzz", "fuzz", "Fuzz", PATCH(14), 0, 63, 0, Unit::Int, 12});
  d.push_back({"crush", "crsh", "Crush", PATCH(15), 0, 31, 0, Unit::Int, 13});
  // Filter.
  d.push_back({"cutoff", "freq", "Filter Frequency", PATCH(16), 0, 127, 96, Unit::Raw, 74});
  d.push_back({"resonance", "reso", "Filter Resonance", PATCH(17), 0, 63, 0, Unit::Int, 71});
  d.push_back({"filter_mode", "mode", "Filter Mode", PATCH(18), 0, 2, 0, Unit::FilterMode, 28});
  d.push_back({"filter_env", "env2", "Env2 > VCF", PATCH(22), 0, 63, 24, Unit::Int, 3});
  d.push_back({"filter_lfo", "lfo2", "LFO2 > VCF", PATCH(23), 0, 63, 0, Unit::Int, 9});
  // Envelopes & LFOs.
  static const int defAttack[3] = {0, 0, 0}, defDecay[3] = {40, 40, 40}, defSustain[3] = {20, 0, 100},
                   defRelease[3] = {60, 40, 40}, defRate[3] = {24, 32, 48};
  static const int envCC[4] = {73, 75, 70, 72};
  for (int i = 0; i < 3; ++i) {
    int b = 24 + i * 8;
    auto id = [i](const char* s) { return strdup((juce::String("env") + juce::String(i + 1) + "_" + s).toRawUTF8()); };
    auto nm = [i](const char* s) { return strdup((juce::String(s) + " " + juce::String(i + 1)).toRawUTF8()); };
    auto lid = [i](const char* s) { return strdup((juce::String("lfo") + juce::String(i + 1) + "_" + s).toRawUTF8()); };
    d.push_back({id("attack"), "attk", nm("Attack"), PATCH(b + 0), 0, 127, defAttack[i], Unit::Raw, i == 0 ? envCC[0] : -1});
    d.push_back({id("decay"), "deca", nm("Decay"), PATCH(b + 1), 0, 127, defDecay[i], Unit::Raw, i == 0 ? envCC[1] : -1});
    d.push_back({id("sustain"), "sust", nm("Sustain"), PATCH(b + 2), 0, 127, defSustain[i], Unit::Raw, i == 0 ? envCC[2] : -1});
    d.push_back({id("release"), "rele", nm("Release"), PATCH(b + 3), 0, 127, defRelease[i], Unit::Raw, i == 0 ? envCC[3] : -1});
    d.push_back({lid("shape"), "wave", nm("LFO Waveform"), PATCH(b + 4), 0, LFO_WAVEFORM_LAST - 1, 0, Unit::LfoShape, i == 0 ? 46 : -1});
    d.push_back({lid("rate"), "rate", nm("LFO Rate"), PATCH(b + 5), 0, kNumSyncedLfoRates + 127, kNumSyncedLfoRates + defRate[i], Unit::LfoRate, i == 0 ? 45 : -1});
    d.push_back({lid("sync"), "trig", nm("LFO Trigger"), PATCH(b + 7), 0, LFO_SYNC_MODE_LAST - 1, 0, Unit::LfoSync, i == 0 ? 44 : -1});
  }
  d.push_back({"vlfo_shape", "wave", "Voice LFO Waveform", PATCH(48), 0, LFO_WAVEFORM_RAMP, 0, Unit::VoiceLfoShape, 48});
  d.push_back({"vlfo_rate", "rate", "Voice LFO Rate", PATCH(49), 0, 127, 72, Unit::Raw, 47});
  // Modulation matrix.
  static const int defSrc[14] = {MOD_SRC_ENV_1, MOD_SRC_ENV_1, MOD_SRC_LFO_1, MOD_SRC_LFO_1, MOD_SRC_LFO_2, MOD_SRC_LFO_2, MOD_SRC_LFO_3,
                                 MOD_SRC_LFO_4, MOD_SRC_SEQ_1, MOD_SRC_SEQ_2, MOD_SRC_ENV_3, MOD_SRC_VELOCITY, MOD_SRC_PITCH_BEND, MOD_SRC_LFO_4};
  static const int defDst[14] = {MOD_DST_PARAMETER_1, MOD_DST_PARAMETER_2, MOD_DST_OSC_1, MOD_DST_OSC_2, MOD_DST_PARAMETER_1, MOD_DST_PARAMETER_2,
                                 MOD_DST_MIX_BALANCE, MOD_DST_FILTER_CUTOFF, MOD_DST_FILTER_CUTOFF, MOD_DST_MIX_BALANCE, MOD_DST_VCA, MOD_DST_VCA,
                                 MOD_DST_OSC_1_2_COARSE, MOD_DST_OSC_1_2_COARSE};
  static const int defAmt[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 63, 16, 32, 16};
  for (int i = 0; i < 14; ++i) {
    int b = 50 + i * 3;
    auto id = [i](const char* s) { return strdup((juce::String("mod") + juce::String(i + 1) + "_" + s).toRawUTF8()); };
    auto nm = [i](const char* s) { return strdup((juce::String("Mod ") + juce::String(i + 1) + " " + s).toRawUTF8()); };
    d.push_back({id("src"), "srce", nm("Source"), PATCH(b + 0), 0, MOD_SRC_CONSTANT_4, defSrc[i], Unit::ModSource, -1});
    d.push_back({id("dst"), "dest", nm("Destination"), PATCH(b + 1), 0, MOD_DST_LAST - 1, defDst[i], Unit::ModDest, -1});
    d.push_back({id("amt"), "amnt", nm("Amount"), PATCH(b + 2), -63, 63, defAmt[i], Unit::Int, -1});
  }
  static const int defIn1[4] = {MOD_SRC_LFO_1, MOD_SRC_LFO_2, MOD_SRC_LFO_3, MOD_SRC_SEQ_1};
  static const int defIn2[4] = {MOD_SRC_LFO_2, MOD_SRC_LFO_3, MOD_SRC_SEQ_1, MOD_SRC_SEQ_2};
  for (int i = 0; i < 4; ++i) {
    int b = 92 + i * 3;
    auto id = [i](const char* s) { return strdup((juce::String("modif") + juce::String(i + 1) + "_" + s).toRawUTF8()); };
    auto nm = [i](const char* s) { return strdup((juce::String("Modifier ") + juce::String(i + 1) + " " + s).toRawUTF8()); };
    d.push_back({id("in1"), "in1", nm("In 1"), PATCH(b + 0), 0, MOD_SRC_LAST - 1, defIn1[i], Unit::ModSource, -1});
    d.push_back({id("in2"), "in2", nm("In 2"), PATCH(b + 1), 0, MOD_SRC_LAST - 1, defIn2[i], Unit::ModSource, -1});
    d.push_back({id("op"), "oper", nm("Operator"), PATCH(b + 2), 0, MODIFIER_LAST - 1, 0, Unit::ModifierOp, -1});
  }
  // Part.
  d.push_back({"volume", "volu", "Part Volume", PART(0), 0, 127, 120, Unit::Raw, 7});
  d.push_back({"octave", "octv", "Octave", PART(1), -2, 2, 0, Unit::Int, -1});
  d.push_back({"tuning", "tune", "Fine Tuning", PART(2), -127, 127, 0, Unit::Int, 94});
  d.push_back({"spread", "sprd", "Spread", PART(3), 0, 40, 0, Unit::Int, 95});
  d.push_back({"raga", "raga", "Raga", PART(4), 0, 31, 0, Unit::Raga, -1});
  d.push_back({"legato", "lega", "Legato", PART(5), 0, 1, 0, Unit::Boolean, 68});
  d.push_back({"portamento", "port", "Portamento", PART(6), 0, 63, 0, Unit::Int, 5});
  d.push_back({"arp_mode", "a/sq", "Arp/Seq Mode", PART(7), 0, ARP_SEQUENCER_MODE_LAST - 1, 0, Unit::ArpMode, 102});
  d.push_back({"arp_dir", "dir", "Arp Direction", PART(8), 0, ARPEGGIO_DIRECTION_LAST - 1, 0, Unit::ArpDirection, 103});
  d.push_back({"arp_range", "rnge", "Arp Range", PART(9), 1, 4, 1, Unit::Int, 104});
  d.push_back({"arp_pattern", "patt", "Arp Pattern", PART(10), 0, kNumArpeggiatorPatterns - 1, 0, Unit::Int, 105});
  d.push_back({"arp_grid", "grid", "Arp/Seq Grid", PART(11), 0, 14, 10, Unit::ArpGrid, 106});
  d.push_back({"seq1_len", "len1", "Seq 1 Length", PART(12), 1, 16, 16, Unit::Int, -1});
  d.push_back({"seq2_len", "len2", "Seq 2 Length", PART(13), 1, 16, 16, Unit::Int, -1});
  d.push_back({"patt_len", "lenp", "Note Pattern Length", PART(14), 1, 16, 16, Unit::Int, -1});
  d.push_back({"poly_mode", "mode", "Polyphony Mode", PART(15), 0, POLYPHONY_MODE_LAST - 1, POLY, Unit::PolyMode, 107});
  // Plugin level.
  d.push_back({pid::board, "card", "Voicecard Type", PLUG, 0, 2, 0, Unit::Board, -1});
  d.push_back({pid::voices, "voic", "Voicecards", PLUG, 1, 6, 6, Unit::Int, -1});
  d.push_back({pid::output, "outp", "Output Level", PLUG, -40, 12, 0, Unit::Decibel, -1});
  d.push_back({pid::noise, "hiss", "Analog Hiss", PLUG, 0, 100, 30, Unit::Percent, -1});
  d.push_back({pid::spread, "ster", "Stereo Spread", PLUG, 0, 100, 0, Unit::Percent, -1});
  d.push_back({pid::drive, "driv", "VCF Input Drive", PLUG, 0, 100, 50, Unit::Percent, -1});
  d.push_back({pid::clockSource, "clk", "Clock Source", PLUG, 0, 1, 1, Unit::ClockSource, -1});
  d.push_back({pid::bpm, "bpm", "Internal BPM", PLUG, 40, 240, 120, Unit::Bpm, -1});
  d.push_back({pid::grooveTemplate, "groo", "Groove Template", PLUG, 0, 5, 0, Unit::Groove, -1});
  d.push_back({pid::grooveAmount, "amnt", "Groove Amount", PLUG, 0, 127, 0, Unit::Raw, -1});
  d.push_back({pid::latch, "ltch", "Clock Latch", PLUG, 0, 8, 4, Unit::Int, -1});
  d.push_back({pid::midiChannel, "chan", "MIDI Channel", PLUG, 0, 16, 0, Unit::MidiChannel, -1});
  return d;
}

const std::vector<ParamDef>& paramDefs() {
  static const std::vector<ParamDef> defs = buildDefs();
  return defs;
}

const ParamDef* findParam(const juce::String& id) {
  for (auto& d : paramDefs())
    if (id == d.id) return &d;
  return nullptr;
}

template <size_t N>
static juce::String pick(const char* const (&table)[N], int v) {
  if (v < 0 || v >= static_cast<int>(N)) return juce::String(v);
  return table[v];
}

juce::String formatValue(const ParamDef& def, int v) {
  switch (def.unit) {
    case Unit::Waveform: return pick(kWaveforms, v);
    case Unit::Operator: return pick(kOperators, v);
    case Unit::SubWaveform: return pick(kSubWaveforms, v);
    case Unit::FilterMode: return pick(kFilterModes, v);
    case Unit::LfoShape:
    case Unit::VoiceLfoShape: return pick(kLfoShapes, v);
    case Unit::LfoRate:
      return v < kNumSyncedLfoRates ? juce::String(kLfoSyncedRates[v]) : juce::String(v - kNumSyncedLfoRates);
    case Unit::LfoSync: return pick(kLfoSync, v);
    case Unit::ModSource: return pick(kModSources, v);
    case Unit::ModDest: return pick(kModDests, v);
    case Unit::ModifierOp: return pick(kModifierOps, v);
    case Unit::Raga: return pick(kRagas, v);
    case Unit::ArpMode: return pick(kArpModes, v);
    case Unit::ArpDirection: return pick(kArpDirections, v);
    case Unit::ArpGrid: return pick(kLfoSyncedRates, v);
    case Unit::PolyMode: return pick(kPolyModes, v);
    case Unit::Boolean: return v ? "on" : "off";
    case Unit::Board: return pick(kBoards, v);
    case Unit::ClockSource: return pick(kClockSources, v);
    case Unit::Groove: return pick(kGrooves, v);
    case Unit::MidiChannel: return v == 0 ? juce::String("omni") : juce::String(v);
    case Unit::Percent: return juce::String(v) + "%";
    case Unit::Decibel: return (v > 0 ? "+" : "") + juce::String(v) + "dB";
    case Unit::Bpm: return juce::String(v);
    default: return juce::String(v);
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  for (auto& def : paramDefs()) {
    const ParamDef* d = &def;
    auto attrs = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([d](int v, int) { return formatValue(*d, v); })
        .withValueFromStringFunction([d](const juce::String& s) {
          for (int v = d->minValue; v <= d->maxValue; ++v)
            if (formatValue(*d, v).equalsIgnoreCase(s.trim())) return v;
          return s.getIntValue();
        });
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{def.id, 1}, def.longName, def.minValue, def.maxValue,
        def.defaultValue, attrs));
  }
  return layout;
}

}  // namespace ambika_plugin
