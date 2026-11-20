// Ambika VST - part / voice allocation / LFO / arpeggiator / sequencer.
// Port of the Ambika controller firmware, Copyright 2011 Emilie Gillet. GPL v3.

#include "part.h"

#include "resources.h"

namespace ambika {

static const uint8_t midi_clock_tick_per_step[15] = {
  96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1
};
static const uint16_t lfo_phase_increment_per_clock_tick[15] = {
  683, 910, 1024, 1365, 1820, 2048, 2731, 4096, 5461, 8192, 10923, 16384, 21845, 32768, 65535
};

// ------------------------------------------------------------- init patch --
void InitPatch(Patch* p) {
  static const uint8_t init[112] = {
    WAVEFORM_SAW, 0, 0, 0,
    WAVEFORM_SQUARE, 32, static_cast<uint8_t>(-12), 12,
    32, OP_SUM, 31, WAVEFORM_SUB_OSC_SQUARE_1, 0, 0, 0, 0,
    96, 0, 0, 0, 0, 0, 24, 0,
    0, 40, 20, 60, LFO_WAVEFORM_TRIANGLE, kNumSyncedLfoRates + 24, 0, 0,
    0, 40, 0, 40, LFO_WAVEFORM_TRIANGLE, kNumSyncedLfoRates + 32, 0, 0,
    0, 40, 100, 40, LFO_WAVEFORM_TRIANGLE, kNumSyncedLfoRates + 48, 0, 0,
    LFO_WAVEFORM_TRIANGLE, 72,
    MOD_SRC_ENV_1, MOD_DST_PARAMETER_1, 0,
    MOD_SRC_ENV_1, MOD_DST_PARAMETER_2, 0,
    MOD_SRC_LFO_1, MOD_DST_OSC_1, 0,
    MOD_SRC_LFO_1, MOD_DST_OSC_2, 0,
    MOD_SRC_LFO_2, MOD_DST_PARAMETER_1, 0,
    MOD_SRC_LFO_2, MOD_DST_PARAMETER_2, 0,
    MOD_SRC_LFO_3, MOD_DST_MIX_BALANCE, 0,
    MOD_SRC_LFO_4, MOD_DST_FILTER_CUTOFF, 0,
    MOD_SRC_SEQ_1, MOD_DST_FILTER_CUTOFF, 0,
    MOD_SRC_SEQ_2, MOD_DST_MIX_BALANCE, 0,
    MOD_SRC_ENV_3, MOD_DST_VCA, 63,
    MOD_SRC_VELOCITY, MOD_DST_VCA, 16,
    MOD_SRC_PITCH_BEND, MOD_DST_OSC_1_2_COARSE, 32,
    MOD_SRC_LFO_4, MOD_DST_OSC_1_2_COARSE, 16,
    MOD_SRC_LFO_1, MOD_SRC_LFO_2, 0,
    MOD_SRC_LFO_2, MOD_SRC_LFO_3, 0,
    MOD_SRC_LFO_3, MOD_SRC_SEQ_1, 0,
    MOD_SRC_SEQ_1, MOD_SRC_SEQ_2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
  };
  std::memcpy(p, init, sizeof(Patch));
}

void InitPartData(PartData* d) {
  static const uint8_t init[84] = {
    120,
    0, 0, 0, 0,
    0, 0, 0,
    0, 1, 0, 10,
    16, 16, 16, POLY,
    0xff, 0xff, 0x80, 0x80, 0xcc, 0xcc, 0x20, 0x20,
    0x00, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xff,
    0x00, 0x10, 0x20, 0x40, 0x80, 0xff, 0x80, 0x40,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x80, 0x40,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48, 100,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48, 100,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48 | 0x80, 100 | 0x80,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48, 100,
    0, 0, 0, 0,
  };
  std::memcpy(d, init, sizeof(PartData));
}

// --------------------------------------------------------- voice allocator --
void VoiceAllocator::Clear() {
  ClearNotes();
  for (uint8_t i = 0; i < size_; ++i) lru_[i] = size_ - i - 1;
}

uint8_t VoiceAllocator::NoteOn(uint8_t note) {
  if (size_ == 0) return 0xff;
  uint8_t voice = 0xff;
  if (cyclic_allocator_ == 0xff) {
    voice = Find(note);
    if (voice == 0xff) {
      for (uint8_t i = 0; i < size_; ++i)
        if (lru_[i] < size_ && !(pool_[lru_[i]] & 0x80)) voice = lru_[i];
    }
    if (voice == 0xff) {
      for (uint8_t i = 0; i < size_; ++i)
        if (lru_[i] < size_) voice = lru_[i];
    }
  } else {
    ++cyclic_allocator_;
    if (cyclic_allocator_ >= size_) cyclic_allocator_ = 0;
    voice = cyclic_allocator_;
  }
  pool_[voice] = 0x80 | note;
  Touch(voice);
  return voice;
}

uint8_t VoiceAllocator::Find(uint8_t note) const {
  for (uint8_t i = 0; i < size_; ++i)
    if ((pool_[i] & 0x7f) == note) return i;
  return 0xff;
}

uint8_t VoiceAllocator::FindActive(uint8_t note) const {
  for (uint8_t i = 0; i < size_; ++i)
    if ((pool_[i] & 0x7f) == note && (pool_[i] & 0x80)) return i;
  return 0xff;
}

uint8_t VoiceAllocator::NoteOff(uint8_t note) {
  uint8_t voice = Find(note);
  if (cyclic_allocator_ == 0xff) {
    if (voice != 0xff) { pool_[voice] &= 0x7f; Touch(voice); }
  } else {
    if (voice != 0xff) pool_[voice] = 0xff;
  }
  return voice;
}

void VoiceAllocator::Touch(uint8_t voice) {
  int8_t source = size_ - 1, destination = size_ - 1;
  while (source >= 0) {
    if (lru_[source] != voice) lru_[destination--] = lru_[source];
    --source;
  }
  lru_[0] = voice;
}

// -------------------------------------------------------------------- part --
void Part::Init(Voicecard* voicecards, uint8_t num_voices, uint16_t seed) {
  voices_ = voicecards;
  rng_.Seed(seed);
  InitPatch(&patch_);
  InitPartData(&data_);
  pressed_keys_.Init();
  mono_allocator_.Init();
  std::memset(lfo_step_, 0, sizeof(lfo_step_));
  std::memset(lfo_cycle_length_, 1, sizeof(lfo_cycle_length_));
  std::memset(lfo_previous_values_, 0, sizeof(lfo_previous_values_));
  std::memset(sequencer_step_, 0, sizeof(sequencer_step_));
  std::memset(lfo_rates_, 0xff, sizeof(lfo_rates_));
  for (auto& l : lfo_) l.Init();
  num_allocated_voices_ = num_voices;
  InitializeAllocators();
  Touch();
}

void Part::SetNumVoices(uint8_t n) {
  if (n == num_allocated_voices_) return;
  AllSoundOff();
  num_allocated_voices_ = n;
  InitializeAllocators();
}

void Part::Touch() {
  // Propagate patch & part settings to the voicecards (the hardware sends
  // each edit over SPI; here we simply copy the structures).
  for (uint8_t i = 0; i < kNumVoices; ++i) {
    std::memcpy(voices_[i].mutable_patch(), &patch_, sizeof(Patch));
    VoicePartSettings* s = voices_[i].mutable_part();
    s->volume = data_.volume;
    s->legato = data_.legato;
    s->portamento_time = data_.portamento_time;
  }
  if (data_.polyphony_mode != polyphony_mode_) {
    AllSoundOff();
    InitializeAllocators();
  }
  if (data_.arp_divider != arp_divider_) TouchClock();
  bool lfo_changed = false;
  for (uint8_t i = 0; i < kNumLfos; ++i)
    if (patch_.env_lfo[i].rate != lfo_rates_[i]) lfo_changed = true;
  if (lfo_changed) TouchLfos();
  if (data_.arp_direction != arp_direction_setting_) {
    arp_direction_setting_ = data_.arp_direction;
    arp_direction_ = (data_.arp_direction == ARPEGGIO_DIRECTION_DOWN ? -1 : 1);
    StartArpeggio();
  }
}

void Part::TouchClock() {
  arp_divider_ = data_.arp_divider;
  midi_clock_prescaler_ = midi_clock_tick_per_step[data_.arp_divider % 15];
}

void Part::TouchLfos() {
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    lfo_rates_[i] = patch_.env_lfo[i].rate;
    if (patch_.env_lfo[i].rate < kNumSyncedLfoRates) {
      lfo_cycle_length_[i] = midi_clock_tick_per_step[patch_.env_lfo[i].rate];
    } else {
      uint8_t r = patch_.env_lfo[i].rate - kNumSyncedLfoRates;
      lfo_[i].set_phase_increment(ctl::lut_res_lfo_increments[r & 0x7f]);
    }
  }
}

