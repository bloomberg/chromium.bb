/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncenuminsts.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_aux.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/nc_segment.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Extracts parsed instruction from instruction in API NaClInstStruct. */
#define NACL_INST(s) (&(s)->inst_buffer[(s)->cur_inst_index])

NaClInstStruct *NaClParseInst(uint8_t* ibytes, size_t isize,
                             const NaClPcAddress vbase) {
  /* WARNING: This version of the code uses a global to return the
   * decoded instruction, forcing the use to be in a single thread.
   * The following two (static) locals are used to hold the decoded
   * instruction until the next call to the function.
   */
  static NCDecoderInst dinst;
  static NCDecoderState dstate;

  /* Hand coded to only recognize a single instruction!. */
  NCDecoderStateConstruct(&dstate, ibytes, vbase, isize, &dinst, 1);
  NCDecoderStateNewSegment(&dstate);
  NCConsumeNextInstruction(&dinst);
  return &dstate;
}

uint8_t NaClInstLength(NaClInstStruct *inst) {
  return NACL_INST(inst)->inst.bytes.length;
}

char *NaClInstToStr(NaClInstStruct *inst) {
  return NCInstWithHexToString(NACL_INST(inst));
}

/* Defines a buffer size big enough to hold an instruction. */
#define MAX_INST_TEXT_SIZE 256

const char *NaClOpcodeName(NaClInstStruct *inst) {
  /* WARNING: This version of the code uses a global to return the
   * generated string, forcing the use to be in a single thread.
   */
  static const char* unknown_name = "???";
  static char buffer[MAX_INST_TEXT_SIZE];
  char* str;
  char* op;
  str = NCInstWithoutHexToString(NACL_INST(inst));
  if (str == NULL) return unknown_name;
  op = strtok(str, " \t\n");
  if (op == NULL) return unknown_name;
  /* Force op length to fit into buffer, and null terminate. */
  strncpy(buffer, op, MAX_INST_TEXT_SIZE);
  op[MAX_INST_TEXT_SIZE - 1] = '\0';
  free((void*) str);
  return buffer;
}

Bool NaClInstDecodesCorrectly(NaClInstStruct *inst) {
  NCDecoderInst* dinst = NACL_INST(inst);
  return (dinst->inst_addr < inst->size) &&
      (0 == inst->memory.overflow_count) &&
      (NACLi_UNDEFINED != dinst->opinfo->insttype) &&
      (NACLi_INVALID != dinst->opinfo->insttype);
}

Bool NaClInstValidates(uint8_t* mbase,
                       uint8_t size,
                       NaClPcAddress vbase,
                       NaClInstStruct* inst) {
  /* TODO(karl) Tighten this to chech more validation rules
   * that don't cross instruction boundaries.
   */
  return NaClInstDecodesCorrectly(inst) &&
      (NACLi_ILLEGAL != NACL_INST(inst)->opinfo->insttype);
}

Bool NaClSegmentValidates(uint8_t* mbase,
                          uint8_t size,
                          NaClPcAddress vbase) {
  NaClCPUFeaturesX86 cpu_features;
  NaClValidationStatus  status;

  /* check if NaCl thinks the given code segment is valid. */
  NaClSetAllCPUFeatures(&cpu_features);
  status = NaCl_ApplyValidator_x86_32(
      NACL_SB_DEFAULT, NaClApplyCodeValidation, vbase, mbase, size, 32,
      &cpu_features, NULL);
  switch (status) {
    case NaClValidationSucceeded:
      return TRUE;
    default:
      return FALSE;
  }
}
