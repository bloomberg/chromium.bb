/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_arm/model.h"

/*
 * Models the "instruction classes" that the decoder produces.
 */
namespace nacl_arm_dec {

// Used to describe whether an instruction is safe, and if not, what the issue
// is.  Only instructions that MAY_BE_SAFE should be allowed in untrusted code,
// and even those may be rejected by the validator.
//
// Note: The enumerated values are used in dgen_core.py (see class
// SafetyAction).  Be sure to update values in that class if this list
// changes, so that the two stay in sync. Also be sure to update Int2SafetyLevel
// in inst_classes.cc.
enum SafetyLevel {
  // The initial value of uninitialized SafetyLevels -- treat as unsafe.
  UNINITIALIZED = 0,

  // Values put into one (or more) registers is not known, as specified
  // by the ARMv7 ISA spec.
  // See instructions VSWP, VTRN, VUZP, and VZIP for examples of this.
  UNKNOWN,
  // This instruction is left undefined by the ARMv7 ISA spec.
  UNDEFINED,
  // This instruction is not recognized by the decoder functions.
  NOT_IMPLEMENTED,
  // This instruction has unpredictable effects at runtime.
  UNPREDICTABLE,
  // This instruction is deprecated in ARMv7.
  DEPRECATED,

  // This instruction is forbidden by our SFI model.
  FORBIDDEN,
  // This instruction's operands are forbidden by our SFI model.
  FORBIDDEN_OPERANDS,

  // This instruction was decoded incorrectly, because it should have decoded
  // as a different instruction. This value should never occur, unless there
  // is a bug in our decoder tables (in file armv7.table).
  DECODER_ERROR,

  // This instruction may be safe in untrusted code: in isolation it contains
  // nothing scary, but the validator may overrule this during global analysis.
  MAY_BE_SAFE
};

// Returns the safety level associated with i (or UNINITIALIZED if no such
// safety level exists).
SafetyLevel Int2SafetyLevel(uint32_t i);

// A class decoder is designed to decode a set of instructions that
// have the same semantics, in terms of what the validator needs. This
// includes the bit ranges in the instruction that correspond to
// assigned registers.  as well as whether the instruction is safe to
// use within the validator.
//
// The important property of these class decoders is that the
// corresponding DecoderState (defined in decoder.h) will inspect the
// instruction bits and then dispatch the appropriate class decoder.
//
// The virtuals defined in this class are intended to be used solely
// for the purpose of the validator. For example, for virtual "defs",
// the class decoder will look at the bits defining the assigned
// register of the instruction (typically in bits 12 through 15) and
// add that register to the set of registers returned by the "defs"
// virtual.
//
// There is an underlying assumption that class decoders are constant
// and only provide implementation details for the instructions they
// should be applied to. In general, class decoders should not be
// copied or assigned. Hence, only a no-argument constructor should be
// provided.
class ClassDecoder {
 public:
  // Checks how safe this instruction is, in isolation.
  // This will detect any violation in the ARMv7 spec -- undefined encodings,
  // use of registers that are unpredictable -- and the most basic constraints
  // in our SFI model.  Because ClassDecoders are referentially-transparent and
  // cannot touch global state, this will not check things that may vary with
  // ABI version.
  //
  // Note: To best take advantage of the testing system, define this function
  // to return DECODER_ERROR immediately, if DECODER_ERROR is to be returned by
  // this virtual. This allows testing to (quietly) detect when it is
  // ok that the expected decoder wasn't the actual decoder selected by the
  // instruction decoder.
  //
  // The most positive result this can return is called MAY_BE_SAFE because it
  // is necessary, but not sufficient: the validator has the final say.
  virtual SafetyLevel safety(Instruction i) const = 0;

  // Gets the set of registers affected when an instruction executes.  This set
  // is complete, and includes
  //  - explicit destination (general purpose) register(s),
  //  - changes to condition APSR flags NZCV.
  //  - indexed-addressing writeback,
  //  - changes to r15 by branches,
  //  - implicit register results, like branch-with-link.
  //
  // Note: This virtual only tracks effects to ARM general purpose flags, and
  // NZCV APSR flags.
  //
  // Note: If you are unsure if an instruction changes condition flags, be
  // conservative and add it to the set of registers returned by this
  // function. Failure to do so may cause a potential break in pattern
  // atomicity, which checks that two instructions run under the same condition.
  //
  // The default implementation returns a ridiculous bitmask that suggests that
  // all possible side effects will occur -- override if this is not
  // appropriate. :-)
  virtual RegisterList defs(Instruction i) const;

  // Gets the set of general purpose registers used by the instruction.
  // This set includes:
  //  - explicit source (general purpose) register(s).
  //  - implicit registers, like branch-with-link.
  //
  // The default implementation returns the empty set.
  virtual RegisterList uses(Instruction i) const;