void Part::InitializeAllocators() {
  if (data_.polyphony_mode == MONO) {
    mono_allocator_.Init();
  } else {
    uint8_t size = num_allocated_voices_;
    if (data_.polyphony_mode == UNISON_2X) size = (num_allocated_voices_ + 1) >> 1;
    poly_allocator_.Init(size, data_.polyphony_mode == CYCLIC);
  }
  polyphony_mode_ = data_.polyphony_mode;
}

void Part::NoteOn(uint8_t note, uint8_t velocity) {
  if (!AcceptNote(note)) return;
  if (velocity == 0) { NoteOff(note); return; }
  pressed_keys_.NoteOn(note, velocity);
  if (data_.arp_sequencer_mode == ARP_SEQUENCER_MODE_STEP) InternalNoteOn(note, velocity);
}

void Part::NoteOff(uint8_t note) {
  if (!AcceptNote(note)) return;
  if (ignore_note_off_messages_) {
    for (uint8_t i = 1; i <= pressed_keys_.max_size(); ++i) {
      NoteEntry* e = pressed_keys_.mutable_note(i);
      if (e->note == note && e->velocity) e->velocity |= 0x80;
    }
    return;
  }
  pressed_keys_.NoteOff(note);
  if (data_.arp_sequencer_mode == ARP_SEQUENCER_MODE_STEP ||
      (data_.arp_sequencer_mode == ARP_SEQUENCER_MODE_ARPEGGIATOR &&
       data_.arp_direction == ARPEGGIO_DIRECTION_CHORD)) {
    InternalNoteOff(note);
  } else if (pressed_keys_.size() == 0) {
    ignore_note_off_messages_ = 0;
    AllNotesOff();
  }
}

