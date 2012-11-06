/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 * Copyright 2012, Google Inc.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_MIPS_MODEL_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_MIPS_MODEL_H

/*
 * Models instructions and decode results.
 *
 * TODO(petarj): This file and the classes have been based on the same named
 * module within validator_arm folder. The code has been reduced and changed
 * slightly to accommodate some style issues. There still may be room for
 * improvement as we have not investigated performance impact of this classes.
 * Thus, this should be done later and the code should be revisited.
 */

#include <stdint.h>

namespace nacl_mips_dec {

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
   * Produces the bitmask used to represent this register.
   */
  inline uint32_t Bitmask() const;

  inline bool Equals(const Register &) const;

 private:
  uint32_t _number;
};

const Register kRegisterNone(0);

// Registers with special meaning in our model:
const Register kRegisterStack(29);
const Register kRegisterJumpMask(14);       // $t6 = holds mask for jr.
const Register kRegisterLoadStoreMask(15);  // $t7 = holds mask for load/store.
const Register kRegisterTls(24);            // $t8 = holds tls index.

const Register kRegisterZero(0);

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
  inline bool ContainsAll(RegisterList) const;

  // Checks whether this list contains any of the registers in the operand.
  inline bool ContainsAny(RegisterList) const;

  inline const RegisterList operator&(const RegisterList) const;

 private:
  uint32_t _bits;
};

/*
 * A list containing every possible register, even some we don't define.
 * Used exclusively as a bogus scary return value for forbidden instructions.
 */
static const RegisterList kRegisterListEverything = RegisterList(-1);

static uint32_t const kReservedRegsBitmask = kRegisterTls.Bitmask()
                                             + kRegisterJumpMask.Bitmask()
                                             + kRegisterLoadStoreMask.Bitmask();
static RegisterList const kRegListReserved = RegisterList(kReservedRegsBitmask);

/*
 * A 32-bit Mips instruction of unspecified type.
 *
 * This class is designed for efficiency:
 *  - Its public methods for bitfield extraction are short and inline.
 *  - It has no vtable, so on 32-bit platforms it's exactly the size of the
 *    instruction it models.
 */
class Instruction {
 public:
  explicit inline Instruction(uint32_t bits);

  /*
   * Extracts a range of contiguous bits, right-justifies it, and returns it.
   * Note that the range follows hardware convention, with the high bit first.
   */
  inline uint32_t Bits(int hi, int lo) const;

  /*
   * A convenience method that's exactly equivalent to
   *   Register(instruction.bits(hi, lo))
   *
   * This sequence is quite common in inst_classes.cc.
   */
  inline const Register Reg(int hi, int lo) const;

  /*
   * Extracts a single bit (0 - 31).
   */
  inline bool Bit(int index) const;

  /*
   * Returns an integer equivalent of this instruction, masked by the given
   * mask.  Used during decoding to test bitfields.
   */
  inline uint32_t operator&(uint32_t) const;

 private:
  uint32_t _bits;
};

uint32_t const kInstrSize   = 4;
uint32_t const kInstrAlign  = 0xFFFFFFFC;
uint32_t const kBundleAlign = 0xFFFFFFF0;

}  // namespace

// Definitions for our inlined functions.
#include "native_client/src/trusted/validator_mips/model-inl.h"

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_MIPS_MODEL_H
