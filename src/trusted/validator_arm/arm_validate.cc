/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Validate Arm code meets Native Client Rules.
 */

#include <stdarg.h>

#include <set>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/ncdecode.h"
#include "native_client/src/trusted/validator_arm/validator_patterns.h"

// TODO(cbiffle): this dependency is bad, but we need the info.  Move it out.
#include "native_client/src/trusted/service_runtime/nacl_config.h"

// Defines the block size assumed for unknown branching.
int FLAGS_code_block_size = 16;

// Computes the entry address of a code block, from an address within
// the code block.
inline uint32_t GetCodeBlockEntry(uint32_t address) {
  return address - (address % FLAGS_code_block_size);
}

// Defines the instruction used as a marker of a constant pool.
static const ArmInstKind kDataBlockMarker = ARM_BKPT;

Bool FLAGS_branch_in_text_segment = TRUE;

Bool FLAGS_branch_word_boundary = TRUE;

Bool FLAGS_branch_into_constant_pool = TRUE;

Bool FLAGS_report_disallowed = TRUE;

Bool FLAGS_count_pattern_usage = FALSE;

// Holds the application name for the validator.
static const char *progname = "validate";

// Holds the number of validate errors found.
static unsigned int number_validate_errors = 0;

// Define an address range of form [first, second).
typedef std::pair<uint32_t, uint32_t> AddressRange;

// Holds the set of valid static address ranges of form [min, max).
static std::set<AddressRange>* valid_code_ranges = NULL;

// Holds a map from the set of addresses one can't branch into, to
// the pattern that is broken if the branch is taken. Note:
// If multiple patterns apply at that point, only one will be entered
// into the map.
static std::map<uint32_t, ValidatorPattern*>* unsafe_branch_map = NULL;

// Holds the set of code block entries for data blocks.
static  std::set<uint32_t>* constant_pool_blocks = NULL;

// Holds a map from a pattern, to the coresponding number of times
// the call MayBeUnsafe returns true.
static std::map<ValidatorPattern*, uint32_t>* pattern_check_map = NULL;

// Holds a map from a pattern, to the corresponding number of times
// the call IsSafe returns true.
static std::map<ValidatorPattern*, uint32_t>* pattern_applied_map = NULL;

void PrintPatternUsageCounts() {
  printf("Pattern usage counts:\n");
  printf("+--------+---------------+-----------------------+\n");
  printf("|   safe |         total |      pattern name     |\n");
  printf("+--------+---------------+-----------------------+\n");
  for (std::map<ValidatorPattern*, uint32_t>::iterator iter
           = pattern_check_map->begin();
       iter != pattern_check_map->end();
       ++iter) {
    ValidatorPattern* pattern = iter->first;
    uint32_t candidates = iter->second;
    uint32_t safe = 0;
    std::map<ValidatorPattern*, uint32_t>::iterator safe_pos =
        pattern_applied_map->find(pattern);
    if (safe_pos != pattern_applied_map->end()) {
      safe = safe_pos->second;
    }
    printf("%8d\t%8d\t%s\n", safe, candidates, pattern->GetName().c_str());
  }
}

static void IncrementPatternMap(
    ValidatorPattern* pattern,
    std::map<ValidatorPattern*, uint32_t>* pattern_map) {
  std::map<ValidatorPattern*, uint32_t>::iterator pos =
      pattern_map->find(pattern);
  if (pos == pattern_map->end()) {
    pattern_map->insert(std::make_pair(pattern, 1));
  } else {
    ++(pos->second);
  }
}

void SetValidateProgramName(const char* program_name) {
  progname = program_name;
}

const char* GetValidateProgramName() {
  return progname;
}

void ValidateFatal(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "\n%s: fatal error: ", progname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(2);
}

int ValidateExitCode() {
  return 0 == number_validate_errors ? 0 : 2;
}

void ValidateError(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "\n%s: error: ", progname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  ++number_validate_errors;
}