void Part::SustainPedal(bool down) {
  if (down) {
    ignore_note_off_messages_ = 1;
  } else {
    ignore_note_off_messages_ = 0;
    for (uint8_t i = 1; i <= pressed_keys_.max_size(); ++i) {
      NoteEntry* e = pressed_keys_.mutable_note(i);
      if (e->note != kFreeSlot && (e->velocity & 0x80)) NoteOff(e->note);
    }
  }
}

void Part::SetModulation(uint8_t source, uint8_t value) {
  WriteModulationToAllVoices(source, value);
}

void Part::WriteModulationToAllVoices(uint8_t source, uint8_t value) {
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) voices_[i].set_modulation_source(source, value);
}

void Part::Aftertouch(uint8_t note, uint8_t velocity) {
  uint8_t mode = data_.polyphony_mode;
  if (mode == POLY || mode == CYCLIC || mode == CHAIN) {
    uint8_t voice_index = poly_allocator_.Find(note);
    if (voice_index < poly_allocator_.size())
      voices_[voice_index].set_modulation_source(MOD_SRC_AFTERTOUCH, velocity);
  } else if (mode == UNISON_2X) {
    uint8_t voice_index = poly_allocator_.Find(note);
    if (voice_index < poly_allocator_.size()) {
      voice_index <<= 1;
      voices_[voice_index].set_modulation_source(MOD_SRC_AFTERTOUCH, velocity);
      voices_[GetNextVoice(voice_index)].set_modulation_source(MOD_SRC_AFTERTOUCH, velocity);
    }
  } else {
    WriteModulationToAllVoices(MOD_SRC_AFTERTOUCH, velocity);
  }
}

void Part::AllSoundOff() {
  if (data_.polyphony_mode == MONO) mono_allocator_.Clear();
  else poly_allocator_.Clear();
  pressed_keys_.Clear();
  for (uint8_t i = 0; i < kNumVoices; ++i) voices_[i].Kill();
}

