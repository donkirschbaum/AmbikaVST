// Ambika VST - desktop port of the Mutable Instruments Ambika firmware.
//
// Original firmware: Copyright 2011-2012 Emilie Gillet.
// Port: GPL v3 (see LICENSE).
//
// Portable replacements for avrlib's fixed-point helpers. They reproduce the
// arithmetic of the AVR assembly versions bit-for-bit (8/16 bit wrap-around,
// truncating shifts), which is what gives the voicecard its sound.

#pragma once

#include <cstdint>
#include <cstring>

typedef uint8_t prog_uint8_t;
typedef int8_t prog_int8_t;
typedef uint16_t prog_uint16_t;
typedef int16_t prog_int16_t;
typedef uint32_t prog_uint32_t;
typedef char prog_char;

namespace ambika {

struct uint24_t {
  uint16_t integral;
  uint8_t fractional;
};

struct uint24c_t {
  uint8_t carry;
  uint16_t integral;
  uint8_t fractional;
};

static inline int16_t Clip(int16_t value, int16_t min, int16_t max) {
  return value < min ? min : (value > max ? max : value);
}

static inline int16_t S16ClipU14(int16_t value) {
  uint8_t msb = static_cast<uint16_t>(value) >> 8;
  if (msb & 0x80) return 0;
  if (msb & 0x40) return 16383;
  return value;
}

static inline uint8_t U8AddClip(uint8_t value, uint8_t increment, uint8_t max) {
  value += increment;
  if (value > max) value = max;
  return value;
}

static inline uint24c_t U24AddC(uint24c_t a, uint24_t b) {
  uint32_t av = (static_cast<uint32_t>(a.integral) << 8) + a.fractional;
  uint32_t bv = (static_cast<uint32_t>(b.integral) << 8) + b.fractional;
  uint32_t sum = av + bv;
  uint24c_t r;
  r.integral = static_cast<uint16_t>(sum >> 8);
  r.fractional = static_cast<uint8_t>(sum & 0xff);
  r.carry = (sum & 0xff000000) != 0;
  return r;
}

static inline uint24_t U24Add(uint24_t a, uint24_t b) {
  uint32_t av = (static_cast<uint32_t>(a.integral) << 8) + a.fractional;
  uint32_t bv = (static_cast<uint32_t>(b.integral) << 8) + b.fractional;
  uint32_t sum = av + bv;
  uint24_t r;
  r.integral = static_cast<uint16_t>(sum >> 8);
  r.fractional = static_cast<uint8_t>(sum & 0xff);
  return r;
}

static inline uint24_t U24ShiftRight(uint24_t a) {
  uint32_t av = ((static_cast<uint32_t>(a.integral) << 8) + a.fractional) >> 1;
  uint24_t r;
  r.integral = static_cast<uint16_t>(av >> 8);
  r.fractional = static_cast<uint8_t>(av & 0xff);
  return r;
}

static inline uint24_t U24ShiftLeft(uint24_t a) {
  uint32_t av = ((static_cast<uint32_t>(a.integral) << 8) + a.fractional) << 1;
  uint24_t r;
  r.integral = static_cast<uint16_t>(av >> 8);
  r.fractional = static_cast<uint8_t>(av & 0xff);
  return r;
}

static inline uint8_t S16ClipU8(int16_t v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static inline int8_t S16ClipS8(int16_t v) { return v < -128 ? -128 : (v > 127 ? 127 : v); }

static inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance) {
  return static_cast<uint8_t>((a * (255 - balance) + b * balance) >> 8);
}
static inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t gain_a, uint8_t gain_b) {
  return static_cast<uint8_t>(((a * gain_a + b * gain_b) >> 8) & 0xff);
}
static inline uint16_t U8MixU16(uint8_t a, uint8_t b, uint8_t balance) {
  return static_cast<uint16_t>(a * (255 - balance) + b * balance);
}
static inline uint8_t U8U4MixU8(uint8_t a, uint8_t b, uint8_t balance) {
  return static_cast<uint8_t>((a * (15 - balance) + b * balance) >> 4);
}
static inline uint16_t U8U4MixU12(uint8_t a, uint8_t b, uint8_t balance) {
  return static_cast<uint16_t>(a * (15 - balance) + b * balance);
}
static inline uint8_t U8ShiftRight4(uint8_t a) { return a >> 4; }
static inline uint8_t U8ShiftLeft4(uint8_t a) { return static_cast<uint8_t>(a << 4); }
static inline uint8_t U8Swap4(uint8_t a) { return static_cast<uint8_t>((a << 4) | (a >> 4)); }
static inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b) { return static_cast<uint8_t>((a * b) >> 8); }
static inline int8_t S8U8MulShift8(int8_t a, uint8_t b) { return static_cast<int8_t>((a * b) >> 8); }
static inline int16_t S8U8Mul(int8_t a, uint8_t b) { return static_cast<int16_t>(a * b); }
static inline int16_t S8S8Mul(int8_t a, int8_t b) { return static_cast<int16_t>(a * b); }
static inline uint16_t U8U8Mul(uint8_t a, uint8_t b) { return static_cast<uint16_t>(a * b); }
static inline int8_t S8S8MulShift8(int8_t a, int8_t b) { return static_cast<int8_t>((a * b) >> 8); }
static inline uint8_t U14ShiftRight6(uint16_t v) { return static_cast<uint8_t>(v >> 6); }
static inline uint8_t U15ShiftRight7(uint16_t v) { return static_cast<uint8_t>(v >> 7); }

static inline uint8_t InterpolateSample(const uint8_t* table, uint16_t phase) {
  return U8Mix(table[phase >> 8], table[(phase >> 8) + 1], phase & 0xff);
}

// Galois LFSR used by avrlib::Random. Each voicecard has its own MCU, hence
// its own generator: this is an instance, not a global.
class Rng {
 public:
  void Seed(uint16_t s) { state_ = s ? s : 1; }
  void Update() { state_ = (state_ >> 1) ^ (-(state_ & 1) & 0xb400); }
  uint8_t state_msb() const { return static_cast<uint8_t>(state_ >> 8); }
  uint8_t GetByte() { Update(); return state_msb(); }
  uint16_t state() const { return state_; }
 private:
  uint16_t state_ = 0x21;
};

}  // namespace ambika
