/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 * Copyright 2009, Google Inc.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H

#include <stdint.h>
#include "native_client/src/trusted/validator_arm/v2/model.h"
#include "native_client/src/include/portability.h"

/*
 * Models the "instruction classes" that the decoder produces.
 */
namespace nacl_arm_dec {

/*
 * Used to describe whether an instruction is safe, and if not, what the issue
 * is.  Only instructions that MAY_BE_SAFE should be allowed in untrusted code,
 * and even those may be rejected by the validator.
 */
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

  /*
   * This instruction may be safe in untrusted code: in isolation it contains
   * nothing scary, but the validator may overrule this during global analysis.
   */
  MAY_BE_SAFE
};

/*
 * Decodes a class of instructions.  Does spooky undefined things if handed
 * instructions that don't belong to its class.  Who defines which instructions
 * these are?  Why, the generated decoder, of course.
 *
 * This is an abstract base class intended to be overridden with the details of
 * particular instruction-classes.
 *
 * ClassDecoders should be stateless, and should provide a no-arg constructor
 * for use by the generated decoder.
 */
class ClassDecoder {
 public:
  /*
   * Checks how safe this instruction is, in isolation.
   * This will detect any violation in the ARMv7 spec -- undefined encodings,
   * use of registers that are unpredictable -- and the most basic constraints
   * in our SFI model.  Because ClassDecoders are referentially-transparent and
   * cannot touch global state, this will not check things that may vary with
   * ABI version.
   *
   * The most positive result this can return is called MAY_BE_SAFE because it
   * is necessary, but not sufficient: the validator has the final say.
   */
  virtual SafetyLevel safety(Instruction i) const = 0;

