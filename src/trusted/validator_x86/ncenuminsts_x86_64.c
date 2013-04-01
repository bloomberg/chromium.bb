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
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_memory_protect.h"
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

Bool NaClInstValidates(uint8_t* mbase,
                       uint8_t size,
                       NaClPcAddress vbase,
                       NaClInstStruct* inst) {
  NaClSegment segment;
  NaClValidatorState* state;
  Bool validates = FALSE;
  NaClCPUFeaturesX86 cpu_features;

  NaClGetCurrentCPUFeaturesX86((NaClCPUFeatures *) &cpu_features);
  NACL_FLAGS_unsafe_single_inst_mode = TRUE;
  state = NaClValidatorStateCreate(vbase, (NaClMemorySize) size, RegR15, FALSE,
                                   &cpu_features);
  do {
    NaClSegmentInitialize(mbase, vbase, (NaClMemorySize) size, &segment);
    NaClBaseRegisterMemoryInitialize(state);
    state->cur_iter = NaClInstIterCreate(kNaClDecoderTables, &segment);
    if (NULL == state->cur_iter) break;
    state->cur_inst_state = NaClInstIterGetState(state->cur_iter);
    state->cur_inst = NaClInstStateInst(state->cur_inst_state);
    state->cur_inst_vector = NaClInstStateExpVector(state->cur_inst_state);
    NaClValidateInstructionLegal(state);
    NaClBaseRegisterValidator(state);
    /* induce call to NaClMaybeReportPreviousBad() */
    NaClBaseRegisterSummarize(state);
    NaClMemoryReferenceValidator(state);
    NaClJumpValidator(state);
    validates = NaClValidatesOk(state);
    NaClInstIterDestroy(state->cur_iter);
    state->cur_iter = NULL;
    state->cur_inst_state = NULL;
    state->cur_inst = NULL;
    state->cur_inst_vector = NULL;
  } while(0);
  NaClValidatorStateDestroy(state);
  /* Strictly speaking this shouldn't be necessary, as the mode */
  /* should only be used from tests. Disabling it here as a     */
  /* defensive tactic. */
  NACL_FLAGS_unsafe_single_inst_mode = FALSE;
  return validates;
}

Bool NaClSegmentValidates(uint8_t* mbase,
                          size_t size,
                          NaClPcAddress vbase) {
  NaClCPUFeaturesX86 cpu_features;
  NaClValidationStatus status;
  /* TODO(pasko): Validator initialization can be slow, make it run only once.
   */
  const struct NaClValidatorInterface *validator = NaClCreateValidator();

  /* check if NaCl thinks the given code segment is valid. */
  validator->SetAllCPUFeatures((NaClCPUFeatures *) &cpu_features);
  status = validator->Validate(
      vbase, mbase, size,
      /* stubout_mode= */ FALSE, /* readonly_text= */ FALSE,
      (NaClCPUFeatures *) &cpu_features, NULL, NULL);
  switch (status) {
    case NaClValidationSucceeded:
      return TRUE;
    default:
      return FALSE;
  }
}
