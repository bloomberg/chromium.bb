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

#include <limits>
#include <stdlib.h>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator_arm/address_set.h"
#include "native_client/src/trusted/cpu_features/arch/arm/cpu_arm.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode.h"
#include "native_client/src/trusted/validator_arm/inst_classes.h"
#include "native_client/src/trusted/validator_arm/model.h"

namespace nacl_arm_val {

// Forward declarations of classes used by-reference in the validator, and
// defined at the end of this file.
class CodeSegment;
class DecodedInstruction;
class ProblemSink;

// A simple model of an instruction bundle.  Bundles consist of one or more
// instructions (two or more, in the useful case); the precise size is
// controlled by the parameters passed into SfiValidator, below.
class Bundle {
 public:
  Bundle(uint32_t virtual_base, uint32_t size_bytes)
      : virtual_base_(virtual_base), size_(size_bytes) {}

  uint32_t begin_addr() const { return virtual_base_; }
  uint32_t end_addr() const { return virtual_base_ + size_; }

  bool operator!=(const Bundle& other) const {
    // Note that all Bundles are currently assumed to be the same size.
    return virtual_base_ != other.virtual_base_;
  }

 private:
  uint32_t virtual_base_;
  uint32_t size_;
};

// The SFI validator itself.  The validator is controlled by the following
// inputs:
//   bytes_per_bundle - the number of bytes in each bundle of instructions.
//       Currently this tends to be 16, but we've evaluated alternatives.
//       Must be a power of two.
//   code_region_bytes - number of bytes in the code region, starting at address
//       0 and including the trampolines, etc.  Must be a power of two.
//   data_region_bits - number of bytes in the data region, starting at address
//       0 and including the code region.  Must be a power of two.
//   read_only_registers - registers that untrusted code must not alter (but may
//       read). This currently applies to r9, where we store some thread state.
//   data_address_registers - registers that must contain a valid data-region
//       address at all times.  This currently applies to the stack pointer, but
//       could be extended to include a frame pointer for C-like languages.
//   cpu_features - the ARM CPU whose features should be considered during
//       validation. This matters because some CPUs don't support some
//       instructions, leak information or have erratas when others do not,
//       yet we still want to emit performant code for the given target.
//
// The values of these inputs will typically be taken from the headers of
// untrusted code -- either by the ABI version they indicate, or (perhaps in
// the future) explicit indicators of what SFI model they follow.
class SfiValidator {
 public:
  SfiValidator(uint32_t bytes_per_bundle,
               uint32_t code_region_bytes,
               uint32_t data_region_bytes,
               nacl_arm_dec::RegisterList read_only_registers,
               nacl_arm_dec::RegisterList data_address_registers,
               const NaClCPUFeaturesArm *cpu_features);

  explicit SfiValidator(const SfiValidator& v);

  // The main validator entry point.  Validates the provided CodeSegments,
  // which must be in sorted order, reporting any problems through the
  // ProblemSink.
  //
  // Returns true iff no problems were found.
  bool validate(const std::vector<CodeSegment>& segments, ProblemSink* out);

  // Entry point for validation of dynamic code replacement. Allows
  // micromodifications of dynamically generated code in form of
  // constant updates for inline caches and similar VM techniques.
  // Very minimal modifications allowed, essentially only immediate
  // value update for MOV or ORR instruction.
  // Returns true iff no problems were found.
  bool ValidateSegmentPair(const CodeSegment& old_code,
                           const CodeSegment& new_code,
                           ProblemSink* out);

  // Entry point for dynamic code creation. Copies code from
  // source segment to destination, performing validation
  // and accounting for need of safe handling of cases,
  // where code being replaced is executed.
  // Returns true iff no problems were found.
  bool CopyCode(const CodeSegment& source_code,
                CodeSegment& dest_code,
                NaClCopyInstructionFunc copy_func,
                ProblemSink* out);

