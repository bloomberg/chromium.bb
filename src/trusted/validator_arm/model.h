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
#include <cstddef>
#include <string>
#include "native_client/src/include/portability_bits.h"

namespace nacl_arm_dec {

class RegisterList;

// Defines the architecture version of the ARM processor. Currently assumes
// always 7.
// TODO(karl): Generalize this to handle multiple versions, once we know how
//             to do this.
inline uint32_t ArchVersion() {
  return 7;
}

// A (POD) value class that names a single register.  We could use a typedef
// for this, but it introduces some ambiguity problems because of the
// implicit conversion to/from int.
//
// The 32-bit ARM v7 ARM and Thumb instruction sets have:
//   - 15 32-bit GPRs, with:
//       - R13/R14/R15 serving as SP/LR/PC.
//       - R8-R15 being inaccessible in most 16-bit Thumb instructions.
//   - FPRs, with:
//       - 16 128-bit quadword registers, denoted Q0-Q15.
//       - 32 64-bit doubleword registers, denoted D0-D31.
//       - 32 32-bit single word registers, denoted S0-S31.
//       - Note that the above holds true for only some ARM processors:
//         different VFP implementations might have only D0-D15 with S0-S31,
//         and Advanced SIMD support is required for the quadword registers.
//       - The above FPRs are overlaid and accessing S/D/Q registers
//         interchangeably is sometimes expected by the ARM ISA.
//         Specifically S0-S3 correspond to D0-D1 as well as Q0,
//         S4-S7 to D2-D3 and Q1, and so on.
//         D16-D31 and Q8-Q15 do not correspond to any single word register.
//
// TODO(jfb) detail aarch64 when we support ARM v8.
class Register {
 public:
  typedef uint8_t Number;
  // RegisterMask wide enough for aarch32, including "special" registers.
  typedef uint32_t Mask;

  // TODO(jfb) Need different numbers for aarch64.
  static const Number kNumberConditions = 16;
  static const Number kNumberCondsDontCare = 17;
  static const Number kNumberNone = 32;  // Out of GPR and FPR range.

  Register() : number_(kNumberNone) {}
  explicit Register(Number number) : number_(number) {}
  Register(const Register& r) : number_(r.number_) {}

  // Produces the bitmask used to represent this register, in both RegisterList
  // and ARM's LDM instruction.
  Mask BitMask() const {
    return (number_ == kNumberNone) ? 0 : (1u << number_);
  }

  Number number() const { return number_; }
  bool Equals(const Register& r) const { return number_ == r.number_; }

  Register& Copy(const Register& r) {
    number_ = r.number_;
    return *this;
  }

 private:
  Number number_;
  Register& operator=(const Register& r);  // Disallow assignment.
};

// A special (POD) value used in some cases to indicate that a register field
// is not used.  This is specially chosen to ensure that bitmask() == 0, so
// it can be added to any RegisterList with no effect.
static const Register kRegisterNone(Register::kNumberNone);

// The conditions (i.e. APSR N, Z, C, and V) are collectively modeled as r16.
// These bits of the APSR register are separately tracked, so we can
// test when any of the 4 bits (and hence conditional execution) is
// affected. If you need to track other bits in the APSR, add them as
// a separate register.
//
static const Register kConditions(Register::kNumberConditions);

// Registers with special meaning in our model:
// TODO(jfb) This is different in aarch64.
static const Register kRegisterPc(15);
static const Register kRegisterLink(14);
static const Register kRegisterStack(13);

// For most class decoders, we don't care what the instruction does, other
// than if it is safe, and what general purpose registers are changed.
// Hence, we may have simplified the "defs" virtual of a class decoder
// to always return kConditions (rather than accurately modeling if and
// when it gets updated).
//
// Note: Do not add kCondsDontCareFlag to a RegisterList. Rather,
// use the constant kCondsDontCare.
//
// TODO(jfb) Need a different number for aarch64.
static const Register kCondsDontCareFlag(Register::kNumberCondsDontCare);

// A collection of Registers.  Used to describe the side effects of operations.
//
// Note that this is technically a set, not a list -- but RegisterSet is a
// well-known term that means something else.
class RegisterList {
 public:
  // Defines an empty register list.
  RegisterList() : bits_(0) {}

  // Produces a RegisterList that contains the registers specified in the
  // given bitmask.  To indicate rN, the bitmask must include (1 << N).
  explicit RegisterList(Register::Mask bitmask) : bits_(bitmask) {}

  RegisterList(const RegisterList& other) : bits_(other.bits_) {}

  // Produces a RegisterList containing a single register.
  explicit RegisterList(const Register& r) : bits_(r.BitMask()) {}

  // Checks whether this list contains the given register.
  bool Contains(const Register& r) const {
    return bits_ & r.BitMask();
  }

  // Checks whether this list contains all the registers in the operand.
  bool ContainsAll(const RegisterList& other) const {
    return (bits_ & other.bits_) == other.bits_;
  }

