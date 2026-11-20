// Offline test: renders a few patches to WAV files and prints level stats.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/engine/engine.h"

using namespace ambika;

static void WriteWav(const std::string& path, const std::vector<float>& l,
                     const std::vector<float>& r, int sr) {
  FILE* f = std::fopen(path.c_str(), "wb");
  uint32_t n = static_cast<uint32_t>(l.size());
  uint32_t data = n * 4;
  auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); w32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
  w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
  std::fwrite("data", 1, 4, f); w32(data);
  for (uint32_t i = 0; i < n; ++i) {
    int16_t a = static_cast<int16_t>(std::fmax(-1.0, std::fmin(1.0, l[i])) * 32767);
    int16_t b = static_cast<int16_t>(std::fmax(-1.0, std::fmin(1.0, r[i])) * 32767);
    w16(a); w16(b);
  }
  std::fclose(f);
}

struct Test {
  const char* name;
  void (*setup)(Patch*, PartData*, EngineSettings*);
  bool arp;
};

int main(int argc, char** argv) {
  const int sr = 48000;
  std::string out_dir = argc > 1 ? argv[1] : ".";
  Test tests[] = {
    {"init", [](Patch*, PartData*, EngineSettings*) {}, false},
    {"svf_bp_reso", [](Patch* p, PartData*, EngineSettings* s) {
       s->board = BOARD_SVF; p->filter[0].mode = 1; p->filter[0].resonance = 60; p->filter[0].cutoff = 70; }, false},
    {"4p_hp", [](Patch* p, PartData*, EngineSettings* s) {
       s->board = BOARD_4P; p->filter[0].mode = 2; p->filter[0].cutoff = 60; }, false},
    {"smr4_selfosc", [](Patch* p, PartData*, EngineSettings*) {
       p->filter[0].resonance = 63; p->filter[0].cutoff = 50; p->filter_env = 40; }, false},
    {"vowel", [](Patch* p, PartData*, EngineSettings*) {
       p->osc[0].shape = WAVEFORM_VOWEL; p->osc[1].shape = WAVEFORM_NONE; p->filter[0].cutoff = 127;
       p->modulation[0].amount = 40; }, false},
    {"fm_ring", [](Patch* p, PartData*, EngineSettings*) {
       p->osc[0].shape = WAVEFORM_FM; p->osc[0].parameter = 40; p->osc[0].range = 7;
       p->osc[1].shape = WAVEFORM_CZ_SAW_PK; p->mix_op = OP_RING_MOD; p->mix_parameter = 40;
       p->filter[0].cutoff = 110; }, false},
    {"wavetable_pad", [](Patch* p, PartData* d, EngineSettings* s) {
       p->osc[0].shape = WAVEFORM_WAVETABLE_1 + 2; p->osc[1].shape = WAVEFORM_QUAD_SAW_PAD;
       p->osc[1].parameter = 40; p->modulation[0].amount = 30; p->env_lfo[2].attack = 60;
       p->env_lfo[2].release = 80; s->stereo_spread = 1.0f; d->spread = 3; }, false},
    {"arp_sync", [](Patch* p, PartData* d, EngineSettings*) {
       p->osc[1].shape = WAVEFORM_SAW; p->mix_op = OP_SYNC; p->osc[1].range = 12;
       p->modulation[1].amount = 50; d->arp_sequencer_mode = ARP_SEQUENCER_MODE_ARPEGGIATOR;
       d->arp_octave = 2; d->arp_direction = ARPEGGIO_DIRECTION_UP_DOWN; }, true},
    {"crush_fuzz_noise", [](Patch* p, PartData*, EngineSettings*) {
       p->mix_crush = 12; p->mix_fuzz = 40; p->mix_noise = 20; p->mix_sub_osc = 50; p->filter[0].cutoff = 127; }, false},
  };

  for (auto& t : tests) {
    static Engine engine;  // large object
    engine.Init(sr);
    EngineSettings s;
    InitPatch(engine.mutable_patch());
    InitPartData(engine.mutable_data());
    t.setup(engine.mutable_patch(), engine.mutable_data(), &s);
    engine.ApplySettings(s);
    engine.Touch();

    const int len = sr * 4;
    std::vector<float> l(len), r(len);
    GrooveClock clock;
    clock.SetGroove(0, 0);
    double ppq = 0.0, ppq_per_sample = 120.0 / 60.0 / sr;
    const int chunk = 64;
    int chords[3] = {48, 55, 60};
    for (int pos = 0; pos < len; pos += chunk) {
      if (pos == 0) for (int n : chords) engine.NoteOn(n, 100, true);
      if (pos == sr * 2 + 64 * 0) for (int n : chords) engine.NoteOff(n);
      double ticks[16];
      int nt = clock.TicksIn(ppq, ppq + chunk * ppq_per_sample, ticks, 16);
      for (int k = 0; k < nt; ++k) engine.ClockTick(true);
      ppq += chunk * ppq_per_sample;
      engine.Render(&l[pos], &r[pos], chunk);
    }
    double peak = 0, rms = 0; bool bad = false;
    for (int i = 0; i < len; ++i) {
      if (!std::isfinite(l[i]) || !std::isfinite(r[i])) bad = true;
      peak = std::fmax(peak, std::fabs(l[i]));
      rms += l[i] * l[i];
    }
    rms = std::sqrt(rms / len);
    std::printf("%-18s peak %.3f rms %.3f %s\n", t.name, peak, rms, bad ? "NAN!" : "");
    WriteWav(out_dir + "/" + t.name + ".wav", l, r, sr);
  }
  return 0;
}