  // A 2-dimensional array, defined on the Condition of two
  // instructions, defining when we can statically prove that the
  // conditions of the first instruction implies the conditions of the
  // second instruction.
  //
  // Note: The first index (i.e. row) corresponds to the condition of
  // the first instruction, while the second index (i.e. column)
  // corresponds to the condition of the second instruction.
  //
  // Note: The order the instructions execute is not important in
  // this array. The context defines which instruction, of the
  // instruction pair being compared, appears first.
  //
  // Note: The decoder should prevent UNCONDITIONAL (0b1111) from ever
  // occurring, but we include entries for it out of paranoia, which also
  // happens to make the table 16x16, which is easier to index into.
  static const bool
  condition_implies[nacl_arm_dec::Instruction::kConditionSize + 1]
                   [nacl_arm_dec::Instruction::kConditionSize + 1];

  // Checks whether the given Register always holds a valid data region address.
  // This implies that the register is safe to use in unguarded stores.
  bool is_data_address_register(nacl_arm_dec::Register) const;

  // Number of A32 instructions per bundle.
  uint32_t InstructionsPerBundle() const {
    return bytes_per_bundle_ / (nacl_arm_dec::kArm32InstSize / 8);
  }

  uint32_t code_address_mask() const {
    return ~(code_region_bytes_ - 1) | (bytes_per_bundle_ - 1);
  }
  uint32_t data_address_mask() const {
    return ~(data_region_bytes_ - 1);
  }

  nacl_arm_dec::RegisterList read_only_registers() const {
    return read_only_registers_;
  }
  nacl_arm_dec::RegisterList data_address_registers() const {
    return data_address_registers_;
  }

  const NaClCPUFeaturesArm *CpuFeatures() const {
    return &cpu_features_;
  }

  bool conditional_memory_access_allowed_for_sfi() const {
    return NaClGetCPUFeatureArm(CpuFeatures(), NaClCPUFeatureArm_CanUseTstMem);
  }

  // Utility function that applies the decoder of the validator.
  const nacl_arm_dec::ClassDecoder& decode(
      nacl_arm_dec::Instruction inst) const {
    return decode_state_.decode(inst);
  }

  // Returns the Bundle containing a given address.
  const Bundle bundle_for_address(uint32_t) const;

  // Copy the given validator state.
  SfiValidator& operator=(const SfiValidator& v);

 private:
  // The SfiValidator constructor could have been given invalid values.
  // Returns true the values were bad, and send the details to the ProblemSink.
  // This method should be called from every public validation method.
  bool ConstructionFailed(ProblemSink* out);

  bool is_bundle_head(uint32_t address) const;

  // Validates a straight-line execution of the code, applying patterns.  This
  // is the first validation pass, which fills out the AddressSets for
  // consumption by later analyses.
  //   branches - gets filled in with the address of every direct branch.
  //   critical - gets filled in with every address that isn't safe to jump to,
  //       because it would split an otherwise-safe pseudo-op.
  //
  // Returns true iff no problems were found.
  bool validate_fallthrough(const CodeSegment& segment, ProblemSink* out,
      AddressSet* branches, AddressSet* critical);

  // Factor of validate_fallthrough, above.  Checks a single instruction using
  // the instruction patterns defined in the .cc file, with two possible
  // results:
  //   1. No patterns matched, or all were safe: nothing happens.
  //   2. Patterns matched and were unsafe: problems get sent to 'out'.
  bool apply_patterns(const DecodedInstruction& inst, ProblemSink* out);

  // Factor of validate_fallthrough, above.  Checks a pair of instructions using
  // the instruction patterns defined in the .cc file, with three possible
  // results:
  //   1. No patterns matched: nothing happens.
  //   2. Patterns matched and were safe: the addresses are filled into
  //      'critical' for use by the second pass.
  //   3. Patterns matched and were unsafe: problems get sent to 'out'.
  //
  // Note: Can be used to parse both one and two instruction patterns. This
  // allows precondition checks to be shared. See comments in the implementation
  // of this (in validator.cc) to see specifics on how to implement both single
  // instruction and two instruction pattern testers.
  bool apply_patterns(const DecodedInstruction& first,
                      const DecodedInstruction& second, AddressSet* critical,
                      ProblemSink* out);

