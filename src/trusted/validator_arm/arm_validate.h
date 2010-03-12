/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Validate Arm code meets Native Client Rules.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_VALIDATE_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_VALIDATE_H__


#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_x86/ncfileutil.h"

class NcDecodedInstruction;

class CodeSegment;

// When true, don't allow direct branches to be outsize text segment(s).
extern Bool FLAGS_branch_in_text_segment;

// When true, don't allow direct branches to branch into the middle
// of a word.
extern Bool FLAGS_branch_word_boundary;

// When true, don't allow branches into a constant pool.
extern Bool FLAGS_branch_into_constant_pool;

// When true, report instructions that are disallowed.
extern Bool FLAGS_report_disallowed;

// Defines the block size assumed for unknown branching.
extern int FLAGS_code_block_size;

// Defines if we should count pattern usage.
extern Bool FLAGS_count_pattern_usage;

// Set the name of the validator executable to the passed name.
void SetValidateProgramName(const char* program_name);

// Returns the name of the validator executable.
const char* GetValidateProgramName();

// Generate a string describing the disassembled text of the given instruction.
nacl::string InstructionLine(const NcDecodedInstruction* inst);

// Report that a validation fatal error has occurred.
void ValidateFatal(const char* fmt, ...);

// Report that a validation (recoverable) error has occurred.
void ValidateError(const char* fmt, ...);

// Report a validation warning message.
void ValidateWarn(const char* fmt, ...);

// Returns exit code to use (for call to exit), based on
// reported ValidateError's.
int ValidateExitCode();

// Validate the code segments of the given elf file.
void ValidateNcFile(ncfile* ncf, const char* fname);

// Validate a single code segment.
void ValidateCodeSegment(CodeSegment* segment);

// Print out pattern usage counts.
void PrintPatternUsageCounts();

#endif  /* NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_VALIDATE_H__ */
