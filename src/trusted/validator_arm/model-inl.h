/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_INL_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_INL_H

/*
 * Inline definitions for the classes defined in model.h
 */

namespace nacl_arm_dec {

Register::Register(uint32_t number) : number_(number) {}
uint32_t Register::bitmask() const {
  if (number_ == 31) return 0;

  return (1 << number_);
}

uint32_t Register::number() const {
  return number_;
}

bool Register::operator==(const Register &other) const {
  return number_ == other.number_;
}

bool Register::operator!=(const Register &other) const {
  return number_ != other.number_;
}

inline RegisterList operator+(const Register &r1, const Register& r2) {
  RegisterList regs(r1);
  return regs + r2;
}

RegisterList::RegisterList(uint32_t bits) : bits_(bits) {}
RegisterList::RegisterList(Register r) : bits_(r.bitmask()) {}

bool RegisterList::operator[](Register r) const {
  return bits_ & r.bitmask();
}

bool RegisterList::contains_all(const RegisterList other) const {
  return (bits_ & other.bits_) == other.bits_;
}

bool RegisterList::contains_any(const RegisterList other) const {
  return bits_ & other.bits_;
}

const RegisterList RegisterList::operator&(const RegisterList other) const {
  return RegisterList(bits_ & other.bits_);
}

bool RegisterList::operator==(const RegisterList other) const {
  return bits_ == other.bits_;
}

inline const RegisterList operator+(const RegisterList a,
                                    const RegisterList b) {
  return RegisterList(a.bits_ | b.bits_);
}


inline Instruction::Instruction(uint32_t bits) : bits_(bits) {}

inline Instruction::Instruction(uint16_t word) : bits_(word) {}

inline Instruction::Instruction(uint16_t word1, uint16_t word2)
    : bits_((((uint32_t) word2) << kThumbWordSize) | (uint32_t) word1)
{}

inline uint32_t Instruction::bits(int hi, int lo) const {
  /*
   * When both arguments are constant (the usual case), this can be inlined
   * as
   *    ubfx r0, r0, #hi, #(hi+1-lo)
   *
   * Curiously, even at aggressive optimization levels, GCC 4.3.2 generates a
   * less-efficient sequence of ands, bics, and shifts.
   */
  uint32_t right_justified = bits_ >> lo;
  int bit_count = hi - lo + 1;
  uint32_t mask = (1 << bit_count) - 1;
  return right_justified & mask;
}

inline uint16_t Instruction::word1_bits(int hi, int lo) const {
  return (uint16_t) bits(hi, lo);
}

inline uint16_t Instruction::word2_bits(int hi, int lo) const {
  return (uint16_t) bits(hi + kThumbWordSize, lo + kThumbWordSize);
}

inline void Instruction::set_bits(int hi, int lo, uint32_t value) {
  // Compute bit mask for range of bits.
  int bit_count = hi - lo + 1;
  uint32_t clear_mask = (1 << bit_count) - 1;
  // Remove from the value any bits out of range, then shift to location to add.
  value = (value & clear_mask) << lo;
  // Shift mask to corresponding position and replace bits with value.
  clear_mask = ~(clear_mask << lo);
  bits_ = (bits_ & clear_mask) | value;
}

inline void Instruction::word1_set_bits(int hi, int lo, uint16_t value) {
  set_bits(hi, lo, (uint32_t) value);
}

inline void Instruction::word2_set_bits(int hi, int lo, uint16_t value) {
  set_bits(hi + kThumbWordSize, lo + kThumbWordSize, (uint32_t) value);
}

inline const Register Instruction::reg(int hi, int lo) const {
  return Register(bits(hi, lo));
}

inline const Register Instruction::word1_reg(int hi, int lo) const {
  return Register(word1_bits(hi, lo));
}

inline const Register Instruction::word2_reg(int hi, int lo) const {
  return Register(word2_bits(hi, lo));
}

inline bool Instruction::bit(int index) const {
  return (bits_ >> index) & 1;
}

inline bool Instruction::word1_bit(int index) const {
  return bit(index);
}

inline bool Instruction::word2_bit(int index) const {
  return bit(index + kThumbWordSize);
}

inline void Instruction::set_bit(int index, bool value) {
  uint32_t mask = (1 << index);
  if (value) {
    /* Set to 1 */
    bits_ |= mask;
  } else {
    /* Set to 0 */
    bits_ &= ~mask;
  }
}

inline void Instruction::word1_set_bit(int index, bool value) {
  set_bit(index, value);
}

inline void Instruction::word2_set_bit(int index, bool value) {
  set_bit(index + kThumbWordSize, value);
}

inline Instruction::Condition Instruction::condition() const {
  Instruction::Condition cc = Instruction::Condition(bits(31, 28));

  if (cc == Instruction::UNCONDITIONAL) {
    return Instruction::AL;
  }

  return cc;
}

inline uint32_t Instruction::operator&(uint32_t mask) const {
  return bits_ & mask;
}

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_INL_H
