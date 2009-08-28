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
 * Model code segments, and a set of routines to walk the code segment.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM__NCDECODE_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM__NCDECODE_H__

#include "native_client/src/trusted/validator_arm/arm_insts.h"

#include "native_client/src/trusted/validator_x86/ncfileutil.h"

/*
 * Defines the model of a code segment in an elf file.
 */
typedef struct CodeSegment {
  /* Pointer to the text section where instructions are stored. */
  uint8_t* mbase;
  /* Initial offset of the text section. */
  uint32_t vbase;
  /* The size of the text section. */
  size_t size;
  /* Maximum valid PC (plus one) of the code segment. */
  uint32_t limit;
} CodeSegment;

/*
 * Initializes a code segment with the given parameters.
 */
void CodeSegmentInitialize(
    CodeSegment* segment,
    uint8_t* mbase,
    uint32_t vbase,
    size_t size);

/*
 * Copy a code segment.
 */
void CodeSegmentCopy(const CodeSegment* from_segment, CodeSegment* to_segment);

/*
 * Initialize a code segment using the indexed section of the ELF header,
 * for the given native client file.
 */
void ElfCodeSegmentInitialize(
    CodeSegment* code_segment,
    Elf_Shdr* shdr, int section_index, ncfile* ncf);

/*
 * Test if the given pc is valid for the given code segment.
 */
bool IsInsideCodeSegment(uint32_t address, const CodeSegment* segment);

/*
 * Define an iterator that (1) iterates over the instructions in
 * a text segment; and (2) can jump to any displacement that corresponds
 * to a valid instruction (it is left to the user to prove the validity
 * of this second form).
 *
 * The typical iterator is of the form:
 *
 *    NcDecodeState state(code_segment);
 *    // Iterate over the sequence of instructions.
 *    for (state.GotoStartPc(); state.HasValidPc(); state.NextInstruction()) {
 *      // do something.
 *    }
 *    ...
 *    // Re-iterate over the sequence of instructions.
 *    for (state.GotoStartPc(); state.HasValidPc(); state.NextInstruction()) {
 *      // do something else
 *    }
 */
class NcDecodeState {
 public:
  /*
   * Construct a new NcDecodeState for code_segment, initially not pointing to
   * any instruction in particular.
   */
  explicit NcDecodeState(const CodeSegment &code_segment);

  /*
   * Moves the decode position to the first instruction in the text segment,
   * and (if possible) read in the corresponding instruction.
   */
  void GotoStartPc();

  /*
   * Checks if the decode position is valid.
   */
  bool HasValidPc();

  /*
   * Moves the decode position forward one instruction.
   */
  void NextInstruction();

  /*
   * Moves the decode position backward one instruction.
   */
  void PreviousInstruction();

  /*
   * Moves the decode position to the provided virtual address.
   */
  void GotoInstruction(uint32_t new_pc);

  /*
   * Runtime program counter of current instruction.
   */
  uint32_t CurrentPc() const;

  const CodeSegment &Segment() const;

  /* The decoded instruction at the current_pc. Note: Only defined if
   * HasValidPc returns true.
   */
  const NcDecodedInstruction &CurrentInstruction() const;

  /**
   * Shorthand method for checking if CurrentInstruction is of a certain
   * type.
   */
  bool CurrentInstructionIs(ArmInstType) const;

  /**
   * Shorthand method for checking if CurrentInstruction is of a certain
   * kind.
   */
  bool CurrentInstructionIs(ArmInstKind) const;

  /**
   * Verifies that the condition fields of this object and 'state' match.
   */
  bool ConditionMatches(const NcDecodeState &) const;

 private:
  void DecodeInstruction();
  void ConsumeInstruction();

  /* The code segment that is being decoded. */
  CodeSegment _code_segment;
  uint32_t _current_pc;
  NcDecodedInstruction _current_instruction;
  /* The number of bytes read */
  size_t _instruction_size;
};

/*
 * Attempts to decode the current instruction, using the instruction template
 * at inst_index in the ARM ISA definition.  On success, returns true, and
 * the decoded fields are set; on failure, returns false, and the decoded
 * fields are in an undefined state.
 */
bool NcDecodeMatch(NcDecodedInstruction *inst, int inst_index);

#endif  /* NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM__NCDECODE_H__ */
