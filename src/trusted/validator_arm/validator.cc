/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/validator_arm/validator.h"
#include "native_client/src/include/nacl_macros.h"

#include <assert.h>

using nacl_arm_dec::Instruction;
using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;
using nacl_arm_dec::kRegisterNone;
using nacl_arm_dec::kRegisterPc;
using nacl_arm_dec::kRegisterLink;

using std::vector;

namespace nacl_arm_val {

/*********************************************************
 * Implementations of patterns used in the first pass.
 *
 * N.B.  IF YOU ADD A PATTERN HERE, REGISTER IT BELOW.
 * See the list in apply_patterns.
 *********************************************************/

// A possible result from a validator pattern.
enum PatternMatch {
  // The pattern does not apply to the instructions it was given.
  NO_MATCH,
  // The pattern matches a single instruction, and is safe.
  PATTERN_SAFE,
  // The pattern matches a double instruction, and is safe;
  // do not allow jumps to split it.
  PATTERN_SAFE_2,
  // The pattern matches, and has detected a problem.
  PATTERN_UNSAFE
};

/*
 * Ensures that all loads/stores use a safe base address.  A base address is
 * safe if it
 *     1. Has specific bits masked off by its immediate predecessor, or
 *     2. Is predicated on those bits being clear, as tested by its immediate
 *        predecessor, or
 *     3. Is in a register defined as always containing a safe address.
 *
 * This pattern concerns itself with case #1, early-exiting if it finds #2.
 */
static PatternMatch check_loadstore_mask(const SfiValidator &sfi,
                                         const DecodedInstruction &first,
                                         const DecodedInstruction &second,
                                         ProblemSink *out) {
  if (second.base_address_register() == kRegisterNone /* not a load/store */
      || sfi.is_data_address_register(second.base_address_register())) {
    return NO_MATCH;
  }

  if (second.base_address_register() == kRegisterPc
      && second.offset_is_immediate()) {
    /* PC + immediate addressing is always safe. */
    return PATTERN_SAFE;
  }

  if (first.defines(second.base_address_register())
      && first.clears_bits(sfi.data_address_mask())
      && first.always_precedes(second)) {
    return PATTERN_SAFE_2;
  }

  if (first.sets_Z_if_bits_clear(second.base_address_register(),
                                 sfi.data_address_mask())
      && second.is_conditional_on(first)) {
    return PATTERN_SAFE_2;
  }

  out->report_problem(second.addr(), second.safety(), kProblemUnsafeLoadStore);
  return PATTERN_UNSAFE;
}

/*
 * Ensures that all indirect branches use a safe destination address.  A
 * destination address is safe if it has specific bits masked off by its
 * immediate predecessor.
 */
static PatternMatch check_branch_mask(const SfiValidator &sfi,
                                      const DecodedInstruction &first,
                                      const DecodedInstruction &second,
                                      ProblemSink *out) {
  if (second.branch_target_register() == kRegisterNone) return NO_MATCH;

  if (first.defines(second.branch_target_register())
      && first.clears_bits(sfi.code_address_mask())
      && first.always_precedes(second)) {
    return PATTERN_SAFE_2;
  }

  out->report_problem(second.addr(), second.safety(), kProblemUnsafeBranch);
  return PATTERN_UNSAFE;
}

/*
 * Verifies that any instructions that update a data-address register are
 * immediately followed by a mask.
 */
static PatternMatch check_data_register_update(const SfiValidator &sfi,
                                               const DecodedInstruction &first,
                                               const DecodedInstruction &second,
                                               ProblemSink *out) {
  if (!first.defines_any(sfi.data_address_registers())) return NO_MATCH;

  // A single safe data register update doesn't affect control flow.
  if (first.clears_bits(sfi.data_address_mask())) return NO_MATCH;

  // Exempt updates due to writeback
  RegisterList data_addr_defs = first.defs() & sfi.data_address_registers();
  if ((first.immediate_addressing_defs() & sfi.data_address_registers())
      == data_addr_defs) {
    return NO_MATCH;
  }

  if (second.defines_all(data_addr_defs)
      && second.clears_bits(sfi.data_address_mask())
      && second.always_follows(first)) {
    return PATTERN_SAFE_2;
  }

  out->report_problem(first.addr(), first.safety(), kProblemUnsafeDataWrite);
  return PATTERN_UNSAFE;
}

/*
 * Checks the location of linking branches -- to be useful, they must be in
 * the last bundle slot.
 *
 * This is not a security check per se, more of a guard against Stupid Compiler
 * Tricks.
 */
static PatternMatch check_call_position(const SfiValidator &sfi,
                                        const DecodedInstruction &inst,
                                        ProblemSink *out) {
  // Identify linking branches through their definitions:
  if (inst.defines_all(kRegisterPc + kRegisterLink)) {
    uint32_t last_slot = sfi.bundle_for_address(inst.addr()).end_addr() - 4;
    if (inst.addr() != last_slot) {
      out->report_problem(inst.addr(), inst.safety(), kProblemMisalignedCall);
      return PATTERN_UNSAFE;
    }
  }
  return NO_MATCH;
}

/*
 * Checks for instructions that alter any read-only register.
 */
static PatternMatch check_read_only(const SfiValidator &sfi,
                                    const DecodedInstruction &inst,
                                    ProblemSink *out) {
  if (inst.defines_any(sfi.read_only_registers())) {
    out->report_problem(inst.addr(), inst.safety(), kProblemReadOnlyRegister);
    return PATTERN_UNSAFE;
  }

  return NO_MATCH;
}

/*
 * Checks writes to r15 from instructions that aren't branches.
 */
static PatternMatch check_pc_writes(const SfiValidator &sfi,
                                    const DecodedInstruction &inst,
                                    ProblemSink *out) {
  if (inst.is_relative_branch()
      || inst.branch_target_register() != kRegisterNone) {
    // It's a branch.
    return NO_MATCH;
  }

  if (!inst.defines(nacl_arm_dec::kRegisterPc)) return NO_MATCH;

  if (inst.clears_bits(sfi.code_address_mask())) {
    return PATTERN_SAFE;
  } else {
    out->report_problem(inst.addr(), inst.safety(), kProblemUnsafeBranch);
    return PATTERN_UNSAFE;
  }
}

/*********************************************************
 *
 * Implementation of SfiValidator itself.
 *
 *********************************************************/

SfiValidator::SfiValidator(uint32_t bytes_per_bundle,
                           uint32_t code_region_bytes,
                           uint32_t data_region_bytes,
                           RegisterList read_only_registers,
                           RegisterList data_address_registers)
    : bytes_per_bundle_(bytes_per_bundle),
      data_address_mask_(~(data_region_bytes - 1)),
      code_address_mask_(~(code_region_bytes - 1) | (bytes_per_bundle - 1)),
      code_region_bytes_(code_region_bytes),
      read_only_registers_(read_only_registers),
      data_address_registers_(data_address_registers),
      decode_state_() {}

SfiValidator::SfiValidator(const SfiValidator& v)
    : bytes_per_bundle_(v.bytes_per_bundle_),
      data_address_mask_(v.data_address_mask_),
      code_address_mask_(v.code_address_mask_),
      code_region_bytes_(v.code_region_bytes_),
      read_only_registers_(v.read_only_registers_),
      data_address_registers_(v.data_address_registers_),
      decode_state_() {}

SfiValidator& SfiValidator::operator=(const SfiValidator& v) {
  bytes_per_bundle_ = v.bytes_per_bundle_;
  data_address_mask_ = v.data_address_mask_;
  code_address_mask_ = v.code_address_mask_;
  code_region_bytes_ = v.code_region_bytes_;
  read_only_registers_ = v.read_only_registers_;
  data_address_registers_ = v.data_address_registers_;
  return *this;
}

bool SfiValidator::validate(const vector<CodeSegment> &segments,
                            ProblemSink *out) {
  uint32_t base = segments[0].begin_addr();
  uint32_t size = segments.back().end_addr() - base;
  AddressSet branches(base, size);
  AddressSet critical(base, size);

  bool complete_success = true;

  for (vector<CodeSegment>::const_iterator it = segments.begin();
      it != segments.end(); ++it) {
    complete_success &= validate_fallthrough(*it, out, &branches, &critical);

    if (!out->should_continue()) {
      return false;
    }
  }

  complete_success &= validate_branches(segments, branches, critical, out);

  return complete_success;
}

bool SfiValidator::validate_fallthrough(const CodeSegment &segment,
                                        ProblemSink *out,
                                        AddressSet *branches,
                                        AddressSet *critical) {
  bool complete_success = true;

  nacl_arm_dec::Forbidden initial_decoder;
  // Initialize the previous instruction to a scary BKPT, so patterns all fail.
  DecodedInstruction pred(
      0,  // Virtual address 0, which will be in a different bundle;
      Instruction(0xE1277777),  // The literal-pool-header BKPT instruction;
      initial_decoder);  // and ensure that it decodes as Forbidden.

  for (uint32_t va = segment.begin_addr(); va != segment.end_addr(); va += 4) {
    DecodedInstruction inst(va, segment[va],
                            decode_state_.decode(segment[va]));

    if (inst.safety() != nacl_arm_dec::MAY_BE_SAFE) {
      out->report_problem(va, inst.safety(), kProblemUnsafe);
      if (!out->should_continue()) {
        return false;
      }
      complete_success = false;
    }

    complete_success &= apply_patterns(inst, out);
    if (!out->should_continue()) return false;

    complete_success &= apply_patterns(pred, inst, critical, out);
    if (!out->should_continue()) return false;

    if (inst.is_relative_branch()) {
      branches->add(inst.addr());
    }

    if (inst.is_literal_pool_head()
        && is_bundle_head(inst.addr())) {
      // Add each instruction in this bundle to the critical set.
      uint32_t last_data_addr = bundle_for_address(va).end_addr();
      for (; va != last_data_addr; va += 4) {
        critical->add(va);
      }

      // Decrement the virtual address by one instruction, so the for
      // loop can bump it back forward.  This is slightly dirty.
      va -= 4;
    }

    pred = inst;
  }

  return complete_success;
}

static bool address_contained(uint32_t va, const vector<CodeSegment> &segs) {
  for (vector<CodeSegment>::const_iterator it = segs.begin(); it != segs.end();
      ++it) {
    if (it->contains_address(va)) return true;
  }
  return false;
}

bool SfiValidator::validate_branches(const vector<CodeSegment> &segments,
                                     const AddressSet &branches,
                                     const AddressSet &critical,
                                     ProblemSink *out) {
  bool complete_success = true;

  vector<CodeSegment>::const_iterator seg_it = segments.begin();

  for (AddressSet::Iterator it = branches.begin(); it != branches.end(); ++it) {
    uint32_t va = *it;

    // Invariant: all addresses in branches are covered by some segment;
    // segments are in sorted order.
    while (!seg_it->contains_address(va)) {
      ++seg_it;
    }

    const CodeSegment &segment = *seg_it;

    DecodedInstruction inst(va, segment[va],
                            decode_state_.decode(segment[va]));

    // We know it is_relative_branch(), so we can simply call:
    uint32_t target_va = inst.branch_target();
    if (address_contained(target_va, segments)) {
      if (critical.contains(target_va)) {
        out->report_problem(va, inst.safety(), kProblemBranchSplitsPattern,
                            target_va);
        if (!out->should_continue()) {
          return false;
        }
        complete_success = false;
      }
    } else if ((target_va & code_address_mask()) == 0) {
      // Allow bundle-aligned, in-range direct jump.
    } else {
      out->report_problem(va, inst.safety(), kProblemBranchInvalidDest,
                          target_va);
      if (!out->should_continue()) {
        return false;
      }
      complete_success = false;
    }
  }

  return complete_success;
}

bool SfiValidator::apply_patterns(const DecodedInstruction &inst,
    ProblemSink *out) {
  // Single-instruction patterns. Should return PATTERN_SAFE if the
  // pattern succeeds.
  typedef PatternMatch (*OneInstPattern)(const SfiValidator &,
                                         const DecodedInstruction &,
                                         ProblemSink *out);
  static const OneInstPattern one_inst_patterns[] = {
    &check_read_only,
    &check_pc_writes,
    &check_call_position,
  };

  bool complete_success = true;

  for (uint32_t i = 0; i < NACL_ARRAY_SIZE(one_inst_patterns); i++) {
    PatternMatch r = one_inst_patterns[i](*this, inst, out);
    switch (r) {
      case PATTERN_SAFE:
      case NO_MATCH:
        break;

      case PATTERN_UNSAFE:
        complete_success = false;
        break;

      case PATTERN_SAFE_2:
        assert(false);  // This should not happen. All patterns in the
                        // list are single instruction patterns.
    }
  }

  return complete_success;
}

bool SfiValidator::apply_patterns(const DecodedInstruction &first,
    const DecodedInstruction &second, AddressSet *critical, ProblemSink *out) {
  // Type for two-instruction pattern functions.
  //
  // Note: These functions can recognize single instruction patterns,
  // as well as two instruction patterns. Single instruction patterns
  // should return PATTERN_SAFE while two instruction patterns should
  // return PATTERN_SAFE_2. In addition, single instruction patterns
  // should be applied to the "second" instruction. The main reason for
  // allowing both pattern lengths is so that precondition tests can
  // be shared, and hence more efficient.
  typedef PatternMatch (*TwoInstPattern)(const SfiValidator &,
                                         const DecodedInstruction &first,
                                         const DecodedInstruction &second,
                                         ProblemSink *out);

  // The list of patterns -- defined in static functions up top.
  static const TwoInstPattern two_inst_patterns[] = {
    &check_loadstore_mask,
    &check_branch_mask,
    &check_data_register_update,
  };

  bool complete_success = true;

  for (uint32_t i = 0; i < NACL_ARRAY_SIZE(two_inst_patterns); i++) {
    PatternMatch r = two_inst_patterns[i](*this, first, second, out);
    switch (r) {
      case PATTERN_SAFE:
      case NO_MATCH: break;

      case PATTERN_UNSAFE:
        // Pattern is in charge of reporting specific issue.
        complete_success = false;
        break;

      case PATTERN_SAFE_2:
        if (bundle_for_address(first.addr())
            != bundle_for_address(second.addr())) {
          complete_success = false;
          out->report_problem(first.addr(), first.safety(),
                              kProblemPatternCrossesBundle);
        } else {
          critical->add(second.addr());
        }
        break;
    }
  }
  return complete_success;
}

bool SfiValidator::is_data_address_register(Register r) const {
  return data_address_registers_[r];
}

const Bundle SfiValidator::bundle_for_address(uint32_t address) const {
  uint32_t base = address - (address % bytes_per_bundle_);
  return Bundle(base, bytes_per_bundle_);
}

bool SfiValidator::is_bundle_head(uint32_t address) const {
  return (address % bytes_per_bundle_) == 0;
}


/*
 * We eagerly compute both safety and defs here, because it turns out to be
 * faster by 10% than doing either lazily and memoizing the result.
 */
DecodedInstruction::DecodedInstruction(uint32_t vaddr,
                                       Instruction inst,
                                       const ClassDecoder &decoder)
    : vaddr_(vaddr),
      inst_(inst),
      decoder_(&decoder),
      safety_(decoder.safety(inst_)),
      defs_(decoder.defs(inst_))
{}

}  // namespace