  // Checks whether this list contains any of the registers in the operand.
  bool ContainsAny(const RegisterList& other) const {
    return bits_ & other.bits_;
  }

  bool Equals(const RegisterList& other) const {
    return bits_ == other.bits_;
  }

  // Adds a register to the register list.
  RegisterList& Add(const Register& r) {
    bits_ |= r.BitMask();
    return *this;
  }

  // Removes a register from the register list.
  RegisterList& Remove(const Register& r) {
    bits_ &= ~r.BitMask();
    return *this;
  }

  // Unions this given register list into this.
  RegisterList& Union(const RegisterList& other) {
    bits_ |= other.bits_;
    return *this;
  }

  // Intersects the given register list into this.
  RegisterList& Intersect(const RegisterList& other) {
    bits_ &= other.bits_;
    return *this;
  }

  // Copies the other register list into this.
  RegisterList& Copy(const RegisterList& other) {
    bits_ = other.bits_;
    return *this;
  }

  // Returns the bits defined in the register list.
  Register::Mask bits() const {
    return bits_;
  }

  // Number of ARM GPRs in the list.
  // TODO(jfb) Update for aarch64.
  uint32_t numGPRs() const {
    // aarch32 only has 16 GPRs, we steal other registers for flags and such.
    uint16_t gprs = bits_ & 0xFFFF;
    return nacl::PopCount(gprs);
  }

 private:
  Register::Mask bits_;
  RegisterList& operator=(const RegisterList& r);  // Disallow assignment.
};

// A list containing every possible register, even some we don't define.
// Used exclusively as a bogus scary return value for forbidden instructions.
static const RegisterList kRegisterListEverything(-1);

// A special register list to communicate that we don't care about conditions
// for the given class decoder. Note: This should only be added to register
// lists returned from virtual ClassDecoder::defs, and only for actual
// class decoders. It is used to communicate to class decoder testers
// that the actual class decoder is not tracking conditions.
static const RegisterList kCondsDontCare((1 << Register::kNumberConditions) |
                                         (1 << Register::kNumberCondsDontCare));

// The number of bits in an ARM instruction.
static const int kArm32InstSize = 32;

// The number of bits in a word of a THUMB instruction.
static const int kThumbWordSize = 16;


// Models an instruction, either a 32-bit ARM instruction of unspecified type,
// or one word (16-bit) and two word (32-bit) THUMB instructions.
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

  Instruction() : bits_(0) {}

  Instruction(const Instruction& inst) : bits_(inst.bits_) {}

  // Creates an a 32-bit ARM instruction.
  explicit Instruction(uint32_t bits)  : bits_(bits) {}

  // Returns the entire sequence of bits defined by an instruction.
  uint32_t Bits() const {
    return bits_;
  }

  // Extracts a range of contiguous bits, right-justifies it, and returns it.
  // Note that the range follows hardware convention, with the high bit first.
  uint32_t Bits(int hi, int lo) const {
    // When both arguments are constant (the usual case), this can be inlined
    // as
    //    ubfx r0, r0, #hi, #(hi+1-lo)
    //
    // Curiously, even at aggressive optimization levels, GCC 4.3.2 generates a
    // less-efficient sequence of ands, bics, and shifts.
    //
    // TODO(jfb) Validate and fix this: this function is called very often and
    //           could speed up validation quite a bit.
    uint32_t right_justified = bits_ >> lo;
    int bit_count = hi - lo + 1;
    uint32_t mask = (1 << bit_count) - 1;
    return right_justified & mask;
  }