void ValidateWarn(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "\n%s: warn: ", progname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

// Generate a string describing the disassembled text of the given instruction.
nacl::string InstructionLine(const NcDecodedInstruction* inst) {
  char buffer_1[1024];
  char buffer_2[2014];
  DescribeInst(buffer_1, sizeof(buffer_1), inst);
  snprintf(buffer_2, sizeof(buffer_2),
           "%8x:\t%08x \t%s\n",
           inst->vpc,
           inst->inst,
           buffer_1);
  return nacl::string(buffer_2);
}

// Collect Constant Pool ranges from a text segment, and look
// for unsafe patterns in code blocks.
//
// code_segment - Pointer to the code segment to use.
static void DetectDataBlocksAndApplyPatterns(
    CodeSegment* code_segment) {
  NcDecodeState state(*code_segment);
  bool is_data_block = false;

  for (state.GotoStartPc(); state.HasValidPc(); state.NextInstruction()) {
    // Now check if the next instruction is a beginning of data block marker.
    if (state.CurrentPc() == GetCodeBlockEntry(state.CurrentPc())) {
      if (kDataBlockMarker ==
          state.CurrentInstruction().matched_inst->inst_kind) {
        is_data_block = true;
        constant_pool_blocks->insert(state.CurrentPc());
      } else {
        is_data_block = false;
      }
    }

    if (!is_data_block) {
      // Check if instruction is allowed.
      if (FLAGS_report_disallowed &&
          !state.CurrentInstruction().matched_inst->arm_safe) {
        ValidateError("Instruction not allowed:\n\t%s",
                      InstructionLine(&(state.CurrentInstruction())).c_str());
      }

      // Now check if any patterns are violated.
      for (std::vector<ValidatorPattern*>::iterator iter =
               RegisteredValidatorPatternsBegin();
           iter != RegisteredValidatorPatternsEnd();
           ++iter) {
        ValidatorPattern* pattern = *iter;
        if (pattern->MayBeUnsafe(state)) {
          if (FLAGS_count_pattern_usage) {
            IncrementPatternMap(pattern, pattern_check_map);
          }
          if (pattern->IsSafe(state)) {
            if (FLAGS_count_pattern_usage) {
              IncrementPatternMap(pattern, pattern_applied_map);
            }
            for (uint32_t pc = pattern->StartPc(state);
                pc < pattern->EndPc(state); pc += ARM_WORD_LENGTH) {
              if (unsafe_branch_map->find(pc) == unsafe_branch_map->end()) {
                unsafe_branch_map->insert(std::make_pair(pc, pattern));
              }
            }
            // Now check if any patterns are violated by crossing
            // block boundaries.
            uint32_t start_address = pattern->StartPc(state);
            uint32_t end_address = pattern->EndPc(state) - ARM_WORD_LENGTH;
            uint32_t start_block = GetCodeBlockEntry(start_address);
            uint32_t end_block = GetCodeBlockEntry(end_address);
            if (start_block != end_block) {
              pattern->ReportCrossesBlockBoundary(state);
            }
          } else {
            pattern->ReportUnsafeError(state);
          }
        }
      }
    }
  }
}

// Check if the instruction in the current state is a valid branch instruction.
static void CheckBranchInst(const NcDecodeState &state) {
  const NcDecodedInstruction& branch = state.CurrentInstruction();
  uint32_t target = RealAddress(branch.vpc, branch.values.immediate);

  // Verify that the target is in a text segment, or in the trampoline range.
  if (target >= NACL_TRAMPOLINE_START && target < NACL_TRAMPOLINE_END) {
    /*
     * Branch targets within the trampoline range must target a trampoline
     * entry point.  Note that this is a subset of possible bundle entry
     * points, as the trampolines may be larger than bundles.
     */
    if ((target % NACL_SYSCALL_BLOCK_SIZE) != 0) {
      ValidateError(
          "Branch to trampoline does not target a valid entry point:\n  %s",
          InstructionLine(&branch).c_str());
    }
  } else {
    bool in_text_segment = false;
    for (std::set<AddressRange>::const_iterator iter
             = valid_code_ranges->begin();
         iter != valid_code_ranges->end();
         ++iter) {
      const AddressRange& pair = *iter;
      if (pair.first <= target && pair.second > target) {
        in_text_segment = true;
        break;
      }
    }
    if (!in_text_segment) {
      ValidateError("Branch not in text segment boundaries:\n\t%s",
                    InstructionLine(&branch).c_str());
    }
  }

  // Verify at even word boundary.
  // TODO(cbiffle): the branches we allow can't even *generate* an unaligned
  // address.  This is probably unnecessary, but confirm.
  if (FLAGS_branch_word_boundary && 0 != target % ARM_WORD_LENGTH) {
    ValidateError("Branch to non-word boundary:\n\t%s",
                  InstructionLine(&branch).c_str());
  }

  // Verify that the target isn't inside a constant pool.
  uint32_t target_entry = GetCodeBlockEntry(target);
  if (FLAGS_branch_into_constant_pool &&
      constant_pool_blocks->find(target_entry)
      != constant_pool_blocks->end()) {
    ValidateError("Branch into unprotected constant pool:\n\t%s",
                  InstructionLine(&branch).c_str());
  }

  // Verify that the target isn't inside a pattern.
  std::map<uint32_t, ValidatorPattern*>::const_iterator pos =
      unsafe_branch_map->find(target);
  if (pos != unsafe_branch_map->end()) {
    pos->second->ReportUnsafeBranchTo(state, target);
  }

  /* TODO(kschimpf): Get timing results to see if this solution is
     faster than the map solution.
  for (std::vector<ValidatorPattern*>::iterator iter =
           RegisteredValidatorPatternsBegin();
       iter != RegisteredValidatorPatternsEnd();
       ++iter) {
    ValidatorPattern* pattern = *iter;
    if (!pattern->SafeToBranchTo(state, target)) {
      pattern->ReportUnsafeBranchTo(state, target);
    }
  }
  */
}

/*
 * Check if the instruction in the current state defines a branch,
 * and if so, that the branch is valid.
 *
 * Note that only direct (immediate) branches are checked here.  Indirect
 * branches can be checked in isolation without global context, and are
 * handled by dedicated patterns.
 */
static void CheckBranchInstructions(const NcDecodeState &state) {
  switch (state.CurrentInstruction().matched_inst->inst_kind) {
    case ARM_BRANCH_INST:
    case ARM_BRANCH_AND_LINK:
      CheckBranchInst(state);
      break;
    default:
      break;
  }
}

// Validate control flow for the given code segment.
static void ValidateControlFlow(CodeSegment* code_segment) {
  NcDecodeState state(*code_segment);

  bool is_data_block = false;
  for (state.GotoStartPc(); state.HasValidPc(); state.NextInstruction()) {
    if (state.CurrentPc() == GetCodeBlockEntry(state.CurrentPc())) {
      // Mark if data block.
      is_data_block = false;
      if (constant_pool_blocks->find(state.CurrentPc())
          != constant_pool_blocks->end()) {
        is_data_block = true;
      }
    }

    if (!is_data_block) {
      CheckBranchInstructions(state);
    }
  }
}

// Analyze the sections in an executable file.
static int ValidateSections(ncfile *ncf) {
  int badsections = 0;
  Elf_Shdr *shdr = ncf->sheaders;

  printf("*** Looking for constant pools and checking patterns ***\n");
  for (int ii = 0; ii < ncf->shnum; ii++) {
    // NcDecodeState state;
    printf("section %d sh_addr %x offset %x flags %x\n",
           ii, (uint32_t)shdr[ii].sh_addr,
           (uint32_t)shdr[ii].sh_offset, (uint32_t)shdr[ii].sh_flags);
    if ((shdr[ii].sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR)
      continue;
    printf("parsing section %d\n", ii);
    CodeSegment code_segment;
    ElfCodeSegmentInitialize(&code_segment, shdr, ii, ncf);
    valid_code_ranges->insert(
        std::make_pair(code_segment.vbase, code_segment.limit));
    DetectDataBlocksAndApplyPatterns(&code_segment);
  }

  printf("*** Validating control flow ***\n");
  for (int ii = 0; ii < ncf->shnum; ii++) {
    printf("section %d sh_addr %x offset %x flags %x\n",
           ii, (uint32_t)shdr[ii].sh_addr,
           (uint32_t)shdr[ii].sh_offset, (uint32_t)shdr[ii].sh_flags);
    if ((shdr[ii].sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR)
      continue;
    printf("parsing section %d\n", ii);
    CodeSegment code_segment;
    ElfCodeSegmentInitialize(&code_segment, shdr, ii, ncf);
    ValidateControlFlow(&code_segment);
  }
  return -badsections;
}

static void InitializeSegmentValidator() {
  if (unsafe_branch_map == NULL) {
    unsafe_branch_map = new std::map<uint32_t, ValidatorPattern*>();
    constant_pool_blocks = new std::set<uint32_t>();
    valid_code_ranges = new std::set<AddressRange>();
  }
  if (FLAGS_count_pattern_usage && NULL == pattern_check_map) {
    pattern_check_map = new std::map<ValidatorPattern*, uint32_t>();
    pattern_applied_map = new std::map<ValidatorPattern*, uint32_t>();
  }
}

// Analyze the code segments from an executable file.
void ValidateNcFile(ncfile *ncf, const char *fname) {
  InitializeSegmentValidator();
  if (ValidateSections(ncf) < 0) {
    fprintf(stderr, "%s: text validate failed\n", fname);
  }
}

void ValidateCodeSegment(CodeSegment* segment) {
  InitializeSegmentValidator();
  valid_code_ranges->insert(
      std::make_pair(segment->vbase, segment->limit));
  DetectDataBlocksAndApplyPatterns(segment);
  ValidateControlFlow(segment);
}