  // Validates all branches found by a previous pass, checking destinations.
  // Returns true iff no problems were found.
  bool validate_branches(const std::vector<CodeSegment>& segments,
      const AddressSet& branches, const AddressSet& critical,
      ProblemSink* out);


  NaClCPUFeaturesArm cpu_features_;
  uint32_t bytes_per_bundle_;
  uint32_t code_region_bytes_;
  uint32_t data_region_bytes_;
  // Registers which cannot be modified by untrusted code.
  nacl_arm_dec::RegisterList read_only_registers_;
  // Registers which must always contain a valid data region address.
  nacl_arm_dec::RegisterList data_address_registers_;
  // Defines the decoder parser to use.
  const nacl_arm_dec::Arm32DecoderState decode_state_;
  // True if construction failed and further validation should be prevented.
  bool construction_failed_;
};


// A facade that combines an Instruction with its address and a ClassDecoder.
// This makes the patterns substantially easier to write and read than managing
// all three variables separately.
//
// ClassDecoders do all decoding on-demand, with no caching.  DecodedInstruction
// has knowledge of the validator, and pairs a ClassDecoder with a constant
// Instruction -- so it can cache commonly used values, and does.  Caching
// safety and defs doubles validator performance.  Add other values only
// under guidance of a profiler.
class DecodedInstruction {
 public:
  DecodedInstruction(uint32_t vaddr, nacl_arm_dec::Instruction inst,
      const nacl_arm_dec::ClassDecoder& decoder);

  uint32_t addr() const { return vaddr_; }

  // 'this' dominates 'other', where 'this' is the instruction
  // immediately preceding 'other': if 'other' executes, we can guarantee
  // that 'this' was executed as well.

  // This is important if 'this' produces a sandboxed value that 'other'
  // must consume.
  //
  // Note: If the conditions of the two instructions do
  // not statically infer that the conditional execution is correct,
  // we assume that it is not.
  //
  // Note that this function can't see the bundle size, so this result
  // does not take it into account.  The SfiValidator reasons on this
  // separately.
  bool always_dominates(const DecodedInstruction& other) const {
    nacl_arm_dec::Instruction::Condition cond1 = inst_.GetCondition();
    nacl_arm_dec::Instruction::Condition cond2 = other.inst_.GetCondition();
    return !defines(nacl_arm_dec::Register::Conditions()) &&
        // TODO(jfb) Put back mixed-condition handling. See issue #3221.
        //           SfiValidator::condition_implies[cond2][cond1];
        ((cond1 == nacl_arm_dec::Instruction::AL) || (cond1 == cond2));
  }

  // 'this' post-dominates 'other', where 'other' is the instruction
  // immediately preceding 'this': if 'other' executes, we can guarantee
  // that 'this' is executed as well.
  //
  // This is important if 'other' produces an unsafe value that 'this'
  // fixes before it can leak out.
  //
  // Note: if the conditions of the two
  // instructions do not statically infer that the conditional
  // execution is correct, we assume that it is not.
  //
  // Note that this function can't see the bundle size, so this result
  // does not take it into account.  The SfiValidator reasons on this
  // separately.
  bool always_postdominates(const DecodedInstruction& other) const {
    nacl_arm_dec::Instruction::Condition cond1 = other.inst_.GetCondition();
    nacl_arm_dec::Instruction::Condition cond2 = inst_.GetCondition();
    return !other.defines(nacl_arm_dec::Register::Conditions()) &&
        // TODO(jfb) Put back mixed-condition handling. See issue #3221.
        //           SfiValidator::condition_implies[cond1][cond2];
        ((cond2 == nacl_arm_dec::Instruction::AL) || (cond1 == cond2));
  }

