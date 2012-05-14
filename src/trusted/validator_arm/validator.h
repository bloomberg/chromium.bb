/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_VALIDATOR_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_VALIDATOR_H

/*
 * The SFI validator, and some utility classes it uses.
 */

#include <stdint.h>
#include <stdlib.h>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_arm/address_set.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode.h"
#include "native_client/src/trusted/validator_arm/inst_classes.h"
#include "native_client/src/trusted/validator_arm/model.h"
#include "native_client/src/include/portability.h"

namespace nacl_arm_val {

/*
 * Forward declarations of classes used by-reference in the validator, and
 * defined at the end of this file.
 */
class CodeSegment;
class DecodedInstruction;
class ProblemSink;


/*
 * A simple model of an instruction bundle.  Bundles consist of one or more
 * instructions (two or more, in the useful case); the precise size is
 * controlled by the parameters passed into SfiValidator, below.
 */
class Bundle {
 public:
  Bundle(uint32_t virtual_base, uint32_t size_bytes)
      : virtual_base_(virtual_base), size_(size_bytes) {}

  uint32_t begin_addr() const { return virtual_base_; }
  uint32_t end_addr() const { return virtual_base_ + size_; }

  bool operator!=(const Bundle &other) const {
    // Note that all Bundles are currently assumed to be the same size.
    return virtual_base_ != other.virtual_base_;
  }

 private:
  uint32_t virtual_base_;
  uint32_t size_;
};


/*
 * The SFI validator itself.  The validator is controlled by the following
 * inputs:
 *   bytes_per_bundle: the number of bytes in each bundle of instructions.
 *       Currently this tends to be 16, but we've evaluated alternatives.
 *   code_region_bytes: number of bytes in the code region, starting at address
 *       0 and including the trampolines, etc.  Must be a power of two.
 *   data_region_bits: number of bytes in the data region, starting at address
 *       0 and including the code region.  Must be a power of two.
 *   read_only_registers: registers that untrusted code must not alter (but may
 *       read).  This currently applies to r9, where we store some thread state.
 *   data_address_registers: registers that must contain a valid data-region
 *       address at all times.  This currently applies to the stack pointer, but
 *       could be extended to include a frame pointer for C-like languages.
 *
 * The values of these inputs will typically be taken from the headers of
 * untrusted code -- either by the ABI version they indicate, or (perhaps in
 * the future) explicit indicators of what SFI model they follow.
 */
class SfiValidator {
 public:
  SfiValidator(uint32_t bytes_per_bundle,
               uint32_t code_region_bytes,
               uint32_t data_region_bytes,
               nacl_arm_dec::RegisterList read_only_registers,
               nacl_arm_dec::RegisterList data_address_registers);

  explicit SfiValidator(const SfiValidator& v);

  /*
   * The main validator entry point.  Validates the provided CodeSegments,
   * which must be in sorted order, reporting any problems through the
   * ProblemSink.
   *
   * Returns true iff no problems were found.
   */
  bool validate(const std::vector<CodeSegment> &, ProblemSink *out);

  /*
   * Checks whether the given Register always holds a valid data region address.
   * This implies that the register is safe to use in unguarded stores.
   */
  bool is_data_address_register(nacl_arm_dec::Register) const;

  uint32_t data_address_mask() const { return data_address_mask_; }
  uint32_t code_address_mask() const { return code_address_mask_; }

  nacl_arm_dec::RegisterList read_only_registers() const {
    return read_only_registers_;
  }
  nacl_arm_dec::RegisterList data_address_registers() const {
    return data_address_registers_;
  }

  // Returns the Bundle containing a given address.
  const Bundle bundle_for_address(uint32_t) const;

  /*
   * Change masks: this is useful for debugging and cannot be completely
   *               controlled with constructor arguments
   */
  void change_masks(uint32_t code_address_mask, uint32_t data_address_mask) {
    code_address_mask_ = code_address_mask;
    data_address_mask_ = data_address_mask;
  }

  /*
   * Copy the given validator state.
   */
  SfiValidator& operator=(const SfiValidator& v);

 private:
  bool is_bundle_head(uint32_t address) const;

  /*
   * Validates a straight-line execution of the code, applying patterns.  This
   * is the first validation pass, which fills out the AddressSets for
   * consumption by later analyses.
   *   branches: gets filled in with the address of every direct branch.
   *   critical: gets filled in with every address that isn't safe to jump to,
   *       because it would split an otherwise-safe pseudo-op.
   *
   * Returns true iff no problems were found.
   */
  bool validate_fallthrough(const CodeSegment &, ProblemSink *,
      AddressSet *branches, AddressSet *critical);

  /*
   * Factor of validate_fallthrough, above.  Checks a single instruction using
   * the instruction patterns defined in the .cc file, with two possible
   * results:
   *   1. No patterns matched, or all were safe: nothing happens.
   *   2. Patterns matched and were unsafe: problems get sent to 'out'.
   */
  bool apply_patterns(const DecodedInstruction &, ProblemSink *out);

