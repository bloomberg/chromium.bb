/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_H_

#include <stdint.h>
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
enum SafetyLevel {
  // The initial value of uninitialized SafetyLevels -- treat as unsafe.
  UNKNOWN = 0,

  // This instruction is left undefined by the ARMv7 ISA spec.
  UNDEFINED,
  // This instruction has unpredictable effects at runtime.
  UNPREDICTABLE,
  // This instruction is deprecated in ARMv7.
  DEPRECATED,

  // This instruction is forbidden by our SFI model.
  FORBIDDEN,
  // This instruction's operands are forbidden by our SFI model.
  FORBIDDEN_OPERANDS,

  // This instruction may be safe in untrusted code: in isolation it contains
  // nothing scary, but the validator may overrule this during global analysis.
  MAY_BE_SAFE
};

// ------------------------------------------------------------------
// The following list of Interface classes are "mixed" into the class
// decoders as static constant fields. The point of these interfaces
// is to control access to data fields within the scope of the
// instruction class decoder, using higher level symbolic names.
//
// For example, register Rn may be located in different bit sequences
// in different instructions. However, they all refer to the concept
// of register Rn (some use bits 0..3 while others use bits
// 16..19). The interfaces for each possible Rn is integrated as a
// static const field named n. Hence virtuals can now use n.reg() to get
// the corresponding register within its virtual functions.
//
// Note: These fields are static const, and the corresponding methods are
// static so that the reference to the static field is only used to
// control scope look ups, and will be optimized out of compiled code.
// ------------------------------------------------------------------

// Interface class to pull out shift type from bits 5 through 6
class ShiftTypeBits5To6Interface {
 public:
  static inline uint32_t value(const Instruction& i) {
    return i.Bits(6, 5);
  }
  // Converts the given immediate value using the shift type specified
  // by this interface. Defined in A8.4.3, page A8-11.
  static inline uint32_t DecodeImmShift(Instruction insn, uint32_t imm5_value) {
    return ComputeDecodeImmShift(value(insn), imm5_value);
  }
 private:
  ShiftTypeBits5To6Interface() {}
  static uint32_t ComputeDecodeImmShift(uint32_t shift_type,
                                        uint32_t imm5_value);
  NACL_DISALLOW_COPY_AND_ASSIGN(ShiftTypeBits5To6Interface);
};

// Interface class to pull out the condition in bits 28 through 31
class ConditionBits28To31Interface {
 public:
  static inline uint32_t value(const Instruction& i) {
    return i.Bits(31, 28);
  }
  static inline bool defined(const Instruction& i) {
    return value(i) != 0xF;
  }
  static inline bool undefined(const Instruction& i)  {
    return !defined(i);
  }

 private:
  ConditionBits28To31Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(ConditionBits28To31Interface);
};

// Interface class to pull out Register D from bits 12 through 15.
class RegDBits12To15Interface {
 public:
  static inline uint32_t number(const Instruction& i) {
    return i.Bits(15, 12);
  }
  static inline Register reg(const Instruction& i) {
    return Register(number(i));
  }

 private:
  RegDBits12To15Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(RegDBits12To15Interface);
};

// Interface class to pull out Register M from bits 0 through 3.
class RegMBits0To3Interface {
 public:
  static inline uint32_t number(const Instruction& i) {
    return i.Bits(3, 0);
  }
  static inline Register reg(const Instruction& i) {
    return Register(number(i));
  }

 private:
  RegMBits0To3Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(RegMBits0To3Interface);
};

// Interface class to pull out Register M from bits 8 through 11.
class RegMBits8To11Interface {
 public:
  static inline uint32_t number(const Instruction& i) {
    return i.Bits(11, 8);
  }
  static inline Register reg(const Instruction& i) {
    return Register(number(i));
  }

 private:
  RegMBits8To11Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(RegMBits8To11Interface);
};

// Interface class to pull out Register N from bits 0 through 3.
class RegNBits0To3Interface {
 public:
  static inline uint32_t number(const Instruction& i) {
    return i.Bits(3, 0);
  }
  static inline Register reg(const Instruction& i) {
    return Register(number(i));
  }

 private:
  RegNBits0To3Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(RegNBits0To3Interface);
};

// Interface class to pull out Register n from bits 16 through 19.
class RegNBits16To19Interface {
 public:
  static inline uint32_t number(const Instruction& i) {
    return i.Bits(19, 16);
  }
  static inline Register reg(const Instruction& i) {
    return Register(number(i));
  }

 private:
  RegNBits16To19Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(RegNBits16To19Interface);
};

// Interface class to pull out Register S from bits 8 through 11.
class RegSBits8To11Interface {
 public:
  static inline uint32_t number(const Instruction& i) {
    return i.Bits(11, 8);
  }
  static inline Register reg(const Instruction& i) {
    return Register(number(i));
  }

 private:
  RegSBits8To11Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(RegSBits8To11Interface);
};

// Interface class to pull out an immediate value in bits 0 through 11.
class Imm12Bits0To11Interface {
 public:
  static inline uint32_t value(const Instruction& i) {
    return i.Bits(11, 0);
  }
  static uint32_t get_modified_immediate(Instruction i);

 private:
  Imm12Bits0To11Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm12Bits0To11Interface);
};

// Interface class to pull out an immediate value in bits 7 through 11.
class Imm5Bits7To11Interface {
 public:
  static inline uint32_t value(const Instruction& i) {
    return i.Bits(11, 7);
  }

 private:
  Imm5Bits7To11Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm5Bits7To11Interface);
};

