/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Validate Arm code meets Native Client Rules.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_VALIDATE_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_VALIDATE_H__

#include <string>

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
std::string InstructionLine(const NcDecodedInstruction* inst);

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
