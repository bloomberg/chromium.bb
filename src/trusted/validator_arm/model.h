/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_H

/*
 * Models instructions and decode results.
 *
 * Implementation Note:
 * All the classes in this file are designed to be fully inlined as 32-bit
 * (POD) integers.  The goal: to provide a nice, fluent interface for
 * describing instruction bit operations, with zero runtime cost.  Compare:
 *
 *   return (1 << ((insn >> 12) & 0xF)) | (1 << ((insn >> 19) & 0xF));
 *
 *   return insn.reg(15,12) + insn.reg(19,16);
 *
 * Both lines compile to the same machine code, but the second one is easier
 * to read, and eliminates a common error (using X in place of 1 << X).
 *
 * To this end, keep the following in mind:
 *  - Avoid virtual methods.
 *  - Mark all methods as inline.  Small method bodies may be included here,
 *    but anything nontrivial should go in model-inl.h.
 *  - Do not declare destructors.  A destructor causes an object to be passed
 *    on the stack, even when passed by value.  (This may be a GCC bug.)
 *    Adding a destructor to Instruction slowed the decoder down by 10% on
 *    gcc 4.3.3.
 */

#include <stdint.h>

namespace nacl_arm_dec {

class RegisterList;

// A (POD) value class that names a single register.  We could use a typedef
// for this, but it introduces some ambiguity problems because of the
// implicit conversion to/from int.
class Register {
 public:
  explicit inline Register(uint32_t number) : number_(number) {}

  // Note: can't make explicit without introducing compile-time errors
  // on functions returning a Register value.
  inline Register(const Register& r) : number_(r.number_) {}

  // Produces the bitmask used to represent this register, in both RegisterList
  // and ARM's LDM instruction.
  inline uint32_t BitMask() const {
    if (number_ == 31) return 0;
    return (1 << number_);
  }

  // Returns the register number for the register.
  inline uint32_t number() const {
    return number_;
  }

  inline bool Equals(const Register& r) const {
    return number_ == r.number_;
  }

 private:
  uint32_t number_;

  // Disallow assignment.
  Register& operator=(const Register& r);
};

// A special (POD) value used in some cases to indicate that a register field
// is not used.  This is specially chosen to ensure that bitmask() == 0, so
// it can be added to any RegisterList with no effect.
// Note that -1 or 32 can't be used here because C++ doesn't define a portable
// meaning for such shift distances.
const Register kRegisterNone(31);

// The conditions (i.e. APSR N, Z, C, and V) are collectively modeled as r16.
// These bits of the APSR register are separately tracked, so we can
// test when any of the 4 bits (and hence conditional execution) is
// affected. If you need to track other bits in the APSR, add them as
// a separate register.
const Register kConditions(16);

// Registers with special meaning in our model:
const Register kRegisterPc(15);
const Register kRegisterLink(14);
const Register kRegisterStack(13);

// A collection of Registers.  Used to describe the side effects of operations.
//
// Note that this is technically a set, not a list -- but RegisterSet is a
// well-known term that means something else.
class RegisterList {
 public:
  // Defines an empty register list.
  inline RegisterList() : bits_(0) {}

  // Produces a RegisterList that contains the registers specified in the
  // given bitmask.  To indicate rN, the bitmask must include (1 << N).
  explicit inline RegisterList(uint32_t bitmask) : bits_(bitmask) {}

  // Note: can't make explicit without introducing compile-time errors
  // on functions returning a RegisterList value.
  inline RegisterList(const RegisterList& other) : bits_(other.bits_) {}

  // Produces a RegisterList containing a single register.
  explicit inline RegisterList(const Register& r) : bits_(r.BitMask()) {}

  // Checks whether this list contains the given register.
  inline bool Contains(const Register& r) const {
    return bits_ & r.BitMask();
  }

  // Checks whether this list contains all the registers in the operand.
  inline bool ContainsAll(const RegisterList& other) const {
    return (bits_ & other.bits_) == other.bits_;
  }