  // Returns true if the base register has small immediate writeback.
  //
  // This distinction is useful for operations like SP-relative loads, because
  // the maximum displacement that immediate addressing can produce is small and
  // will therefore never cross guard pages if the base register isn't
  // constrained to the untrusted address space.
  //
  // Note that this does not include writeback produced by *register* indexed
  // addressing writeback, since they have no useful properties in our model.
  //
  // Stubbed to indicate that no such writeback occurs.
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;

  // For instructions that can read or write memory, gets the register used as
  // the base for generating the effective address.
  //
  // It is stubbed to return nonsense.
  virtual Register base_address_register(Instruction i) const;

  // Checks whether the instruction is a PC-relative load + immediate.
  //
  // It is stubbed to return false.
  virtual bool is_literal_load(Instruction i) const;

  // For indirect branch instructions, returns the register being moved into
  // r15.  Otherwise, reports Register::None().
  //
  // Note that this exclusively describes instructions that write r15 from a
  // register, unmodified.  This means BX, BLX, and MOV without shift.  Not
  // even BIC, which we allow to write to r15, is modeled this way.
  //
  virtual Register branch_target_register(Instruction i) const;

  // Checks whether the instruction is a direct relative branch -- meaning it
  // adds a constant offset to r15.
  virtual bool is_relative_branch(Instruction i) const;

  // For relative branches, gets the offset added to the instruction's
  // virtual address to find the target.  The results are bogus unless
  // is_relative_branch() returns true.
  //
  // Note that this is different than the offset added to r15 at runtime, since
  // r15 reads as 8 bytes ahead.  This function does the math so you don't have
  // to.
  virtual int32_t branch_target_offset(Instruction i) const;

  // Checks whether this instruction is the special bit sequence that marks
  // the start of a literal pool.
  virtual bool is_literal_pool_head(Instruction i) const;

  // Checks that an instruction clears a certain pattern of bits in all its
  // (non-flag) result registers.  The mask should include 1s in the positions
  // that should be cleared.
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

  // Checks that an instruction will set Z if certain bits in r (chosen by 1s in
  // the mask) are clear.
  //
  // Note that the inverse does not hold: the actual instruction i may require
  // *more* bits to be clear to set Z.  This is fine.
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const;

  // Returns true only if the given thread register (r9) is used in one of
  // the following forms:
  //    ldr Rn, [r9]     ; load use thread pointer.
  //    ldr Rn, [r9, #4] ; load IRT thread pointer.
  // That is, accesses one of the two legal thread pointers.
  //
  // The default virtual returns false.
  virtual bool is_load_thread_address_pointer(Instruction i) const;

  // Returns the sentinel version of the instruction for dynamic code
  // replacement. In dynamic code replacement, only certain immediate
  // constants for specialized instructions may be modified by a dynamic
  // code replacement. For such instructions, this method returns the
  // instruction with the immediate constant normalized to zero. For
  // all other instructions, this method returns a copy of the instruction.
  //
  // This result is used by method ValidateSegmentPair in validator.cc to
  // verify that only such constant changes are allowed.
  //
  // Note: This method should not be defined if any of the following
  // virtuals are overridden by the decoder class, since they make assumptions
  // about the literal constants within them:
  //     offset_is_immediate
  //     is_relative_branch
  //     branch_target_offset
  //     is_literal_pool_head
  //     clears_bits
  //     sets_Z_if_bits_clear
  virtual Instruction dynamic_code_replacement_sentinel(Instruction i) const;

 protected:
  ClassDecoder() {}
  virtual ~ClassDecoder() {}

  // A common idiom in derived classes: def() is often implemented in terms
  // of this function: the base register must be added to the def set when
  // there's small immediate writeback.
  //
  // Note that there are other types of writeback into base than small
  // immediate writeback.
  Register base_small_writeback_register(Instruction i) const {
    return base_address_register_writeback_small_immediate(i)
        ? base_address_register(i) : Register::None();
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ClassDecoder);
};

//----------------------------------------------------------------
// The following class decoders define common cases, defining a
// concept that simply associates a non MAY_BE_SAFE with the
// instructions it processes. As such, they provide default
// implementation that returns the corresponding safety value, and
// assumes nothing else interesting happens.
//----------------------------------------------------------------

class UnsafeClassDecoder : public ClassDecoder {
 public:
  // Return the safety associated with this class.
  virtual SafetyLevel safety(Instruction i) const;

  // Switch off the def warnings -- it's already forbidden!
  virtual RegisterList defs(Instruction i) const;

 protected:
  explicit UnsafeClassDecoder(SafetyLevel safety)
      : safety_(safety) {}

 private:
  SafetyLevel safety_;
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(UnsafeClassDecoder);
};

class Forbidden : public UnsafeClassDecoder {
 public:
  Forbidden() : UnsafeClassDecoder(FORBIDDEN) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Forbidden);
};

// Represents that the instruction is not implemented.
class NotImplemented : public UnsafeClassDecoder {
 public:
  NotImplemented() : UnsafeClassDecoder(NOT_IMPLEMENTED) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NotImplemented);
};

}  // namespace nacl_arm_dec

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_H_
