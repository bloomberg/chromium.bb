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
 * integers.  The goal: to provide a nice, fluent interface for describing
 * instruction bit operations, with zero runtime cost.  Compare:
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

/*
 * A value class that names a single register.  We could use a typedef for this,
 * but it introduces some ambiguity problems because of the implicit conversion
 * to/from int.
 *
 * May be implicitly converted to a single-entry RegisterList (see below).
 */
class Register {
 public:
  explicit inline Register(uint32_t);

  /*
   * Produces the bitmask used to represent this register, in both RegisterList
   * and ARM's LDM instruction.
   */
  inline uint32_t bitmask() const;

  /*
   * Returns the register number for the register.
   */
  inline uint32_t number() const;

  inline bool operator==(const Register &) const;
  inline bool operator!=(const Register &) const;

  // Build a register list from two registers.
  friend RegisterList operator+(const Register &r1, const Register& r2);

 private:
  uint32_t number_;
};

/*
 * A special value used in some cases to indicate that a register field
 * is not used.  This is specially chosen to ensure that bitmask() == 0, so
 * it can be added to any RegisterList with no effect.
 *
 * Note that -1 or 32 can't be used here because C++ doesn't define a portable
 * meaning for such shift distances.
 */
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

/*
 * A collection of Registers.  Used to describe the side effects of operations.
 *
 * Note that this is technically a set, not a list -- but RegisterSet is a
 * well-known term that means something else.
 */
class RegisterList {
 public:
  /*
   * Produces a RegisterList that contains the registers specified in the
   * given bitmask.  To indicate rN, the bitmask must include (1 << N).
   */
  explicit inline RegisterList(uint32_t bitmask);

  /*
   * Produces a RegisterList containing a single register.
   *
   * Note that this is an implicit constructor.  This is okay in this case
   * because
   *  - It converts between two types that we control,
   *  - It converts at most one step (no implicit conversions to Register),
   *  - It inlines to a single machine instruction,
   *  - The readability gain in inst_classes.cc is large.
   */
  inline RegisterList(const Register r);

  /*
   * Checks whether this list contains the given register.
   */
  inline bool operator[](const Register) const;

  // Checks whether this list contains all the registers in the operand.
  inline bool contains_all(RegisterList) const;

  // Checks whether this list contains any of the registers in the operand.
  inline bool contains_any(RegisterList) const;

  inline bool operator==(RegisterList) const;

  inline const RegisterList operator&(const RegisterList) const;

  // Allow the addition operator access to our bitwise representation.
  friend const RegisterList operator+(const RegisterList, const RegisterList);

  // Returns the bits defined in the register list.
  inline uint32_t number() const {
    return bits_;
  }

 private:
  uint32_t bits_;
};

/*
 * Returns a new RegisterList that contains the union of two lists.
 *
 * This is not a member function to allow implicit conversion of Register.
 */
inline const RegisterList operator+(const RegisterList, const RegisterList);

/*
 * A list containing every possible register, even some we don't define.
 * Used exclusively as a bogus scary return value for forbidden instructions.
 */
static const RegisterList kRegisterListEverything = RegisterList(-1);

/* The number of bits in an ARM instruction. */
static const int kArm32InstSize = 32;

/* The number of bits in a word of a THUMB instruction. */
static const int kThumbWordSize = 16;


/*
 * Models an instruction, either a 32-bit ARM instruction of unspecified type,
 * or one word (16-bit) and two word (16-bit) THUMB instructions.
 *
 * This class is designed for efficiency:
 *  - Its public methods for bitfield extraction are short and inline.
 *  - It has no vtable, so on 32-bit platforms it's exactly the size of the
 *    instruction it models.
 *  - API's exist for accessing both ARM (32-bit) instructions and
 *    THUMB instructions (which are 1 or two (16-bit) words).
 */
class Instruction {
 public:
  /******************************
   * Arm 32-bit instruction API *
   *****************************/

  /* Creates an a 32-bit ARM instruction. */
  explicit inline Instruction(uint32_t bits);