  // Checks whether this list contains any of the registers in the operand.
  inline bool ContainsAny(const RegisterList& other) const {
    return bits_ & other.bits_;
  }

  inline bool Equals(const RegisterList& other) const {
    return bits_ == other.bits_;
  }

  // Adds a register to the register list.
  inline RegisterList& Add(const Register& r) {
    bits_ |= r.BitMask();
    return *this;
  }

  // Unions this given register list into this.
  inline RegisterList& Union(const RegisterList& other) {
    bits_ |= other.bits_;
    return *this;
  }

  // Intersects the given register list into this.
  inline RegisterList& Intersect(const RegisterList& other) {
    bits_ &= other.bits_;
    return *this;
  }

  // Copies the other register list into this.
  inline RegisterList& Copy(const RegisterList& other) {
    bits_ = other.bits_;
    return *this;
  }

  // Returns the bits defined in the register list.
  inline uint32_t bits() const {
    return bits_;
  }

 private:
  uint32_t bits_;

  // Disallow assignment.
  RegisterList& operator=(const RegisterList& r);
};

// A list containing every possible register, even some we don't define.
// Used exclusively as a bogus scary return value for forbidden instructions.
static const RegisterList kRegisterListEverything(-1);

// The number of bits in an ARM instruction.
static const int kArm32InstSize = 32;

// The number of bits in a word of a THUMB instruction.
static const int kThumbWordSize = 16;


// Models an instruction, either a 32-bit ARM instruction of unspecified type,
// or one word (16-bit) and two word (16-bit) THUMB instructions.
//
// This class is designed for efficiency:
//  - Its public methods for bitfield extraction are short and inline.
//  - It has no vtable, so on 32-bit platforms it's exactly the size of the
//    instruction it models.
//  - API's exist for accessing both ARM (32-bit) instructions and
//    THUMB instructions (which are 1 or two (16-bit) words).
class Instruction {
 public:
  // ****************************
  // Arm 32-bit instruction API *
  // ****************************

  inline Instruction(const Instruction& inst) : bits_(inst.bits_) {}

  // Creates an a 32-bit ARM instruction.
  explicit inline Instruction(uint32_t bits)  : bits_(bits) {}

  // Returns the entire sequence of bits defined by an instruction.
  inline uint32_t Bits() const {
    return bits_;
  }

  // Extracts a range of contiguous bits, right-justifies it, and returns it.
  // Note that the range follows hardware convention, with the high bit first.
  inline uint32_t Bits(int hi, int lo) const {
    // When both arguments are constant (the usual case), this can be inlined
    // as
    //    ubfx r0, r0, #hi, #(hi+1-lo)
    //
    // Curiously, even at aggressive optimization levels, GCC 4.3.2 generates a
    // less-efficient sequence of ands, bics, and shifts.
    uint32_t right_justified = bits_ >> lo;
    int bit_count = hi - lo + 1;
    uint32_t mask = (1 << bit_count) - 1;
    return right_justified & mask;
  }

  // Changes the range of contiguous bits, with the given value.
  // Note: Assumes the value fits, if not, it is truncated.
  inline void SetBits(int hi, int lo, uint32_t value) {
    // Compute bit mask for range of bits.
    int bit_count = hi - lo + 1;
    uint32_t clear_mask = (1 << bit_count) - 1;
    // Remove from the value any bits out of range, then shift to location
    // to add.
    value = (value & clear_mask) << lo;
    // Shift mask to corresponding position and replace bits with value.
    clear_mask = ~(clear_mask << lo);
    bits_ = (bits_ & clear_mask) | value;
  }

  // A convenience method for converting bits to a register.
  inline const Register Reg(int hi, int lo) const {
    return Register(Bits(hi, lo));
  }

  // Extracts a single bit (0 - 31).
  inline bool Bit(int index) const {
    return (bits_ >> index) & 1;
  }

  // Sets the specified bit to the specified value for an ARM instruction.
  inline void SetBit(int index, bool value) {
    uint32_t mask = (1 << index);
    if (value) {
      // Set to 1
      bits_ |= mask;
    } else {
      // Set to 0
      bits_ &= ~mask;
    }
  }

