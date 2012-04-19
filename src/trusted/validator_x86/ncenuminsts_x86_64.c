/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncenuminsts.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/x86/decoder/ncop_exps.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/ncdis_decode_tables.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_illegal.h"
#include "native_client/src/trusted/validator/x86/nc_segment.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

NaClInstStruct *NaClParseInst(uint8_t* ibytes, size_t isize,
                             const NaClPcAddress vbase) {

  /* WARNING: This version of the code uses a global to return the
   * decoded instruction, forcing the use to be in a single thread.
   * The following two (static) locals are used to hold the decoded
   * instruction until the next call to the function.
   */
  static NaClInstIter* ND_iterator = NULL;
  static NaClSegment ND_segment;

  NaClSegmentInitialize(ibytes, vbase, isize, &ND_segment);
  if (ND_iterator != NULL) {
    NaClInstIterDestroy(ND_iterator);
  }
  ND_iterator = NaClInstIterCreate(kNaClDecoderTables, &ND_segment);
  return NaClInstIterGetState(ND_iterator);
}

uint8_t NaClInstLength(NaClInstStruct *inst) {
  return NaClInstStateLength(inst);
}

char *NaClInstToStr(NaClInstStruct *inst) {
  return NaClInstStateInstructionToString(inst);
}

const char *NaClOpcodeName(NaClInstStruct *inst) {
  const struct NaClInst *nacl_opcode = NaClInstStateInst(inst);
  return NaClMnemonicName(nacl_opcode->name);
}

Bool NaClInstDecodesCorrectly(NaClInstStruct *inst) {
  return NaClInstStateIsValid(inst);
}

/* Checks that if a memory reference is a segment address, the segment is
 * one of the ignored segment registers.
 *
 * Note: This is a simplified validator test from NaClMemoryReferenceValidator
 * in file nc_memory_protect.c, pulling out the check on ignored segment
 * registers.
 */
static void NaClSafeSegmentReference(NaClValidatorState* state,
                                     NaClInstIter* iter,
                                     void* ignore) {
  uint32_t i;
  NaClInstState* inst_state = state->cur_inst_state;
  NaClExpVector* vector = state->cur_inst_vector;

  DEBUG({
      struct Gio* g = NaClLogGetGio();
      NaClLog(LOG_INFO, "Validating segments:\n");
      NaClInstStateInstPrint(g, inst_state);
      NaClInstPrint(g, state->decoder_tables, state->cur_inst);
      NaClExpVectorPrint(g, inst_state);
    });
  /* Look for references to a segment address. */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    NaClExp* node = &vector->node[i];
    if (state->quit) break;
    DEBUG(NaClLog(LOG_INFO, "processing argument %"NACL_PRIu32"\n", i));
    if (ExprSegmentAddress == node->kind) {
        int seg_prefix_reg_index;
        NaClOpKind seg_prefix_reg;
        DEBUG(NaClLog(LOG_INFO,
                      "found segment assign at node %"NACL_PRIu32"\n", i));

        /* Only allow if 64 bit segment addresses. */
        if (NACL_EMPTY_EFLAGS == (node->flags & NACL_EFLAG(ExprSize64))) {
          NaClValidatorInstMessage(
              LOG_ERROR, state, inst_state,
              "Assignment to non-64 bit segment address\n");
          continue;
        }
        /* Only allow segment prefix registers that are treated as
         * null prefixes.
         */
        seg_prefix_reg_index = NaClGetExpKidIndex(vector, i, 0);
        seg_prefix_reg = NaClGetExpVectorRegister(vector, seg_prefix_reg_index);
        switch (seg_prefix_reg) {
          case RegCS:
          case RegDS:
          case RegES:
          case RegSS:
            continue;
          default:
            break;
        }
        /* If reached, we don't know how to handle the segment reference. */
        NaClValidatorInstMessage(LOG_ERROR, state, inst_state,
                                 "Segment memory reference not allowed\n");
    }
  }
}

Bool NaClInstValidates(uint8_t* mbase,
                       uint8_t size,
                       NaClPcAddress vbase,
                       NaClInstStruct* inst) {
  NaClSegment segment;
  NaClInstIter* iter;
  NaClValidatorState* state;
  Bool validates = FALSE;
  NaClCPUFeaturesX86 cpu_features;

  NaClGetCurrentCPUFeatures(&cpu_features);
  state = NaClValidatorStateCreate(vbase, (NaClMemorySize) size, RegR15, FALSE,
                                   &cpu_features);
  do {
    NaClSegmentInitialize(mbase, vbase, (NaClMemorySize) size, &segment);
    iter = NaClInstIterCreate(kNaClDecoderTables, &segment);
    if (NULL == iter) break;
    state->cur_inst_state = NaClInstIterGetState(iter);
    state->cur_inst = NaClInstStateInst(state->cur_inst_state);
    state->cur_inst_vector = NaClInstStateExpVector(state->cur_inst_state);
    NaClValidateInstructionLegal(state);
    NaClSafeSegmentReference(state, iter, NULL);
    validates = NaClValidatesOk(state);
    state->cur_inst_state = NULL;
    state->cur_inst = NULL;
    state->cur_inst_vector = NULL;
    NaClInstIterDestroy(iter);
  } while(0);
  NaClValidatorStateDestroy(state);
  return validates;
}

Bool NaClSegmentValidates(uint8_t* mbase,
                          uint8_t size,
                          NaClPcAddress vbase) {
  NaClCPUFeaturesX86 cpu_features;
  NaClValidationStatus status;

  /* check if NaCl thinks the given code segment is valid. */
  NaClSetAllCPUFeatures(&cpu_features);
  status = NaCl_ApplyValidator_x86_64(
      NACL_SB_DEFAULT, vbase, mbase, size,
      /* stubout_mode= */ FALSE, /* readonly_text= */ FALSE, &cpu_features,
      NULL);
  switch (status) {
    case NaClValidationSucceeded:
      return TRUE;
    default:
      return FALSE;
  }
}