void Part::AllNotesOff() {
  if (ignore_note_off_messages_) return;
  if (data_.polyphony_mode == MONO) mono_allocator_.Clear();
  else poly_allocator_.ClearNotes();
  pressed_keys_.Clear();
  for (uint8_t i = 0; i < kNumVoices; ++i) voices_[i].Release();
  previous_generated_note_ = 0xff;
}

void Part::ResetAllControllers() {
  ignore_note_off_messages_ = 0;
  for (uint8_t i = 0; i < kNumVoices; ++i) voices_[i].ResetAllControllers();
}

void Part::Clock() {
  ++midi_clock_counter_;
  if (midi_clock_counter_ >= midi_clock_prescaler_) {
    midi_clock_counter_ = 0;
    ClockSequencer();
    ClockArpeggiator();
  }
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    if (patch_.env_lfo[i].rate < kNumSyncedLfoRates) {
      ++lfo_step_[i];
      if (lfo_step_[i] >= lfo_cycle_length_[i]) lfo_step_[i] = 0;
      uint16_t increment = lfo_phase_increment_per_clock_tick[patch_.env_lfo[i].rate];
      uint16_t lfo_phase = static_cast<uint16_t>(increment * lfo_step_[i]);
      lfo_[i].set_phase(lfo_phase);
      lfo_[i].set_phase_increment(increment / (midi_clock_tick_duration_ ? midi_clock_tick_duration_ : 1));
    }
  }
  midi_clock_tick_duration_ = 0;
}

void Part::Start() {
  std::memset(sequencer_step_, 0, kNumSequences);
  std::memset(lfo_step_, 0, kNumLfos);
  midi_clock_counter_ = midi_clock_prescaler_;
  previous_generated_note_ = 0xff;
  arp_pattern_mask_ = 0x1;
  arp_direction_ = (data_.arp_direction == ARPEGGIO_DIRECTION_DOWN ? -1 : 1);
  StartArpeggio();
}

void Part::Stop() {
  ignore_note_off_messages_ = 0;
  if (data_.arp_sequencer_mode != ARP_SEQUENCER_MODE_STEP) AllNotesOff();
}

uint16_t Part::TuneNote(uint8_t midi_note) const {
  int16_t n = Clip(static_cast<int16_t>(midi_note) + S8U8Mul(data_.octave, 12), 0, 127);
  int16_t note = static_cast<int16_t>(U8U8Mul(static_cast<uint8_t>(n), 128));
  if (data_.raga) {
    const int16_t* scale = reinterpret_cast<const int16_t*>(
        ctl::lookup_table_table[ctl::LUT_RES_SCALE_JUST + ((data_.raga - 1) & 31)]);
    note += scale[midi_note % 12];
  }
  note += data_.tuning;
  return static_cast<uint16_t>(note);
}

uint8_t Part::AcceptNote(uint8_t midi_note) const {
  if (data_.raga) {
    const int16_t* scale = reinterpret_cast<const int16_t*>(
        ctl::lookup_table_table[ctl::LUT_RES_SCALE_JUST + ((data_.raga - 1) & 31)]);
    if (scale[midi_note % 12] == 32767) return 0;
  }
  return 1;
}

void Part::InternalNoteOn(uint8_t note, uint8_t velocity) {
  if (!AcceptNote(note)) return;
  velocity <<= 1;  // voicecard_tx.Trigger sends velocity << 1
  uint8_t retrigger_lfos = 0;
  if (data_.polyphony_mode == MONO) {
    mono_allocator_.NoteOn(note, velocity);
    uint16_t tuned_note = TuneNote(note);
    uint8_t legato = mono_allocator_.size() > 1;
    uint8_t pitch_drift = 0;
    for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
      voices_[i].Trigger(tuned_note + pitch_drift, velocity, legato);
      pitch_drift += data_.spread;
    }
    retrigger_lfos = !legato || !data_.legato;
  } else {
    uint8_t voice_index = poly_allocator_.FindActive(note);
    if (voice_index == 0xff) voice_index = poly_allocator_.NoteOn(note);
    if (data_.polyphony_mode == UNISON_2X) {
      if (voice_index < poly_allocator_.size()) {
        voice_index <<= 1;
        uint16_t tuned_note = TuneNote(note);
        voices_[voice_index].Trigger(tuned_note, velocity, 0);
        voices_[GetNextVoice(voice_index)].Trigger(tuned_note + data_.spread, velocity, 0);
        retrigger_lfos = 1;
      }
    } else {
      // POLY, CYCLIC and CHAIN (no further Ambika to chain to: acts as poly).
      if (voice_index < poly_allocator_.size()) {
        uint16_t tuned_note = TuneNote(note);
        voices_[voice_index].Trigger(tuned_note + voice_index * data_.spread, velocity, 0);
        retrigger_lfos = 1;
      }
    }
  }
  if (retrigger_lfos) RetriggerLfos();
}