  /*
   * Factor of validate_fallthrough, above.  Checks a pair of instructions using
   * the instruction patterns defined in the .cc file, with three possible
   * results:
   *   1. No patterns matched: nothing happens.
   *   2. Patterns matched and were safe: the addresses are filled into
   *      'critical' for use by the second pass.
   *   3. Patterns matched and were unsafe: problems get sent to 'out'.
   *
   * Note: Can be used to parse both one and two instruction patterns. This
   * allows precondition checks to be shared. See comments in the implementation
   * of this (in validator.cc) to see specifics on how to implement both single
   * instruction and two instruction pattern testers.
   */
  bool apply_patterns(const DecodedInstruction &first,
      const DecodedInstruction &second, AddressSet *critical, ProblemSink *out);

  /*
   * Validates all branches found by a previous pass, checking destinations.
   * Returns true iff no problems were found.
   */
  bool validate_branches(const std::vector<CodeSegment> &,
      const AddressSet &branches, const AddressSet &critical,
      ProblemSink *);


  uint32_t bytes_per_bundle_;
  uint32_t data_address_mask_;
  uint32_t code_address_mask_;
  uint32_t code_region_bytes_;
  // Registers which cannot be modified by untrusted code.
  nacl_arm_dec::RegisterList read_only_registers_;
  // Registers which must always contain a valid data region address.
  nacl_arm_dec::RegisterList data_address_registers_;
  // Defines the decoder parser to use.
  const nacl_arm_dec::Arm32DecoderState decode_state_;
};


/*
 * A facade that combines an Instruction with its address and a ClassDecoder.
 * This makes the patterns substantially easier to write and read than managing
 * all three variables separately.
 *
 * ClassDecoders do all decoding on-demand, with no caching.  DecodedInstruction
 * has knowledge of the validator, and pairs a ClassDecoder with a constant
 * Instruction -- so it can cache commonly used values, and does.  Caching
 * safety and defs doubles validator performance.  Add other values only
 * under guidance of a profiler.
 */
class DecodedInstruction {
 public:
  DecodedInstruction(uint32_t vaddr, nacl_arm_dec::Instruction inst,
      const nacl_arm_dec::ClassDecoder &decoder);
  // We permit the default copy ctor and assignment operator.

  uint32_t addr() const { return vaddr_; }

  /*
   * Checks that 'this' always precedes 'other' -- meaning that if 'other'
   * executes, 'this' is guaranteed to have executed.  This is important if
   * 'this' produces a sandboxed value that 'other' must consume.
   *
   * Note: This function is conservative in that if it isn't sure whether
   * this instruction changes the condition, it assumes that it does.
   *
   * Note that this function can't see the bundle size, so this result does not
   * take it into account.  The SfiValidator reasons on this separately.
   */
  bool always_precedes(const DecodedInstruction &other) const {
    return inst_.condition() == other.inst_.condition() &&
        !defines(nacl_arm_dec::kRegisterFlags);
  }

  /*
   * Checks that 'this' always follows 'other' -- meaning that if 'other'
   * executes, 'this' is guaranteed to follow.  This is important if 'other'
   * produces an unsafe value that 'this' fixes before it can leak out.
   *
   * Note: This function is conservative in that if it isn't sure whether the
   * other instruction changes the codntion, it assumes that it does.
   *
   * Note that this function can't see the bundle size, so this result does not
   * take it into account.  The SfiValidator reasons on this separately.
   */
  bool always_follows(const DecodedInstruction &other) const {
    return other.always_precedes(*this);
  }

  /*
   * Checks that the execution of 'this' is conditional on the test result
   * (specifically, the Z flag being set) from 'other' -- which must be
   * adjacent for this simple check to be meaningful.
   */
  bool is_conditional_on(const DecodedInstruction &other) const {
    return inst_.condition() == nacl_arm_dec::Instruction::EQ
        && other.inst_.condition() == nacl_arm_dec::Instruction::AL
        && other.defines(nacl_arm_dec::kRegisterFlags);
  }

  // The methods below mirror those on ClassDecoder, but are cached and cheap.
  nacl_arm_dec::SafetyLevel safety() const { return safety_; }
  nacl_arm_dec::RegisterList defs() const { return defs_; }

  // The methods below pull values from ClassDecoder on demand.
  bool is_relative_branch() const {
    return decoder_->is_relative_branch(inst_);
  }

  const nacl_arm_dec::Register branch_target_register() const {
    return decoder_->branch_target_register(inst_);
  }

  bool is_literal_pool_head() const {
    return decoder_->is_literal_pool_head(inst_);
  }

  uint32_t branch_target() const {
    return vaddr_ + decoder_->branch_target_offset(inst_);
  }

  const nacl_arm_dec::Register base_address_register() const {
    return decoder_->base_address_register(inst_);
  }

  bool offset_is_immediate() const {
    return decoder_->offset_is_immediate(inst_);
  }

  bool clears_bits(uint32_t mask) const {
    return decoder_->clears_bits(inst_, mask);
  }

  bool sets_Z_if_bits_clear(nacl_arm_dec::Register r, uint32_t mask) const {
    return decoder_->sets_Z_if_bits_clear(inst_, r, mask);
  }

