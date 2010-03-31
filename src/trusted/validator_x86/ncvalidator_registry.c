/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_x86/ncvalidator_registry.h"
#include "native_client/src/trusted/validator_x86/nc_jumps.h"
#include "native_client/src/trusted/validator_x86/nc_cpu_checks.h"
#include "native_client/src/trusted/validator_x86/nc_illegal.h"
#include "native_client/src/trusted/validator_x86/nc_jumps.h"
#include "native_client/src/trusted/validator_x86/nc_opcode_histogram.h"
#include "native_client/src/trusted/validator_x86/nc_protect_base.h"
#include "native_client/src/trusted/validator_x86/nc_store_protect.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

Bool NACL_FLAGS_opcode_histogram = FALSE;

Bool NACL_FLAGS_validator_trace = FALSE;

Bool NACL_FLAGS_validator_trace_verbose = FALSE;

static void NaClValidatorTrace(NaClValidatorState* state,
                               NaClInstIter* iter,
                               void* local_memory) {
  NaClInstState* inst_state = NaClInstIterGetState(iter);
  if (NACL_FLAGS_validator_trace_verbose) {
    printf("-> ");
  }
  printf("visit: ");
  NaClInstStateInstPrint(stdout, inst_state);
  if (NACL_FLAGS_validator_trace_verbose) {
    NaClInstPrint(stdout, NaClInstStateInst(inst_state));
    NaClExpVectorPrint(stdout, NaClInstStateExpVector(inst_state));
  }
}

static void NaClValidatorPostTrace(NaClValidatorState* state,
                                   NaClInstIter* iter,
                                   void* local_memory) {
  if (NACL_FLAGS_validator_trace_verbose) {
    printf("<- visit\n");
  }
}

void NaClValidatorInit() {

  NaClRegisterValidatorClear();

  if (NACL_FLAGS_validator_trace || NACL_FLAGS_validator_trace_verbose) {
    NaClRegisterValidator(
        (NaClValidator) NaClValidatorTrace,
        (NaClValidatorPostValidate) NaClValidatorPostTrace,
        (NaClValidatorPrintStats) NULL,
        (NaClValidatorMemoryCreate) NULL,
        (NaClValidatorMemoryDestroy) NULL);
  }

  NaClRegisterValidator(
      (NaClValidator) NaClJumpValidator,
      (NaClValidatorPostValidate) NULL,
      (NaClValidatorPrintStats) NaClJumpValidatorSummarize,
      (NaClValidatorMemoryCreate) NaClJumpValidatorCreate,
      (NaClValidatorMemoryDestroy) NaClJumpValidatorDestroy);

  NaClRegisterValidator(
      (NaClValidator) NaClCpuCheck,
      (NaClValidatorPostValidate) NULL,
      (NaClValidatorPrintStats) NaClCpuCheckSummary,
      (NaClValidatorMemoryCreate) NaClCpuCheckMemoryCreate,
      (NaClValidatorMemoryDestroy) NaClCpuCheckMemoryDestroy);

  NaClRegisterValidator(
      (NaClValidator) NaClValidateInstructionLegal,
      (NaClValidatorPostValidate) NULL,
      (NaClValidatorPrintStats) NULL,
      (NaClValidatorMemoryCreate) NULL,
      (NaClValidatorMemoryDestroy) NULL);

  NaClRegisterValidator(
      (NaClValidator) NaClBaseRegisterValidator,
      (NaClValidatorPostValidate) NaClBaseRegisterSummarize,
      (NaClValidatorPrintStats) NULL,
      (NaClValidatorMemoryCreate) NaClBaseRegisterMemoryCreate,
      (NaClValidatorMemoryDestroy) NaClBaseRegisterMemoryDestroy);

  NaClRegisterValidator(
      (NaClValidator) NaClStoreValidator,
      (NaClValidatorPostValidate) NULL,
      (NaClValidatorPrintStats) NULL,
      (NaClValidatorMemoryCreate) NULL,
      (NaClValidatorMemoryDestroy) NULL);

  if (NACL_FLAGS_opcode_histogram) {
    NaClRegisterValidator(
        (NaClValidator) NaClOpcodeHistogramRecord,
        (NaClValidatorPostValidate) NULL,
        (NaClValidatorPrintStats) NaClOpcodeHistogramPrintStats,
        (NaClValidatorMemoryCreate) NaClOpcodeHistogramMemoryCreate,
        (NaClValidatorMemoryDestroy) NaClOpcodeHistogramMemoryDestroy);
  }
}