void Part::InternalNoteOff(uint8_t note) {
  if (note == 0xff) return;
  uint8_t retrigger_lfos = 0;
  if (data_.polyphony_mode == MONO) {
    uint8_t top_note = mono_allocator_.most_recent_note().note;
    mono_allocator_.NoteOff(note);
    if (mono_allocator_.size() == 0) {
      for (uint8_t i = 0; i < num_allocated_voices_; ++i) voices_[i].Release();
    } else if (top_note == note) {
      uint16_t tuned_note = TuneNote(mono_allocator_.most_recent_note().note);
      uint8_t pitch_drift = 0;
      for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
        voices_[i].Trigger(tuned_note + pitch_drift,
                           mono_allocator_.most_recent_note().velocity, 1);
        pitch_drift += data_.spread;
      }
      retrigger_lfos = !data_.legato;
    }
  } else {
    uint8_t voice_index = poly_allocator_.NoteOff(note);
    if (data_.polyphony_mode == UNISON_2X) {
      if (voice_index < poly_allocator_.size()) {
        voice_index <<= 1;
        voices_[voice_index].Release();
        voices_[GetNextVoice(voice_index)].Release();
      }
    } else if (voice_index < poly_allocator_.size()) {
      voices_[voice_index].Release();
    }
  }
  if (retrigger_lfos) RetriggerLfos();
}

void Part::UpdateLfos(uint8_t refresh_cycle) {
  ++midi_clock_tick_duration_;
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    uint8_t new_lfo_value = lfo_[i].Render(patch_.env_lfo[i].shape % LFO_WAVEFORM_LAST,
                                           &rng_, ctl::wav_res_lfo_waveforms);
    // LFO2 and LFO3 are refreshed at half the control rate on the hardware.
    if (i == 0 || refresh_cycle) {
      if (new_lfo_value != lfo_previous_values_[i])
        WriteModulationToAllVoices(MOD_SRC_LFO_1 + i, new_lfo_value);
      lfo_previous_values_[i] = new_lfo_value;
    }
    if (patch_.env_lfo[i].retrigger_mode == LFO_SYNC_MODE_MASTER) {
      uint8_t lfo_looped = lfo_[i].looped();
      if (patch_.env_lfo[i].rate < kNumSyncedLfoRates) lfo_looped = lfo_step_[i] == 0;
      if (lfo_looped) {
        for (uint8_t j = 0; j < num_allocated_voices_; ++j) voices_[j].TriggerEnvelope(i, ATTACK);
      }
    }
  }
}

void Part::RetriggerLfos() {
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    if (patch_.env_lfo[i].retrigger_mode == LFO_SYNC_MODE_SLAVE) {
      lfo_[i].set_phase(0);
      lfo_step_[i] = 0;
    }
  }
}

void Part::ClockSequencer() {
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t value = data_.step_value(i, sequencer_step_[i]);
    if (data_.sequence_length[i]) WriteModulationToAllVoices(MOD_SRC_SEQ_1 + i, value);
  }
  if (data_.arp_sequencer_mode == ARP_SEQUENCER_MODE_NOTE &&
      pressed_keys_.size() && data_.sequence_length[2]) {
    NoteStep n = data_.note_step(sequencer_step_[2]);
    uint8_t note = static_cast<uint8_t>(Clip(
        static_cast<int16_t>(n.note) + pressed_keys_.most_recent_note().note - 60, 0, 127));
    if (!n.gate) {
      InternalNoteOff(previous_generated_note_);
      previous_generated_note_ = 0xff;
    } else {
      if (!n.legato) {
        InternalNoteOff(previous_generated_note_);
        InternalNoteOn(note, n.velocity & 0x7f);
      } else if (previous_generated_note_ != note) {
        InternalNoteOn(note, n.velocity & 0x7f);
        InternalNoteOff(previous_generated_note_);
      }
      previous_generated_note_ = note;
    }
  }
  for (uint8_t i = 0; i < kNumSequences; ++i) {
    ++sequencer_step_[i];
    if (sequencer_step_[i] >= data_.sequence_length[i]) sequencer_step_[i] = 0;
  }
}

