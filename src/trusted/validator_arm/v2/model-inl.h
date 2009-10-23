/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 * Copyright 2009, Google Inc.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_INL_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_INL_H
/*
 * Inline definitions for the classes defined in model.h
 */

namespace nacl_arm_dec {

Register::Register(uint32_t bits) : _bits(bits) {}
uint32_t Register::bitmask() const {
  return (1 << _bits);
}

bool Register::operator==(const Register &other) const {
  return _bits == other._bits;
}

bool Register::operator!=(const Register &other) const {
  return _bits != other._bits;
}


RegisterList::RegisterList(uint32_t bits) : _bits(bits) {}
RegisterList::RegisterList(Register r) : _bits(r.bitmask()) {}

bool RegisterList::operator[](Register r) const {
  return _bits & r.bitmask();
}

bool RegisterList::contains_all(const RegisterList other) const {
  return (_bits & other._bits) == other._bits;
}

bool RegisterList::contains_any(const RegisterList other) const {
  return _bits & other._bits;
}

const RegisterList RegisterList::operator&(const RegisterList other) const {
  return RegisterList(_bits & other._bits);
}

bool RegisterList::operator==(const RegisterList other) const {
  return _bits == other._bits;
}

const RegisterList operator+(const RegisterList a, const RegisterList b) {
  return RegisterList(a._bits | b._bits);
}


inline Instruction::Instruction(uint32_t bits) : _bits(bits) {}

inline uint32_t Instruction::bits(int hi, int lo) const {
  /*
   * When both arguments are constant (the usual case), this can be inlined
   * as
   *    ubfx r0, r0, #hi, #(hi+1-lo)
   *
   * Curiously, even at aggressive optimization levels, GCC 4.3.2 generates a
   * less-efficient sequence of ands, bics, and shifts.
   */
  uint32_t mask = (1 << (hi + 1)) - 1;
  return (_bits & mask) >> lo;
}

inline const Register Instruction::reg(int hi, int lo) const {
  return Register(bits(hi, lo));
}

inline bool Instruction::bit(int index) const {
  return (_bits >> index) & 1;
}

inline Instruction::Condition Instruction::condition() const {
  return Instruction::Condition(bits(31, 28));
}

inline uint32_t Instruction::operator&(uint32_t mask) const {
  return _bits & mask;
}

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_INL_H