  // Checks that the execution of 'this' is conditional on the test result
  // (specifically, the Z flag being set) from 'other' -- which must be
  // adjacent for this simple check to be meaningful.
  bool is_eq_conditional_on(const DecodedInstruction& other) const {
    return inst_.GetCondition() == nacl_arm_dec::Instruction::EQ
        && other.inst_.GetCondition() == nacl_arm_dec::Instruction::AL
        && other.defines(nacl_arm_dec::Register::Conditions());
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
    // branch_target_offset takes care of adding 8 to the instruction's
    // immediate: the ARM manual states that "PC reads as the address of
    // the current instruction plus 8".
    return vaddr_ + decoder_->branch_target_offset(inst_);
  }

  const nacl_arm_dec::Register base_address_register() const {
    return decoder_->base_address_register(inst_);
  }

  bool is_literal_load() const {
    return decoder_->is_literal_load(inst_);
  }

  bool clears_bits(uint32_t mask) const {
    return decoder_->clears_bits(inst_, mask);
  }

  bool sets_Z_if_bits_clear(nacl_arm_dec::Register r, uint32_t mask) const {
    return decoder_->sets_Z_if_bits_clear(inst_, r, mask);
  }

  bool base_address_register_writeback_small_immediate() const {
    return decoder_->base_address_register_writeback_small_immediate(inst_);
  }

  bool is_load_thread_address_pointer() const {
    return decoder_->is_load_thread_address_pointer(inst_);
  }

  // Some convenience methods, defined in terms of ClassDecoder:
  bool defines(nacl_arm_dec::Register r) const {
    return defs().Contains(r);
  }

  bool defines_any(nacl_arm_dec::RegisterList rl) const {
    return defs().ContainsAny(rl);
  }

  bool defines_all(nacl_arm_dec::RegisterList rl) const {
    return defs().ContainsAll(rl);
  }

  // Returns true if the instruction uses the given register.
  bool uses(nacl_arm_dec::Register r) const {
     return decoder_->uses(inst_).Contains(r);
  }

  const nacl_arm_dec::Instruction& inst() const {
    return inst_;
  }

  DecodedInstruction& Copy(const DecodedInstruction& other) {
    vaddr_ = other.vaddr_;
    inst_.Copy(other.inst_);
    decoder_ = other.decoder_;
    safety_ = other.safety_;
    defs_.Copy(other.defs_);
    return *this;
  }

 private:
  uint32_t vaddr_;
  nacl_arm_dec::Instruction inst_;
  const nacl_arm_dec::ClassDecoder* decoder_;

  nacl_arm_dec::SafetyLevel safety_;
  nacl_arm_dec::RegisterList defs_;

  NACL_DISALLOW_COPY_AND_ASSIGN(DecodedInstruction);
};

// Describes a memory region that contains executable code.  Note that the code
// need not live in its final location -- we pretend the code lives at the
// provided start_addr, regardless of where the base pointer actually points.
class CodeSegment {
 public:
  CodeSegment(const uint8_t* base, uint32_t start_addr, size_t size)
      : base_(base),
        start_addr_(start_addr),
        size_(static_cast<uint32_t>(size)) {
    CHECK(size <= std::numeric_limits<uint32_t>::max());
    CHECK(start_addr <= std::numeric_limits<uint32_t>::max() - size_);
  }

  uint32_t begin_addr() const { return start_addr_; }
  uint32_t end_addr() const { return start_addr_ + size_; }
  uint32_t size() const { return size_; }
  bool contains_address(uint32_t a) const {
    return (a >= begin_addr()) && (a < end_addr());
  }

  const nacl_arm_dec::Instruction operator[](uint32_t address) const {
    const uint8_t* element = &base_[address - start_addr_];
    return nacl_arm_dec::Instruction(
        *reinterpret_cast<const uint32_t *>(element));
  }