void Part::ClockArpeggiator() {
  uint16_t pattern = ctl::lut_res_arpeggiator_patterns[data_.arp_pattern % kNumArpeggiatorPatterns];
  uint8_t has_arpeggiator_note = (arp_pattern_mask_ & pattern) ? 255 : 0;
  WriteModulationToAllVoices(MOD_SRC_ARP_STEP, has_arpeggiator_note);

  if (data_.arp_sequencer_mode == ARP_SEQUENCER_MODE_ARPEGGIATOR) {
    if (pressed_keys_.size() && has_arpeggiator_note) {
      if (data_.arp_direction != ARPEGGIO_DIRECTION_CHORD) {
        InternalNoteOff(previous_generated_note_);
        StepArpeggio();
        const NoteEntry* arpeggio_note = &pressed_keys_.sorted_note(arp_step_);
        if (data_.arp_direction == ARPEGGIO_DIRECTION_AS_PLAYED)
          arpeggio_note = &pressed_keys_.played_note(arp_step_);
        int16_t note = arpeggio_note->note;
        uint8_t velocity = arpeggio_note->velocity & 0x7f;
        note += 12 * arp_octave_;
        while (note > 127) note -= 12;
        InternalNoteOn(static_cast<uint8_t>(note), velocity);
        previous_generated_note_ = static_cast<uint8_t>(note);
      } else {
        for (uint8_t i = 0; i < pressed_keys_.size(); ++i) {
          const NoteEntry* n = &pressed_keys_.sorted_note(i);
          InternalNoteOn(n->note, n->velocity & 0x7f);
        }
        previous_generated_note_ = 60;
      }
    } else {
      if (data_.arp_direction != ARPEGGIO_DIRECTION_CHORD) {
        InternalNoteOff(previous_generated_note_);
      } else if (previous_generated_note_ != 0xff) {
        for (uint8_t i = 0; i < pressed_keys_.size(); ++i)
          InternalNoteOff(pressed_keys_.sorted_note(i).note);
      }
      previous_generated_note_ = 0xff;
    }
  }
  arp_pattern_mask_ <<= 1;
  if (!arp_pattern_mask_) arp_pattern_mask_ = 1;
}

void Part::StartArpeggio() {
  if (arp_direction_ == 1) {
    arp_octave_ = 0;
    arp_step_ = 0;
  } else {
    arp_step_ = pressed_keys_.size() - 1;
    arp_octave_ = data_.arp_octave - 1;
  }
}

void Part::StepArpeggio() {
  uint8_t num_notes = pressed_keys_.size();
  uint8_t arp_octaves = data_.arp_octave ? data_.arp_octave : 1;
  if (data_.arp_direction == ARPEGGIO_DIRECTION_RANDOM) {
    uint8_t random_byte = rng_.GetByte();
    arp_octave_ = random_byte & 0xf;
    arp_step_ = (random_byte & 0xf0) >> 4;
    while (arp_octave_ >= arp_octaves) arp_octave_ -= arp_octaves;
    while (arp_step_ >= num_notes) arp_step_ -= num_notes;
  } else {
    arp_step_ += arp_direction_;
    uint8_t change_octave = 0;
    if (arp_step_ >= num_notes) { arp_step_ = 0; change_octave = 1; }
    else if (arp_step_ < 0) { arp_step_ = num_notes - 1; change_octave = 1; }
    if (change_octave) {
      arp_octave_ += arp_direction_;
      if (arp_octave_ >= arp_octaves || arp_octave_ < 0) {
        if (data_.arp_direction == ARPEGGIO_DIRECTION_UP_DOWN) {
          arp_direction_ = -arp_direction_;
          StartArpeggio();
          if (num_notes > 1 || arp_octaves > 1) StepArpeggio();
        } else {
          StartArpeggio();
        }
      }
    }
  }
  if (arp_step_ < 0 || arp_step_ >= num_notes) arp_step_ = 0;
}

}  // namespace ambika
