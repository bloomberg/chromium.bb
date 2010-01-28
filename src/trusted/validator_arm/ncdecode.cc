/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Model code segments, and a set of routines to walk the code segment.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "native_client/src/trusted/validator_arm/generated_decoder.h"
#include "native_client/src/trusted/validator_arm/ncdecode.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/shared/utils/formatting.h"

#define DEBUGGING 0
#include "native_client/src/trusted/validator_arm/debugging.h"

/*
 * ********************************************************
 * This section defines the code to iterate through a text
 * section and process the instructions.
 * ********************************************************
 */

void CodeSegmentInitialize(CodeSegment* code_segment,
                           uint8_t* mbase, uint32_t vbase, size_t size) {
  code_segment->mbase = mbase;
  code_segment->vbase = vbase;
  code_segment->size = size;
  code_segment->limit = vbase + size;
}

void CodeSegmentCopy(const CodeSegment* from_segment, CodeSegment* to_segment) {
  to_segment->mbase = from_segment->mbase;
  to_segment->vbase = from_segment->vbase;
  to_segment->size = from_segment->size;
  to_segment->limit = from_segment->limit;
}

void ElfCodeSegmentInitialize(
    CodeSegment* code_segment,
    Elf_Shdr* shdr, int section_index, ncfile* ncf) {
  CodeSegmentInitialize(code_segment,
                        ncf->data + (shdr[section_index].sh_addr - ncf->vbase),
                        shdr[section_index].sh_addr,
                        shdr[section_index].sh_size);
}

bool IsInsideCodeSegment(uint32_t address, const CodeSegment* segment) {
  return address >= segment->vbase && address < segment->limit;
}

/*
 * Initialize the decode state (before decoding)
 */
static void InitDecoder(NcDecodedInstruction *mstate) {
  mstate->matched_inst = &kUndefinedArmOp;
  DecodeValues(mstate->inst, mstate->matched_inst->inst_type,
               &(mstate->values));
}

/* The following macro is used to define code that matches a given
 * field in an NcDecodedInstruction. A macro is used, so that the
 * corresponding arguments to ValuesDefaultMatch can be automatically
 * generated.
 */
#define VALUES_DEFAULT_MATCH(field) \
  ValuesDefaultMatch(inst->values.field, \
                     op->expected_values.field, \
                     masks->field >> PosOfLowestBitSet(masks->field))

bool NcDecodeMatch(NcDecodedInstruction *inst, int inst_index) {
  const OpInfo* op = arm_instruction_set.instructions[inst_index];
  const InstValues* masks = GetArmInstMasks(op->inst_type);
  DecodeValues(inst->inst, op->inst_type, &(inst->values));
  inst->matched_inst = op;
  return VALUES_DEFAULT_MATCH(cond) &&
      VALUES_DEFAULT_MATCH(prefix) &&
      VALUES_DEFAULT_MATCH(opcode) &&
      VALUES_DEFAULT_MATCH(arg1) &&
      VALUES_DEFAULT_MATCH(arg2) &&
      VALUES_DEFAULT_MATCH(arg3) &&
      VALUES_DEFAULT_MATCH(arg4) &&
      VALUES_DEFAULT_MATCH(shift) &&
      VALUES_DEFAULT_MATCH(immediate) &&
      /* If specified, apply other checks*/
      (NULL == op->check_fcn || op->check_fcn(inst));
}

/*
 * Read the next instruction by getting the next 4 bytes.
 * Note: Don't assume the instruction is word aligned (so
 * that we can recognize thumb instructions).
 */
void NcDecodeState::ConsumeInstruction() {
  int i;
  uint32_t offset = 0;
  int max_bytes;
  uint8_t* nextbyte = _code_segment.mbase +
                      (_current_pc - _code_segment.vbase);
  _current_instruction.inst = 0;
  _current_instruction.vpc = _current_pc;
  /* Note: this code checks that we don't overflow the end of the text section.
   * This code can be simplified if we assume that the text section must be
   * complete words.
   */
  max_bytes = _code_segment.limit - _current_pc;
  if (max_bytes > ARM_WORD_LENGTH) max_bytes = ARM_WORD_LENGTH;
  /* Read the next word in the text section. */
  for (i = 0; i < max_bytes; ++i) {
    _current_instruction.inst |= ((*(nextbyte++)) << offset);
    offset += 8;
  }
  /* Deal with the case where the text section is not word aligned, and
   * hence, fewer than four bytes were read. In this case, shift the
   * data so that the instruction is word aligned.
   */
  if (i < ARM_WORD_LENGTH) {
    _current_instruction.inst <<= 8 * (ARM_WORD_LENGTH - i);
  }
  _instruction_size = max_bytes;
}

void NcDecodeState::DecodeInstruction() {
  if (HasValidPc()) {
    ConsumeInstruction();
#ifdef USE_DEFAULT_PARSER
    int i;
    InitDecoder(_current_instruction);
    for (i = 0; i < arm_instruction_set.size; ++i) {
      if (NcDecodeMatch(i, _current_instruction)) {
        DEBUG(PrintIndexedInstruction(i));
        return;
      }
    }
    /* No instruction matched, assume undefined. */
    DecodeValues(_current_instruction.inst,
        _current_instruction.matched_inst->inst_type,
        &(_current_instruction.values));
    _current_instruction.matched_inst = &kUndefinedArmOp;
#else
    DecodeNcDecodedInstruction(&_current_instruction);
#endif
  } else {
    // Move to an undefined instruction, to be safe.
    _current_instruction.inst = 0;
    _current_instruction.vpc = _current_pc;
    _instruction_size = ARM_WORD_LENGTH;
    InitDecoder(&_current_instruction);
  }
}

/*
 * ********************************************************
 * This section defines helper routines for NcDecodedInstruction
 * ********************************************************
 */

NcDecodeState::NcDecodeState(const CodeSegment &code_segment) {
  DEBUG(fprintf(stdout,
                "state = 0x%x, mbase = 0x%x, vbase = %u, size = %d\n",
                state, code_segment->mbase,
                code_segment->vbase,
                code_segment->size));

  CodeSegmentCopy(&code_segment, &_code_segment);
  /* put pc initially out of bounds. */
  _current_pc = _code_segment.limit;
}

void NcDecodeState::GotoInstruction(uint32_t new_pc) {
  if (_current_pc != new_pc) {
    _current_pc = new_pc;
    DecodeInstruction();
  }
}

void NcDecodeState::GotoStartPc() {
  GotoInstruction(_code_segment.vbase);
}

void NcDecodeState::NextInstruction() {
  GotoInstruction(_current_pc + ARM_WORD_LENGTH);
}

void NcDecodeState::PreviousInstruction() {
  GotoInstruction(_current_pc - ARM_WORD_LENGTH);
}

bool NcDecodeState::HasValidPc() {
  return IsInsideCodeSegment(_current_pc, &_code_segment);
}

uint32_t NcDecodeState::CurrentPc() const {
  return _current_pc;
}

const NcDecodedInstruction &NcDecodeState::CurrentInstruction() const {
  return _current_instruction;
}

bool NcDecodeState::CurrentInstructionIs(ArmInstType type) const {
  return _current_instruction.matched_inst->inst_type == type;
}

bool NcDecodeState::CurrentInstructionIs(ArmInstKind kind) const {
  return _current_instruction.matched_inst->inst_kind == kind;
}

bool NcDecodeState::ConditionMatches(const NcDecodeState &other) const {
  return _current_instruction.values.cond
      == other._current_instruction.values.cond;
}

const CodeSegment &NcDecodeState::Segment() const {
  return _code_segment;
}