  bool operator<(const CodeSegment& other) const {
    return start_addr_ < other.start_addr_;
  }

  const uint8_t* base() const {
    return base_;
  }

 private:
  const uint8_t* base_;
  uint32_t start_addr_;
  uint32_t size_;
};

// Enumerated type of possible problems reported by the validator.
// Specific problems are identified by this enumerated type, and
// the corresponding call to report problem in class ProblemSink (below).
typedef enum {
  // An instruction is unsafe -- more information in the SafetyLevel.
  // Generated by:
  //    ReportProblemSafety
  kProblemUnsafe,
  // A branch would break a pseudo-operation pattern.
  // Generated by:
  //    ReprotProblemAddress
  kProblemBranchSplitsPattern,
  // A branch targets an invalid code address (out of segment).
  // Generated by:
  //    ReportProblemAddress
  kProblemBranchInvalidDest,
  // An load/store uses an unsafe (non-masked) base address.
  // Generated by:
  //    ReportProblemRegisterInstructionPair - atomic issues.
  //    ReportProblemRegister - masking issues.
  kProblemUnsafeLoadStore,
  // A load/store on PC that doesn't increment with a valid immediate
  // address.
  // Generated by:
  //    ReportProblem
  kProblemIllegalPcLoadStore,
  // A branch uses an unsafe (non-masked) destination address.
  // Generated by:
  //    ReportProblemRegisterInstructionPair - atomic issues.
  //    ReportProblemRegister - masking issues.
  kProblemUnsafeBranch,
  // An instruction updates a data-address register(s) (e.g. SP)
  // without masking.
  // Generated by:
  //    ReportProblemRegisterListInstructionPair - atomic issues.
  //    ReportProblemRegisterList - masking issues.
  kProblemUnsafeDataWrite,
  // An instruction updates a read-only register(s) (e.g. r9).
  // Generated by:
  //    ReportProblemRegisterList
  kProblemReadOnlyRegister,
  // A pseudo-op pattern crosses a bundle boundary.
  // Generated by:
  //    ReportRegisterInstructionPair
  kProblemPatternCrossesBundle,
  // A linking branch instruction is not in the last bundle slot.
  // Generated by:
  //    ReportProblem
  kProblemMisalignedCall,
  // Construction of the SfiValidator failed because its arguments were invalid.
  kProblemConstructionFailed,
  // Code uses thread pointer in non-load TLS pointer situation.
  kProblemIllegalUseOfThreadPointer,
  // Special constant defining the number of elements in this list.
  kValidatorProblemSize
} ValidatorProblem;

// Defines types of two instruction failures.
typedef enum {
  // No specific known reason for instruction pair failing.
  kNoSpecificPairProblem,
  // First instruction sets conditions flags, and hence, can't
  // guarantee that the next instruction will always be executed.
  kFirstSetsConditionFlags,
  // Conditions on instructions don't guarantee that instructions
  // will run atomically.
  kConditionsOnPairNotSafe,
  // Second is dependent on eq being set by first instruction.
  kEqConditionalOn,
  // TST+LDR or TST+STR was used, but it's disallowed on this CPU.
  kTstMemDisallowed,
  // Instruction pair crosses bundle boundary.
  kPairCrossesBundle
} ValidatorInstructionPairProblem;

// Defines the maximum number of data elements assocated with validator
// problem user data.
static const size_t kValidatorProblemUserDataSize = 6;

// Defines array used to hold user data associated with a problem.
typedef uint32_t ValidatorProblemUserData[kValidatorProblemUserDataSize];

// Defines Which ReportProblem... function was called to generate
// user data associated with the problem.
typedef enum {
  kReportProblemSafety,
  kReportProblem,
  kReportProblemAddress,
  kReportProblemInstructionPair,
  kReportProblemRegister,
  kReportProblemRegisterInstructionPair,
  kReportProblemRegisterList,
  kReportProblemRegisterListInstructionPair
} ValidatorProblemMethod;

// A class that consumes reports of validation problems, and may decide whether
// to continue validating, or early-exit.
//
// In a sel_ldr context, we early-exit at the first problem we find.  In an SDK
// context, however, we collect more reports to give the developer feedback;
// even then it may be desirable to exit after the first, say, 200 reports.
class ProblemSink {
 public:
  ProblemSink() {}
  virtual ~ProblemSink() {}

