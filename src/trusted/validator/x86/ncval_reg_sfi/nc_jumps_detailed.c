/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * nc_jumps_detailed.c - Validate jumps using a second pass of the
 * isntructions.
 */

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_jumps_detailed.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_jumps.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter_inl.c"

/* Returns true if the given address is within the code segment.
 */
static Bool NaClCheckAddressRange(NaClPcAddress address,
                                  NaClValidatorState* vstate) {
  if (address < vstate->vbase) {
    return FALSE;
  }
  if (address >= vstate->vbase + vstate->codesize) {
    return FALSE;
  }
  return TRUE;
}

static void NaClInstLayoutCheck(NaClValidatorState* vstate) {
  NaClPcAddress start;
  NaClPcAddress end;
  NaClPcAddress i;
  if (NULL == vstate->cur_inst_state) return;

  DEBUG(NaClLog(LOG_INFO, "Jump layout check: ");
        NaClInstStateInstPrint(NaClLogGetGio(), vstate->cur_inst_state));

  /* Check basic block boundaries. */
  start = vstate->cur_inst_state->vpc;

  /* Check that if first instruction in a basic block, it isn't in the
   * middle of a pattern.
   */
  if ((0 == (start & vstate->alignment_mask)) &&
      NaClAddressSetContains(vstate->jump_sets.removed_targets,
                             start, vstate)) {
    NaClValidatorInstMessage(
        LOG_ERROR, vstate, vstate->cur_inst_state,
        "Instruction begins basic block, but in middle of nacl pattern\n");
  }

  /* Check that instruction doesn't cross block boundaries. */
  end = (NaClPcAddress) (start + vstate->cur_inst_state->bytes.length);
  for (i = start + 1; i < end; ++i) {
    if (0 == (i & vstate->alignment_mask)) {
      NaClValidatorInstMessage(
          LOG_ERROR, vstate, vstate->cur_inst_state,
          "Instruction crosses basic block alignment\n");
    }
  }

  /* Check jump targets. */
  if (NaClHasBit(vstate->cur_inst_state->inst->flags,
                 NACL_IFLAG(JumpInstruction) | NACL_IFLAG(ConditionalJump))) {
    uint32_t i;
    NaClExpVector* vector = NaClInstStateExpVector(vstate->cur_inst_state);
    for (i = 0; i < vector->number_expr_nodes; ++i) {
      if (NaClHasBit(vector->node[i].flags, NACL_EFLAG(ExprJumpTarget))) {
        switch (vector->node[i].kind) {
          case ExprConstant:
          case ExprConstant64:
            {
              /* Explicit jump value. Check if legal! */
              NaClPcAddress target = end
                  + (NaClPcNumber) NaClGetExpConstant(vector, i);
              /* Don't report targets that are out of range. They should have
               * been reported in the first pass!
               */
              if (NaClCheckAddressRange(target, vstate)) {
                if (NaClAddressSetContains(vstate->jump_sets.possible_targets,
                                           target, vstate)) {
                  if (NaClAddressSetContains(vstate->jump_sets.removed_targets,
                                             target, vstate)) {
                    NaClValidatorInstMessage(
                        LOG_ERROR, vstate, vstate->cur_inst_state,
                        "Jumps into middle of nacl pattern\n");
                  }
                } else {
                  NaClValidatorInstMessage(
                      LOG_ERROR, vstate, vstate->cur_inst_state,
                      "Doesn't jump to instruction address\n");
                }
              }
            }
            break;
          default:
            break;
        }
      }
    }
  }
}

void NaClJumpValidatorSummarizeDetailed(NaClValidatorState* vstate) {
  if (vstate->quit) return;
  NaClValidatorMessage(
      LOG_INFO, vstate,
      "Checking block alignment and jump targets: %"NACL_PRIxNaClPcAddress
      " to %"NACL_PRIxNaClPcAddress"\n",
      vstate->vbase, vstate->vbase + vstate->codesize);

  /* Check that code segment starts at an aligned address. */
  if (vstate->vbase & vstate->alignment_mask) {
    NaClValidatorMessage(
        LOG_ERROR, vstate,
        "Code segment starts at 0x%"NACL_PRIxNaClPcAddress", "
        "which isn't aligned properly.\n",
        vstate->vbase);
  }

  /* Perform second pass on instructions to find instructions crossing
   * block boundaries, and (explicitly) branch to bad places.
   */
  do {
    if (!NaClValidatorStateIterReset(vstate)) {
      NaClValidatorMessage(LOG_ERROR, vstate,
                           "Not enough memory to check jumps\n");
      break;
    }
    for (; NaClValidatorStateIterHasNext(vstate);
         NaClValidatorStateIterAdvance(vstate)) {
      NaClInstLayoutCheck(vstate);
    }
    NaClValidatorStateIterFinish(vstate);
  } while (0);
}