  /*
   * Extracts a range of contiguous bits, right-justifies it, and returns it.
   * Note that the range follows hardware convention, with the high bit first.
   */
  inline uint32_t bits(int hi, int lo) const;

  /*
   * Changes the range of contiguous bits, with the given value.
   * Note: Assumes the value fits, if not, it is truncated.
   */
  inline void set_bits(int hi, int lo, uint32_t value);

  /*
   * A convenience method that's exactly equivalent to
   *   Register(instruction.bits(hi, lo))
   *
   * This sequence is quite common in inst_classes.h, inside
   * instruction-specific register methods, such as Rn, Rd, Rm, and Rt.
   */
  inline const Register reg(int hi, int lo) const;

  /*
   * Extracts a single bit (0 - 31).
   */
  inline bool bit(int index) const;

  /*
   * Sets the specified bit to the specified value for an ARM instruction.
   */
  inline void set_bit(int index, bool value);

  /*
   * Returns an integer equivalent of this instruction, masked by the given
   * mask.  Used during decoding to test bitfields.
   */
  inline uint32_t operator&(uint32_t) const;

  /*
   * Possible values for the condition field, from the spec.  The names are
   * pretty cryptic, but in practice we care only whether condition() == AL or
   * not.
   */
  enum Condition {
    // Values 0-14 -- order is significant!
    EQ = 0, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL,
    UNCONDITIONAL = 0xF  // Equivalent to AL -- converted in our API
  };

  /*
   * Extracts the condition field.  UNCONDITIONAL is converted to AL -- in the
   * event that you need to distinguish, (1) make sure that's really true and
   * then (2) explicitly extract bits(31,28).
   */
  inline Condition condition() const;

  /************************************
   * Arm 16-bit instruction words API *
   ************************************/

  /* Creates a 1 word THUMB instruction. */
  explicit inline Instruction(uint16_t word);

  /* Creates a 2 word THUMB instruction. */
  explicit inline Instruction(uint16_t word1, uint16_t word2);

  /*
   * Extracts a range of contiguous bits from the first word of a
   * THUMB instruction , right-justifies it, and returns it.  Note
   * that the range follows hardware convention, with the high bit
   * first.
   */
  inline uint16_t word1_bits(int hi, int lo) const;

  /*
   * Extracts a range of contiguous bits from the second word of a
   * THUMB instruction , right-justifies it, and returns it.  Note
   * that the range follows hardware convention, with the high bit
   * first.
   */
  inline uint16_t word2_bits(int hi, int lo) const;

  /*
   * Changes the range of contiguous bits of the first word of a THUMB
   * instruction, with the given value.  Note: Assumes the value fits,
   * if not, it is truncated.
   */
  inline void word1_set_bits(int hi, int lo, uint16_t value);

  /*
   * Changes the range of contiguous bits of the second word of a THUMB
   * instruction, with the given value.  Note: Assumes the value fits,
   * if not, it is truncated.
   */
  inline void word2_set_bits(int hi, int lo, uint16_t value);

  /*
   * A convenience method that extracts the register specified by
   * the corresponding bits of the first word of a THUMB instruction.
   */
  inline const Register word1_reg(int hi, int lo) const;

  /*
   * A convenience method that extracts the register specified by
   * the corresponding bits of the second word of a THUMB instruction.
   */
  inline const Register word2_reg(int hi, int lo) const;

  /*
   * Extracts a single bit (0 - 15) from the first word of a
   * THUMB instruction.
   */
  inline bool word1_bit(int index) const;

  /*
   * Extracts a single bit (0 - 15) from the second word of
   * a THUMB instruction.
   */
  inline bool word2_bit(int index) const;

  /* Sets the specified bit of the first word in a THUMB instruction
   * to the corresponding value.
   */
  inline void word1_set_bit(int index, bool value);

  /* Sets the specified bit of the second word in a THUMB instruction
   * to the corresponding value.
   */
  inline void word2_set_bit(int index, bool value);

 private:
  uint32_t bits_;
};

}  // namespace

// Definitions for our inlined functions.
#include "native_client/src/trusted/validator_arm/model-inl.h"

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_MODEL_H