  /*
   * Gets the set of registers affected when an instruction executes.  This set
   * is complete, and includes
   *  - explicit destination register(s),
   *  - changes to flags,
   *  - indexed-addressing writeback,
   *  - changes to r15 by branches,
   *  - implicit register results, like branch-with-link.
   *
   * The default implementation returns a ridiculous bitmask that suggests that
   * all possible side effects will occur -- override if this is not
   * appropriate. :-)
   */
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterListEverything;
  }

  /*
   * Gets the set of registers that this instruction defines through immediate
   * indexed addressing writeback -- a subset of the defs() set.
   *
   * This distinction is useful for operations like SP-relative loads, because
   * the maximum displacement that immediate addressing can produce is small.
   *
   * Note that this does not include defs produced by *register* indexed
   * addressing writeback, since they have no useful properties in our model.
   *
   * Stubbed to indicate that no such addressing occurs.
   */
  virtual RegisterList immediate_addressing_defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  /*
   * Checks whether the instruction can write to memory.  Note that we only
   * permit base+immediate addressing stores, so if the safety() looks good
   * and writes_memory() is true, you can assume base+immediate addressing is
   * being used.
   *
   * Stubbed to return 'false', which is the common case.
   */
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * For instructions that can write memory, gets the register used as the base
   * for generating the effective address.
   *
   * The result is useful only for safe instructions where writes_memory() is
   * true.  It is stubbed to return nonsense.
   */
  virtual Register base_address_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  /*
   * For indirect branch instructions, returns the register being moved into
   * r15.  Otherwise, reports kRegisterNone.
   *
   * Note that this exclusively describes instructions that write r15 from a
   * register, unmodified.  This means BX, BLX, and MOV without shift.  Not
   * even BIC, which we allow to write to r15, is modeled this way.
   */
  virtual Register branch_target_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  /*
   * Checks whether the instruction is a direct relative branch -- meaning it
   * adds a constant offset to r15.
   */
  virtual bool is_relative_branch(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * For relative branches, gets the offset added to the instruction's
   * virtual address to find the target.  The results are bogus unless
   * is_relative_branch() returns true.
   *
   * Note that this is different than the offset added to r15 at runtime, since
   * r15 reads as 8 bytes ahead.  This function does the math so you don't have
   * to.
   */
  virtual int32_t branch_target_offset(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return 0;
  }

  /*
   * Checks whether this instruction is the special bit sequence that marks
   * the start of a literal pool.
   */
  virtual bool is_literal_pool_head(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * Checks that an instruction clears a certain pattern of bits in all its
   * (non-flag) result registers.  The mask should include 1s in the positions
   * that should be cleared.
   */
  virtual bool clears_bits(Instruction i, uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

 protected:
  ClassDecoder() {}
  virtual ~ClassDecoder() {}
};

/*
 * Represents an instruction that is forbidden under all circumstances, so we
 * didn't bother decoding it further.
 */
class Forbidden : public ClassDecoder {
 public:
  virtual ~Forbidden() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return FORBIDDEN;
  }
  // Switch off the def warnings -- it's already forbidden!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Represents the undefined space in the instruction encoding.
 */
class Undefined : public ClassDecoder {
 public:
  virtual ~Undefined() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return UNDEFINED;
  }
  // Switch off the def warnings -- it's already undefined!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Represents instructions that have been deprecated in ARMv7.
 */
class Deprecated : public ClassDecoder {
 public:
  virtual ~Deprecated() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return DEPRECATED;
  }
  // Switch off the def warnings -- it's already deprecated!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Represents an unpredictable encoding.  Note that many instructions may
 * *become* unpredictable based on their operands -- this is used only for
 * the case where a large space of the instruction set is unpredictable.
 */
class Unpredictable : public ClassDecoder {
 public:
  virtual ~Unpredictable() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return UNPREDICTABLE;
  }
  // Switch off the def warnings -- it's already unpredictable!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * An instruction that, for modeling purposes, is a no-op.  It has no
 * side effects that are significant to SFI, and has no operand combinations
 * that cause it to become unsafe.
 *
 * Uses of this class must be carefully evaluated, because it bypasses all
 * further validation.
 */
class EffectiveNoOp : public ClassDecoder {
 public:
  virtual ~EffectiveNoOp() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * BKPT
 * We model this mostly so we can use it to recognize literal pools -- untrusted
 * code isn't expected to use it, but it's not unsafe, and there are cases where
 * we may generate it.
 */
class Breakpoint : public ClassDecoder {
 public:
  virtual ~Breakpoint() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
  virtual bool is_literal_pool_head(Instruction i) const;
};

/*
 * Models the most common class of data processing instructions.  We use this
 * for any operation that
 *  - writes a single register, specified in 15:12;
 *  - does not write memory,
 *  - should not be permitted to cause a jump by writing r15,
 *  - writes flags when bit 20 is set.
 */
class DataProc : public ClassDecoder {
 public:
  virtual ~DataProc() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
};

/*
 * Models the few data-processing instructions that *don't* produce a result,
 * but may still set flags.  e.g. TST.
 */
class Test : public DataProc {
 public:
  virtual ~Test() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * A special-case data processing instruction: the immediate BIC.  We consider
 * this the only instruction that reliably clears bits in its result, so we
 * model it separately from other logic ops.
 */
class ImmediateBic : public DataProc {
 public:
  virtual ~ImmediateBic() {}

  // ImmediateBic is exempted from the writes-r15 check.
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual bool clears_bits(Instruction i, uint32_t mask) const;
};

/*
 * Models the Pack/Saturate/Reverse instructions, which
 *  - Write a register identified by 15:12,
 *  - Are not permitted to affect r15,
 *  - Always set flags.
 *
 *  Note that not all of the Pack/Sat/Rev instructions really set flags, but
 *  we deliberately err on the side of caution to simplify modeling (because we
 *  don't care, in practice, about their flag effects).
 */
class PackSatRev : public ClassDecoder {
 public:
  virtual ~PackSatRev() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
};


/*
 * Models multiply instructions, which
 *  - Write a register identified by 19:16,
 *  - Are not permitted to affect r15,
 *  - Always set flags.
 *
 * Note that multiply instructions don't *really* always set the flags.  We
 * deliberately take this shortcut to simplify modeling of the many, many
 * multiply instructions -- some of which always set flags, some of which never
 * do, and some of which set conditionally.
 */
class Multiply : public ClassDecoder {
 public:
  virtual ~Multiply() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
};

/*
 * Models double-word multiply instructions, which
 *  - Write TWO registers, identified by 19:16 and 15:12,
 *  - Are not permitted to affect r15,
 *  - Always set flags.
 *
 * Note that multiply instructions don't *really* always set the flags.  We
 * deliberately take this shortcut to simplify modeling of the many, many
 * multiply instructions -- some of which always set flags, some of which never
 * do, and some of which set conditionally.
 */
class LongMultiply : public Multiply {
 public:
  virtual ~LongMultiply() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * Models a weird variation on double-word multiply instructions, where
 * the destination registers are identified in 15:12 and 11:8.
 */
class LongMultiply15_8 : public Multiply {
 public:
  virtual ~LongMultiply15_8() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * Saturating adds and subtracts.  Conceptually equivalent to DataProc,
 * except for the use of the S bit (bit 20) -- they always set flags.
 */
class SatAddSub : public DataProc {
 public:
  virtual ~SatAddSub() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * Move to Status Register.  Used from application code to alter or restore
 * condition flags in APSR.  The side effects are similar to the Test class,
 * but we model it separately so we can catch code trying to poke the reserved
 * APSR bits.
 */
class MoveToStatusRegister : public ClassDecoder {
 public:
  virtual ~MoveToStatusRegister() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;
};

/*
 * A base+immediate store, of unspecified width.  (We don't care whether it
 * stores one byte or 64.)
 */
class StoreImmediate : public ClassDecoder {
 public:
  virtual ~StoreImmediate() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
};

/*
 * STREX - a lot like a store, but with restricted addressing modes and more
 * register writes.  Unfortunately the encodings aren't compatible, so they
 * don't share code.
 */
class StoreExclusive : public ClassDecoder {
 public:
  virtual ~StoreExclusive() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
};

/*
 * The most common class of loads, where
 *  - The destination register is 15:12
 *  - The address in 19:16 is written when bit 21 is set
 *  - No flags changes or anything weird.
 *
 * Notice we do not care about the width of the loaded value, because it doesn't
 * affect addressing.
 */
class Load : public ClassDecoder {
 public:
  virtual ~Load() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
};

/*
 * A less common class of loads, which have all the side effects of Load, but
 * also write register 15:12 + 1.
 */
class LoadDouble : public Load {
 public:
  virtual ~LoadDouble() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * And, finally, the oddest class of loads: LDM.  In addition to the base
 * register, this may write every other register, subject to a bitmask.
 */
class LoadMultiple : public ClassDecoder {
 public:
  virtual ~LoadMultiple() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
};

/*
 * A load to a vector register.  Like LoadCoprocessor below, the only visible
 * side effect is from writeback.
 */
class VectorLoad : public ClassDecoder {
 public:
  virtual ~VectorLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
};

/*
 * A store from a vector register.
 */
class VectorStore : public ClassDecoder {
 public:
  virtual ~VectorStore() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
};

/*
 * A generic coprocessor instruction that (by default) has no side effects.
 * These instructions are:
 * - Permitted only for whitelisted coprocessors that we've analyzed;
 * - Not permitted to update r15.
 *
 * Coprocessor ops with visible side-effects should extend and override this.
 */
class CoprocessorOp : public ClassDecoder {
 public:
  virtual ~CoprocessorOp() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * LDC/LDC2, which load data from memory directly into a coprocessor.
 * The only visible side effect of this is optional indexing writeback,
 * controlled by bit 21.
 */
class LoadCoprocessor : public CoprocessorOp {
 public:
  virtual ~LoadCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
};

/*
 * STC/STC2, which store data from a coprocessor into RAM.  Fortunately the
 * base address is not a coprocessor register!
 *
 * The side effects of this are identical to LoadCoprocessor, except of course
 * that it writes memory.  Unfortunately, the size of the memory write is not
 * defined in the ISA, but by the coprocessor.  Thus, we wind up having to
 * whitelist certain cases of this on known coprocessor types (see the impl).
 */
class StoreCoprocessor : public CoprocessorOp {
 public:
  virtual ~StoreCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
};

/*
 * MRC/MRC2, which load a single register from a coprocessor register.
 */
class MoveFromCoprocessor : public CoprocessorOp {
 public:
  virtual ~MoveFromCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * MRRC/MRRC2, which load pairs of registers from a coprocessor register.
 */
class MoveDoubleFromCoprocessor : public CoprocessorOp {
 public:
  virtual ~MoveDoubleFromCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * BX and BLX - everyone's favorite register-indirect branch.
 * This implementation makes assumptions about where the L bit is located, and
 * should not be used for other indirect branches (such as mov pc, rN).
 * Hence the cryptic name.
 */
class BxBlx : public ClassDecoder {
 public:
  virtual ~BxBlx() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;
};

/*
 * B and BL
 *
 * This implementation makes assumptions about where the L bit is located, but
 * the assumption holds for all non-illegal direct branches.
 */
class Branch : public ClassDecoder {
 public:
  virtual ~Branch() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_relative_branch(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual int32_t branch_target_offset(Instruction i) const;
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H