  nacl_arm_dec::RegisterList immediate_addressing_defs() const {
    return decoder_->immediate_addressing_defs(inst_);
  }

  // Some convenience methods, defined in terms of ClassDecoder:
  bool defines(nacl_arm_dec::Register r) const {
    return defs().contains_all(r);
  }

  bool defines_any(nacl_arm_dec::RegisterList rl) const {
    return defs().contains_any(rl);
  }

  bool defines_all(nacl_arm_dec::RegisterList rl) const {
    return defs().contains_all(rl);
  }

 private:
  uint32_t vaddr_;
  nacl_arm_dec::Instruction inst_;
  const nacl_arm_dec::ClassDecoder *decoder_;

  nacl_arm_dec::SafetyLevel safety_;
  nacl_arm_dec::RegisterList defs_;
};


/*
 * Describes a memory region that contains executable code.  Note that the code
 * need not live in its final location -- we pretend the code lives at the
 * provided start_addr, regardless of where the base pointer actually points.
 */
class CodeSegment {
 public:
  CodeSegment(const uint8_t *base, uint32_t start_addr, size_t size)
      : base_(base), start_addr_(start_addr), size_(size) {}

  uint32_t begin_addr() const { return start_addr_; }
  uint32_t end_addr() const { return start_addr_ + size_; }
  uint32_t size() const { return size_; }
  bool contains_address(uint32_t a) const {
    return (a >= begin_addr()) && (a < end_addr());
  }

  const nacl_arm_dec::Instruction operator[](uint32_t address) const {
    const uint8_t *element = &base_[address - start_addr_];
    return nacl_arm_dec::Instruction(
        *reinterpret_cast<const uint32_t *>(element));
  }

  bool operator<(const CodeSegment &other) const {
    return start_addr_ < other.start_addr_;
  }

 private:
  const uint8_t *base_;
  uint32_t start_addr_;
  size_t size_;
};


/*
 * A class that consumes reports of validation problems, and may decide whether
 * to continue validating, or early-exit.
 *
 * In a sel_ldr context, we early-exit at the first problem we find.  In an SDK
 * context, however, we collect more reports to give the developer feedback;
 * even then it may be desirable to exit after the first, say, 200 reports.
 */
class ProblemSink {
 public:
  virtual ~ProblemSink() {}

  /*
   * Reports a problem in untrusted code.
   *  vaddr: the virtual address where the problem occurred.  Note that this is
   *      probably not the address of memory that contains the offending
   *      instruction, since we allow CodeSegments to lie about their base
   *      addresses.
   *  safety: the safety level of the instruction, as reported by the decoder.
   *      This may be MAY_BE_SAFE while still indicating a problem.
   *  problem_code: a constant string, defined below, that uniquely identifies
   *      the problem.  These are not intended to be human-readable, and should
   *      be looked up for localization and presentation to the developer.
   *  ref_vaddr: A second virtual address of more code that affected the
   *      decision -- typically a branch target.
   */
  virtual void report_problem(uint32_t vaddr, nacl_arm_dec::SafetyLevel safety,
      const nacl::string &problem_code, uint32_t ref_vaddr = 0) {
    UNREFERENCED_PARAMETER(vaddr);
    UNREFERENCED_PARAMETER(safety);
    UNREFERENCED_PARAMETER(problem_code);
    UNREFERENCED_PARAMETER(ref_vaddr);
  }

  /*
   * Called after each invocation of report_problem.  If this returns false,
   * the validator exits.
   */
  virtual bool should_continue() { return false; }
};


/*
 * Strings used to describe the current set of validator problems.  These may
 * be worth splitting into a separate header file, so that dev tools can
 * process them into localized messages without needing to pull in the whole
 * validator...we'll see.
 */

// An instruction is unsafe -- more information in the SafetyLevel.
const char * const kProblemUnsafe = "kProblemUnsafe";
// A branch would break a pseudo-operation pattern.
const char * const kProblemBranchSplitsPattern = "kProblemBranchSplitsPattern";
// A branch targets an invalid code address (out of segment).
const char * const kProblemBranchInvalidDest = "kProblemBranchInvalidDest";
// A load/store uses an unsafe (non-masked) base address.
const char * const kProblemUnsafeLoadStore = "kProblemUnsafeLoadStore";
// A branch uses an unsafe (non-masked) destination address.
const char * const kProblemUnsafeBranch = "kProblemUnsafeBranch";
// An instruction updates a data-address register (e.g. SP) without masking.
const char * const kProblemUnsafeDataWrite = "kProblemUnsafeDataWrite";
// An instruction updates a read-only register (e.g. r9).
const char * const kProblemReadOnlyRegister = "kProblemReadOnlyRegister";
// A pseudo-op pattern crosses a bundle boundary.
const char * const kProblemPatternCrossesBundle =
    "kProblemPatternCrossesBundle";
// A linking branch instruction is not in the last bundle slot.
const char * const kProblemMisalignedCall = "kProblemMisalignedCall";

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_VALIDATOR_H
