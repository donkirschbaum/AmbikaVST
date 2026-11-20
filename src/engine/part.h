// Ambika VST - the "part" running on the Ambika motherboard.
// Port of controller/{part,note_stack,voice_allocator}.{h,cc},
// Copyright 2011 Emilie Gillet. GPL v3.
#pragma once

#include "avr_compat.h"
#include "patch.h"
#include "voicecard.h"

namespace ambika {

static const uint8_t kFreeSlot = 0xff;

struct NoteEntry { uint8_t note; uint8_t velocity; uint8_t next_ptr; };

template <uint8_t capacity>
class NoteStack {
 public:
  void Init() { Clear(); }
  void NoteOn(uint8_t note, uint8_t velocity) {
    NoteOff(note);
    if (size_ == capacity) {
      uint8_t least_recent_note = 0;
      for (uint8_t i = 1; i <= capacity; ++i)
        if (pool_[i].next_ptr == 0) least_recent_note = pool_[i].note;
      NoteOff(least_recent_note);
    }
    uint8_t free_slot = 0;
    for (uint8_t i = 1; i <= capacity; ++i) {
      if (pool_[i].note == kFreeSlot) { free_slot = i; break; }
    }
    if (!free_slot) return;
    pool_[free_slot].next_ptr = root_ptr_;
    pool_[free_slot].note = note;
    pool_[free_slot].velocity = velocity;
    root_ptr_ = free_slot;
    for (uint8_t i = 0; i < size_; ++i) {
      if (pool_[sorted_ptr_[i]].note > note) {
        for (uint8_t j = size_; j > i; --j) sorted_ptr_[j] = sorted_ptr_[j - 1];
        sorted_ptr_[i] = free_slot;
        free_slot = 0;
        break;
      }
    }
    if (free_slot) sorted_ptr_[size_] = free_slot;
    ++size_;
  }
  void NoteOff(uint8_t note) {
    uint8_t current = root_ptr_, previous = 0;
    while (current) {
      if (pool_[current].note == note) break;
      previous = current;
      current = pool_[current].next_ptr;
    }
    if (current) {
      if (previous) pool_[previous].next_ptr = pool_[current].next_ptr;
      else root_ptr_ = pool_[current].next_ptr;
      for (uint8_t i = 0; i < size_; ++i) {
        if (sorted_ptr_[i] == current) {
          for (uint8_t j = i; j < size_ - 1; ++j) sorted_ptr_[j] = sorted_ptr_[j + 1];
          break;
        }
      }
      pool_[current].next_ptr = 0;
      pool_[current].note = kFreeSlot;
      pool_[current].velocity = 0;
      --size_;
    }
  }
  void Clear() {
    size_ = 0;
    std::memset(pool_, 0, sizeof(pool_));
    std::memset(sorted_ptr_, 0, sizeof(sorted_ptr_));
    root_ptr_ = 0;
    for (uint8_t i = 0; i <= capacity; ++i) pool_[i].note = kFreeSlot;
  }
  uint8_t size() const { return size_; }
  uint8_t max_size() const { return capacity; }
  const NoteEntry& most_recent_note() const { return pool_[root_ptr_]; }
  const NoteEntry& played_note(uint8_t index) const {
    uint8_t current = root_ptr_;
    index = size_ - index - 1;
    for (uint8_t i = 0; i < index; ++i) current = pool_[current].next_ptr;
    return pool_[current];
  }
  const NoteEntry& sorted_note(uint8_t index) const { return pool_[sorted_ptr_[index]]; }
  NoteEntry* mutable_note(uint8_t index) { return &pool_[index]; }
 private:
  uint8_t size_ = 0;
  NoteEntry pool_[capacity + 1];
  uint8_t root_ptr_ = 0;
  uint8_t sorted_ptr_[capacity + 1];
};

class VoiceAllocator {
 public:
  void Init(uint8_t size, bool cyclic_mode) {
    size_ = size;
    cyclic_allocator_ = cyclic_mode ? 0 : 0xff;
    Clear();
  }
  uint8_t NoteOn(uint8_t note);
  uint8_t NoteOff(uint8_t note);
  uint8_t Find(uint8_t note) const;
  uint8_t FindActive(uint8_t note) const;
  void Clear();
  void ClearNotes() { std::memset(pool_, 0, size_); }
  uint8_t size() const { return size_; }
 private:
  void Touch(uint8_t voice);
  uint8_t pool_[16];
  uint8_t lru_[16];
  uint8_t size_ = 0;
  uint8_t cyclic_allocator_ = 0xff;
};

class Part {
 public:
  void Init(Voicecard* voicecards, uint8_t num_voices, uint16_t seed);
  void SetNumVoices(uint8_t n);

  // Called whenever the patch/part data was edited (by the plugin).
  void Touch();

  void NoteOn(uint8_t note, uint8_t velocity);
  void NoteOff(uint8_t note);
  void SustainPedal(bool down);
  void SetModulation(uint8_t source, uint8_t value);
  void Aftertouch(uint8_t note, uint8_t velocity);
  void AllSoundOff();
  void AllNotesOff();
  void ResetAllControllers();

  void Clock();   // 24 ppqn
  void Start();
  void Stop();
  void UpdateLfos(uint8_t refresh_cycle);  // control rate (~980 Hz)

  uint8_t num_pressed_keys() const { return pressed_keys_.size(); }
  uint8_t lfo_value(uint8_t i) const { return lfo_previous_values_[i]; }
  uint8_t step(uint8_t i) const { return sequencer_step_[i]; }

  Patch* mutable_patch() { return &patch_; }
  PartData* mutable_data() { return &data_; }
  const Patch& patch() const { return patch_; }
  const PartData& data() const { return data_; }

 private:
  void InitializeAllocators();
  void TouchClock();
  void TouchLfos();
  void RetriggerLfos();
  void ClockSequencer();
  void ClockArpeggiator();
  void StartArpeggio();
  void StepArpeggio();
  uint8_t AcceptNote(uint8_t note) const;
  uint16_t TuneNote(uint8_t midi_note) const;
  void InternalNoteOn(uint8_t note, uint8_t velocity);
  void InternalNoteOff(uint8_t note);
  uint8_t GetNextVoice(uint8_t index) const { return index + 1 < num_allocated_voices_ ? index + 1 : 0; }
  void WriteModulationToAllVoices(uint8_t source, uint8_t value);

  Voicecard* voices_ = nullptr;
  Patch patch_;
  PartData data_;
  Rng rng_;
  uint8_t num_allocated_voices_ = 0;

  Lfo lfo_[kNumLfos];
  uint8_t lfo_step_[kNumLfos];
  uint8_t lfo_cycle_length_[kNumLfos];
  uint8_t lfo_previous_values_[kNumLfos];
  uint16_t midi_clock_tick_duration_ = 1;
  uint8_t midi_clock_prescaler_ = 1;
  uint8_t midi_clock_counter_ = 0;

  uint8_t ignore_note_off_messages_ = 0;
  NoteStack<12> pressed_keys_;
  NoteStack<7> mono_allocator_;
  VoiceAllocator poly_allocator_;

  uint8_t sequencer_step_[kNumSequences];
  uint8_t previous_generated_note_ = 0xff;
  uint16_t arp_pattern_mask_ = 1;
  int8_t arp_direction_ = 1;
  int8_t arp_step_ = 0;
  int8_t arp_octave_ = 0;
  uint8_t polyphony_mode_ = 0xff;
  uint8_t arp_divider_ = 0xff;
  uint8_t arp_direction_setting_ = 0xff;
  uint8_t lfo_rates_[kNumLfos];
};

}  // namespace ambika
