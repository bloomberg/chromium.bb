/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncvalidate_details.c
 * Attach detailed error reporter to the NaCl validator. Does a second
 * walk of the instructions to find instructions that explicitly branch
 * to illegal addresses.
 *
 * See function NCJumpSummarize in ncvalidate.c for a the terse version
 * which doesn't require a second pass.
 */

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_detailed.h"

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_internaltypes.h"

static void NCJumpSummarizeDetailed(struct NCValidatorState* vstate);


/* Null method for decoder state. */
static void NCNullDecoderStateMethod(struct NCValidatorState* vstate) {
}

/* Detailed (summary) error check on target value, defined in the given decoder
 * instruction.
 */
static void NCJumpCheck(struct NCValidatorState* vstate,
                        const NCDecoderInst* dinst,
                        NaClPcAddress target) {
  if (NCAddressInMemoryRange(target, vstate) &&
      !NCGetAdrTable(target - vstate->iadrbase, vstate->vttable)) {
    if (NCGetAdrTable(target - vstate->iadrbase,
                      vstate->pattern_nonfirst_insts_table)) {
      NCBadInstructionError(dinst, "Jumps into middle of nacl pattern");
    } else {
      NCBadInstructionError(dinst, "Doesn't jump to instruction address");
    }
    NCStatsBadTarget(vstate);
  }
}

/* Detailed (summary) error check for a byte jump instruction.
 * Note: This code should match the corresponding validator check
 * function ValidateJmp8 in ncvalidate.c.
 */
static void NCInstCheckJmp8(const NCDecoderInst* dinst) {
  int8_t offset = NCInstBytesByte(&dinst->inst_bytes,
                                  dinst->inst.prefixbytes+1);
  struct NCValidatorState* vstate = NCVALIDATOR_STATE_DOWNCAST(dinst->dstate);
  NaClPcAddress target = dinst->vpc + dinst->inst.bytes.length + offset;
  NCJumpCheck(vstate, dinst, target);
}

/* Detailed (summary) error check for a jump condition instruction.
 * Note: This code should match the corresponding validator check
 * function ValidateJmpz in ncvalidate.c.
 */
static  void NCInstCheckJmpz(const NCDecoderInst* dinst) {
  NCInstBytesPtr opcode;
  uint8_t opcode0;
  int32_t offset;
  NaClPcAddress target;
  NCValidatorState* vstate = NCVALIDATOR_STATE_DOWNCAST(dinst->dstate);
  NCInstBytesPtrInitInc(&opcode, &dinst->inst_bytes,
                        dinst->inst.prefixbytes);
  opcode0 = NCInstBytesByte(&opcode, 0);
  if (opcode0 == 0x0f) {
    /* Multbyte opcode. Intruction is of form:
     *    0F80 .. 0F8F: jCC $Jz
     */
    NCInstBytesPtr opcode_2;
    NCInstBytesPtrInitInc(&opcode_2, &opcode, 2);
    offset = NCInstBytesInt32(&opcode_2);
  } else {
    /* Single byte opcode. Must be one of:
     *    E8: call $Jz
     *    E9: jmp $Jx
     */
    NCInstBytesPtr opcode_1;
    NCInstBytesPtrInitInc(&opcode_1, &opcode, 1);
    offset = NCInstBytesInt32(&opcode_1);
  }
  target = dinst->vpc + dinst->inst.bytes.length + offset;
  NCJumpCheck(vstate, dinst, target);
}

/* Decoder action to perform to detect bad jumps during detailed
 * (summarization) error checking.
 */
static Bool NCInstLayoutCheck(const NCDecoderInst* dinst) {
  NCValidatorState* vstate;
  NaClPcAddress start;
  NaClPcAddress end;
  NaClPcAddress i;
  if (dinst == NULL) return TRUE;
  vstate = NCVALIDATOR_STATE_DOWNCAST(dinst->dstate);

  /* Check that if first instruction is a basic block, it isn't in the middle
   * of a pattern.
   */
  start = dinst->vpc;
  if ((0 == (start % vstate->alignment)) &&
      NCGetAdrTable(start - vstate->iadrbase,
                    vstate->pattern_nonfirst_insts_table)) {
    NCBadInstructionError(
        dinst,
        "Instruction begins basic block, but in middle of nacl pattern\n");
  }

  /* Check that instruction doesn't cross block boundaries. */
  end = (NaClPcAddress) (start + NCInstBytesLength(&dinst->inst_bytes));
  for (i = start + 1; i < end; ++i) {
    if (0 == (i % vstate->alignment)) {
      NCBadInstructionError(dinst, "Instruction crosses basic block alignment");
      NCStatsBadAlignment(vstate);
    }
  }

  /* Check jump targets. */
  switch (dinst->opinfo->insttype) {
    case NACLi_JMP8:
      NCInstCheckJmp8(dinst);
      break;
    case NACLi_JMPZ:
      NCInstCheckJmpz(dinst);
      break;
    default:
      break;
  }
  return TRUE;
}

/* Detailed (summary) error reporting. Rather than looking at summary
 * information collected during the first pass, this code rewalks the
 * instructions are reports each instruction that causes a problem.
 */
static void NCJumpSummarizeDetailed(struct NCValidatorState* vstate) {
  /* Rewalk the code to find instructions that break rules. */
  NCDecoderState* dstate = &vstate->dstate;
  NaClErrorReporter* reporter = dstate->error_reporter;
  NCDecoderStateConstruct(dstate, dstate->mbase, dstate->vbase, dstate->size,
                          vstate->inst_buffer, kNCValidatorInstBufferSize);
  dstate->action_fn = NCInstLayoutCheck;
  dstate->new_segment_fn = (NCDecoderStateMethod) NCNullDecoderStateMethod;
  dstate->internal_error_fn = (NCDecoderStateMethod) NCNullDecoderStateMethod;
  dstate->internal_error_fn = (NCDecoderStateMethod) NCStatsInternalError;
  NCDecoderStateSetErrorReporter(dstate, reporter);
  NCDecoderStateDecode(dstate);
}

struct NCValidatorState *NCValidateInitDetailed(const NaClPcAddress vbase,
                                                const NaClPcAddress vlimit,
                                                const uint8_t alignment) {
  struct NCValidatorState *vstate = NCValidateInit(vbase, vlimit, alignment);
  if (NULL != vstate) {
    vstate->summarize_fn = NCJumpSummarizeDetailed;
    vstate->pattern_nonfirst_insts_table =
        (uint8_t *)calloc(NCIATOffset(vlimit - vbase) + 1, 1);
    if (NULL == vstate->pattern_nonfirst_insts_table) {
      if (NULL != vstate->kttable) free(vstate->kttable);
      if (NULL != vstate->vttable) free(vstate->vttable);
      free(vstate);
      return NULL;
    }
  }
  return vstate;
}