// Interface class to pull out an immediate value in bits 16 through 20.
class Imm5Bits16To20Interface {
 public:
  static inline uint32_t value(const Instruction& i) {
    return i.Bits(20, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm5Bits16To20Interface);
};

// Interface class to pull out an immediate value in bits 16 through 19.
class Imm4Bits16To19Interface {
 public:
  static inline uint32_t value(const Instruction& i) {
    return i.Bits(19, 16);
  }

 private:
  Imm4Bits16To19Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm4Bits16To19Interface);
};

// Interface class to pull out S (update) bit from bit 20, which
// defines if the condition bits in APSR are updated by the instruction.
class UpdatesConditionsBit20Interface {
 public:
  // Returns true if bit is set that states that the condition bits
  // APSR is updated.
  static inline bool is_updated(const Instruction i) {
    return i.Bit(20);
  }
  // Returns the conditions register if it is used.
  static inline Register conds_if_updated(const Instruction i) {
    return is_updated(i) ? kConditions : kRegisterNone;
  }

 private:
  UpdatesConditionsBit20Interface() {}
  NACL_DISALLOW_COPY_AND_ASSIGN(UpdatesConditionsBit20Interface);
};

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
  // The most positive result this can return is called MAY_BE_SAFE because it
  // is necessary, but not sufficient: the validator has the final say.
  virtual SafetyLevel safety(Instruction i) const = 0;

  // Gets the set of registers affected when an instruction executes.  This set
  // is complete, and includes
  //  - explicit destination register(s),
  //  - changes to condition flags,
  //  - indexed-addressing writeback,
  //  - changes to r15 by branches,
  //  - implicit register results, like branch-with-link.
  //
  // Note: If you are unsure if an instruction changes condition flags, be
  // conservative and add it to the set of registers returned by this
  // function. Failure to do so may cause a potential break in pattern
  // atomicity, which checks that two instructions run under the same condition.
  //
  // The default implementation returns a ridiculous bitmask that suggests that
  // all possible side effects will occur -- override if this is not
  // appropriate. :-)
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterListEverything;
  }

  // Gets the set of registers that this instruction defines through immediate
  // indexed addressing writeback -- a subset of the defs() set.
  //
  // This distinction is useful for operations like SP-relative loads, because
  // the maximum displacement that immediate addressing can produce is small.
  //
  // Note that this does not include defs produced by *register* indexed
  // addressing writeback, since they have no useful properties in our model.
  //
  // Stubbed to indicate that no such addressing occurs.
  virtual RegisterList immediate_addressing_defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return RegisterList();
  }

  // For instructions that can read or write memory, gets the register used as
  // the base for generating the effective address.
  //
  // It is stubbed to return nonsense.
  virtual Register base_address_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  // Checks whether the instruction computes its read or write address as
  // base address + immediate.
  //
  // It is stubbed to return false.
  virtual bool offset_is_immediate(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // For indirect branch instructions, returns the register being moved into
  // r15.  Otherwise, reports kRegisterNone.
  //
  // Note that this exclusively describes instructions that write r15 from a
  // register, unmodified.  This means BX, BLX, and MOV without shift.  Not
  // even BIC, which we allow to write to r15, is modeled this way.
  //
  virtual Register branch_target_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  // Checks whether the instruction is a direct relative branch -- meaning it
  // adds a constant offset to r15.
  virtual bool is_relative_branch(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // For relative branches, gets the offset added to the instruction's
  // virtual address to find the target.  The results are bogus unless
  // is_relative_branch() returns true.
  //
  // Note that this is different than the offset added to r15 at runtime, since
  // r15 reads as 8 bytes ahead.  This function does the math so you don't have
  // to.
  virtual int32_t branch_target_offset(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return 0;
  }

  // Checks whether this instruction is the special bit sequence that marks
  // the start of a literal pool.
  virtual bool is_literal_pool_head(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // Checks that an instruction clears a certain pattern of bits in all its
  // (non-flag) result registers.  The mask should include 1s in the positions
  // that should be cleared.
  virtual bool clears_bits(Instruction i, uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

  // Checks that an instruction will set Z if certain bits in r (chosen by 1s in
  // the mask) are clear.
  //
  // Note that the inverse does not hold: the actual instruction i may require
  // *more* bits to be clear to set Z.  This is fine.
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(r);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

 protected:
  inline ClassDecoder() {}
  virtual ~ClassDecoder() {}

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
  inline explicit UnsafeClassDecoder(SafetyLevel safety)
      : safety_(safety) {}
  virtual ~UnsafeClassDecoder() {}

  // Return the safety associated with this class.
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return safety_;
  }

  // Switch off the def warnings -- it's already forbidden!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return RegisterList();
  }

 private:
  SafetyLevel safety_;
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeClassDecoder);
};

class Forbidden : public UnsafeClassDecoder {
 public:
  inline Forbidden() : UnsafeClassDecoder(FORBIDDEN) {}
  virtual ~Forbidden() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Forbidden);
};

// Represents the undefined space in the instruction encoding.
class Undefined : public UnsafeClassDecoder {
 public:
  inline Undefined() : UnsafeClassDecoder(UNDEFINED) {}
  virtual ~Undefined() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Undefined);
};

// Represents instructions that have been deprecated in ARMv7.
class Deprecated : public UnsafeClassDecoder {
 public:
  inline Deprecated() : UnsafeClassDecoder(DEPRECATED) {}
  virtual ~Deprecated() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Deprecated);
};

// Represents an unpredictable encoding.  Note that many instructions may
// *become* unpredictable based on their operands -- this is used only for
// the case where a large space of the instruction set is unpredictable.
class Unpredictable : public UnsafeClassDecoder {
 public:
  inline Unpredictable() : UnsafeClassDecoder(UNPREDICTABLE) {}
  virtual ~Unpredictable() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unpredictable);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_H_