  // Helper function for reporting safety level issues.
  //    vaddr - the virtual address where the problem occurred.
  //    safety - the (unsafe) safety level being reported.
  void ReportProblemSafety(uint32_t vaddr, nacl_arm_dec::SafetyLevel safety);

  // Helper function for reporting a simple problem with no user data.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  void ReportProblem(uint32_t vaddr, ValidatorProblem problem);

  // Helper function for reporting a problem with a specific instruction
  // address.
  void ReportProblemAddress(uint32_t vaddr, ValidatorProblem problem,
                            uint32_t problem_vaddr);

  // Helper function for reporting a problem with a pair of instructions.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    first - The first instruction of the instruction pair.
  //    second - The second instruction of the instruction pair.
  void ReportProblemInstructionPair(
      uint32_t vaddr, ValidatorProblem problem,
      ValidatorInstructionPairProblem pair_problem,
      const DecodedInstruction& first, const DecodedInstruction& second);

  // Helper function for reporting problems associated with a register.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    reg - The register associated with the problem.
  void ReportProblemRegister(uint32_t vaddr, ValidatorProblem problem,
                             nacl_arm_dec::Register reg);

  // Helper function for reporting an instruction pair that has
  // issues with how a register is set.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    reg - The register associated with the problem.
  //    first - The first instruction of the instruction pair.
  //    second - The second instruction of the instruction pair.
  void ReportProblemRegisterInstructionPair(
      uint32_t vaddr, ValidatorProblem problem,
      ValidatorInstructionPairProblem pair_problem,
      nacl_arm_dec::Register reg,
      const DecodedInstruction& first, const DecodedInstruction& second);

  // Helper function for reporting problems associated with a register list.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    registers - The register list associated with the problem.
  void ReportProblemRegisterList(uint32_t vaddr, ValidatorProblem problem,
                                 nacl_arm_dec::RegisterList registers);

  // Helper function for reporting an instruction pair that has
  // isseus with how a register list is set.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    registers - The register list associated with the problem.
  //    first - The first instruction of the instruction pair.
  //    second - The second instruction of the instruction pair.
  void ReportProblemRegisterListInstructionPair(
      uint32_t vaddr, ValidatorProblem problem,
      ValidatorInstructionPairProblem pair_problem,
      nacl_arm_dec::RegisterList registers,
      const DecodedInstruction& first, const DecodedInstruction& second);

  // Called after each invocation of report_problem.  If this returns false,
  // the validator exits.
  virtual bool should_continue() { return false; }


 protected:
  // Reports a problem in untrusted code. All public report problem functions
  // are automatically converted to a call to this function.
  //  vaddr - the virtual address where the problem occurred.  Note that this is
  //      probably not the address of memory that contains the offending
  //      instruction, since we allow CodeSegments to lie about their base
  //      addresses.
  //  problem - An enumerated type defining the type of problem found.
  //  method - The reporting method used to generate user data.
  //  user_data - An array of additional information about the instruction.
  //
  // Default implementation does nothing!
  virtual void ReportProblemInternal(uint32_t vaddr,
                                     ValidatorProblem problem,
                                     ValidatorProblemMethod method,
                                     ValidatorProblemUserData user_data);

  // Returns the number of elements in user_data, for the given method.
  // Used so that we can blindly copy/compare user data.
  static size_t UserDataSize(ValidatorProblemMethod method);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ProblemSink);
};

}  // namespace nacl_arm_val

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_VALIDATOR_H