  // Possible values for the condition field, from the spec.  The names are
  // pretty cryptic, but in practice we care only whether condition() == AL or
  // not.
  enum Condition {
    // Values 0-14 -- order is significant!
    EQ = 0, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL,
    UNCONDITIONAL = 0xF  // Equivalent to AL -- converted in our API
  };

  // Extracts the condition field.  UNCONDITIONAL is converted to AL -- in the
  // event that you need to distinguish, (1) make sure that's really true and
  // then (2) explicitly extract bits(31,28).
  inline Condition GetCondition() const {
    Instruction::Condition cc = Instruction::Condition(Bits(31, 28));
    if (cc == Instruction::UNCONDITIONAL) {
      return Instruction::AL;
    }
    return cc;
  }

  // **********************************
  // Arm 16-bit instruction words API *
  // **********************************

  // Creates a 1 word THUMB instruction.
  explicit inline Instruction(uint16_t word)
      : bits_(static_cast<uint32_t>(word)) {}

  // Creates a 2 word THUMB instruction.
  explicit inline Instruction(uint16_t word1, uint16_t word2)
      : bits_(((static_cast<int32_t>(word2) << kThumbWordSize)
               | static_cast<uint32_t>(word1)))
  {}

  // Extracts a range of contiguous bits from the first word of a
  // THUMB instruction , right-justifies it, and returns it.  Note
  // that the range follows hardware convention, with the high bit
  // first.
  inline uint16_t Word1Bits(int hi, int lo) const {
    return static_cast<uint16_t>(Bits(hi, lo));
  }

  // Extracts a range of contiguous bits from the second word of a
  // THUMB instruction , right-justifies it, and returns it.  Note
  // that the range follows hardware convention, with the high bit
  // first.
  inline uint16_t Word2Bits(int hi, int lo) const {
    return
        static_cast<uint16_t>(Bits(hi + kThumbWordSize, lo + kThumbWordSize));
  }

  // Changes the range of contiguous bits of the first word of a THUMB
  // instruction, with the given value.  Note: Assumes the value fits,
  // if not, it is truncated.
  inline void Word1SetBits(int hi, int lo, uint16_t value)  {
    SetBits(hi, lo, (uint32_t) value);
  }

  // Changes the range of contiguous bits of the second word of a THUMB
  // instruction, with the given value.  Note: Assumes the value fits,
  // if not, it is truncated.
  inline void Word2SetBits(int hi, int lo, uint16_t value) {
    SetBits(hi + kThumbWordSize, lo + kThumbWordSize,
             static_cast<uint32_t>(value));
  }

  // A convenience method that extracts the register specified by
  // the corresponding bits of the first word of a THUMB instruction.
  inline const Register Word1Reg(int hi, int lo) const {
    return Register(Word1Bits(hi, lo));
  }

  // A convenience method that extracts the register specified by
  // the corresponding bits of the second word of a THUMB instruction.
  inline const Register Word2Reg(int hi, int lo) const {
    return Register(Word2Bits(hi, lo));
  }

  // Extracts a single bit (0 - 15) from the first word of a
  // THUMB instruction.
  inline bool Word1Bit(int index) const {
    return Bit(index);
  }

  // Extracts a single bit (0 - 15) from the second word of
  // a THUMB instruction.
  inline bool Word2Bit(int index) const {
    return Bit(index + kThumbWordSize);
  }

  // Sets the specified bit of the first word in a THUMB instruction
  // to the corresponding value.
  inline void Word1SetBit(int index, bool value) {
    SetBit(index, value);
  }

  // Sets the specified bit of the second word in a THUMB instruction
  // to the corresponding value.
  inline void Word2SetBit(int index, bool value) {
    SetBit(index + kThumbWordSize, value);
  }

  // Copies insn into this.
  Instruction& Copy(const Instruction& insn) {
    bits_ = insn.bits_;
    return *this;
  }

 private:
  uint32_t bits_;

  // Don't allow implicit assignment.
  Instruction& operator=(const Instruction& insn);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_H