  // Changes the range of contiguous bits, with the given value.
  // Note: Assumes the value fits, if not, it is truncated.
  void SetBits(int hi, int lo, uint32_t value) {
    // TODO(jfb) Same as the above function, this should generate a BFI
    //           when hi and lo are compile-time constants.
    //
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
  const Register Reg(int hi, int lo) const {
    return Register(Bits(hi, lo));
  }

  // Extracts a single bit (0 - 31).
  bool Bit(int index) const {
    return (bits_ >> index) & 1;
  }

  // Sets the specified bit to the specified value for an ARM instruction.
  void SetBit(int index, bool value) {
    uint32_t mask = (1 << index);
    if (value) {
      // Set to 1
      bits_ |= mask;
    } else {
      // Set to 0
      bits_ &= ~mask;
    }
  }

  // Possible values for the condition field, from the ARM ARM section A8.3.
  // Conditional execution is determined by the APSR's condition flags: NZCV.
  enum Condition {
    EQ = 0x0,  // Equal                         |  Z == 1
    NE = 0x1,  // Not equal                     |  Z == 0
    CS = 0x2,  // Carry set                     |  C == 1
    CC = 0x3,  // Carry clear                   |  C == 0
    MI = 0x4,  // Minus, negative               |  N == 1
    PL = 0x5,  // Plus, positive or zero        |  N == 0
    VS = 0x6,  // Overflow                      |  V == 1
    VC = 0x7,  // No overflow                   |  V == 0
    HI = 0x8,  // Unsigned higher               |  C == 1 && Z == 0
    LS = 0x9,  // Unsigned lower or same        |  C == 0 || Z == 1
    GE = 0xA,  // Signed greater than or equal  |  N == V
    LT = 0xB,  // Signed less than              |  N != V
    GT = 0xC,  // Signed greater than           |  Z == 0 && N == V
    LE = 0xD,  // Signed less than or equal     |  Z == 1 || N != V
    AL = 0xE,  // Always (unconditional)        |  Any
    UNCONDITIONAL = 0xF,  // Equivalent to AL -- converted in our API
    // Aliases:
    HS = CS,  // Unsigned higher or same
    LO = CC   // Unsigned lower
  };

  // Defines the size of enumerated type Condition, minus
  // UNCONDITIONAL (assuming one uses GetCondition() to get the
  // condition of an instruction).
  static const size_t kConditionSize = 15;

  static const char* ToString(Condition cond);

  static Condition Next(Condition cond) {
    return static_cast<Condition>(static_cast<int>(cond) + 1);
  }

  // Extracts the condition field.  UNCONDITIONAL is converted to AL -- in the
  // event that you need to distinguish, (1) make sure that's really true and
  // then (2) explicitly extract bits(31,28).
  Condition GetCondition() const {
    Instruction::Condition cc = Instruction::Condition(Bits(31, 28));
    if (cc == Instruction::UNCONDITIONAL) {
      return Instruction::AL;
    }
    return cc;
  }

  // **********************************
  // Arm 16-bit instruction words API *
  // **********************************
  //
  // TODO(jfb) This Thumb API isn't currently used, and should be fixed up
  //           before it does get used.

  // Creates a 1 word THUMB instruction.
  explicit Instruction(uint16_t word)
      : bits_(static_cast<uint32_t>(word)) {}

  // Creates a 2 word THUMB instruction.
  explicit Instruction(uint16_t word1, uint16_t word2)
      : bits_(((static_cast<uint32_t>(word2) << kThumbWordSize)
               | static_cast<uint32_t>(word1)))
  {}

  // Extracts a range of contiguous bits from the first word of a
  // THUMB instruction , right-justifies it, and returns it.  Note
  // that the range follows hardware convention, with the high bit
  // first.
  uint16_t Word1Bits(int hi, int lo) const {
    return static_cast<uint16_t>(Bits(hi, lo));
  }

  // Extracts a range of contiguous bits from the second word of a
  // THUMB instruction , right-justifies it, and returns it.  Note
  // that the range follows hardware convention, with the high bit
  // first.
  uint16_t Word2Bits(int hi, int lo) const {
    return
        static_cast<uint16_t>(Bits(hi + kThumbWordSize, lo + kThumbWordSize));
  }

  // Changes the range of contiguous bits of the first word of a THUMB
  // instruction, with the given value.  Note: Assumes the value fits,
  // if not, it is truncated.
  void Word1SetBits(int hi, int lo, uint16_t value)  {
    SetBits(hi, lo, (uint32_t) value);
  }

  // Changes the range of contiguous bits of the second word of a THUMB
  // instruction, with the given value.  Note: Assumes the value fits,
  // if not, it is truncated.
  void Word2SetBits(int hi, int lo, uint16_t value) {
    SetBits(hi + kThumbWordSize, lo + kThumbWordSize,
             static_cast<uint32_t>(value));
  }

  // A convenience method that extracts the register specified by
  // the corresponding bits of the first word of a THUMB instruction.
  const Register Word1Reg(int hi, int lo) const {
    return Register(Word1Bits(hi, lo));
  }

  // A convenience method that extracts the register specified by
  // the corresponding bits of the second word of a THUMB instruction.
  const Register Word2Reg(int hi, int lo) const {
    return Register(Word2Bits(hi, lo));
  }

  // Extracts a single bit (0 - 15) from the first word of a
  // THUMB instruction.
  bool Word1Bit(int index) const {
    return Bit(index);
  }

  // Extracts a single bit (0 - 15) from the second word of
  // a THUMB instruction.
  bool Word2Bit(int index) const {
    return Bit(index + kThumbWordSize);
  }

  // Sets the specified bit of the first word in a THUMB instruction
  // to the corresponding value.
  void Word1SetBit(int index, bool value) {
    SetBit(index, value);
  }

  // Sets the specified bit of the second word in a THUMB instruction
  // to the corresponding value.
  void Word2SetBit(int index, bool value) {
    SetBit(index + kThumbWordSize, value);
  }

  // Copies insn into this.
  Instruction& Copy(const Instruction& insn) {
    bits_ = insn.bits_;
    return *this;
  }

 private:
  uint32_t bits_;
  Instruction& operator=(const Instruction& insn);  // Disallow assignment.
};

}  // namespace nacl_arm_dec

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_H
