/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Run the validator within the set up environment.
 */
#include <sys/timeb.h>
#include <time.h>

#include "native_client/src/trusted/validator_x86/ncval_driver.h"

#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_cpu_checks.h"
#include "native_client/src/trusted/validator_x86/nc_illegal.h"
#include "native_client/src/trusted/validator_x86/nc_jumps.h"
#include "native_client/src/trusted/validator_x86/nc_opcode_histogram.h"
#include "native_client/src/trusted/validator_x86/nc_protect_base.h"
#include "native_client/src/trusted/validator_x86/nc_store_protect.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"


/* make output deterministic */
static Bool NACL_FLAGS_print_timing = FALSE;

/* Command line flag controlling whether an opcode histogram is
 * collected while validating.
 */
static Bool NACL_FLAGS_opcode_histogram = FALSE;

/* Define flags to set log verbosity. */
static Bool NACL_FLAGS_warnings = FALSE;
static Bool NACL_FLAGS_errors = FALSE;
static Bool NACL_FLAGS_fatal = FALSE;

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
static int NaClGrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  if (argc == 0) return 0;
  new_argc = 1;
  /* TODO(karl) Allow command line option to set base register. */
  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (GrokBoolFlag("-histogram", arg, &NACL_FLAGS_opcode_histogram) ||
        GrokBoolFlag("-time", arg, &NACL_FLAGS_print_timing) ||
        GrokBoolFlag("-warnings", arg, &NACL_FLAGS_warnings) ||
        GrokBoolFlag("-errors", arg, &NACL_FLAGS_errors) ||
        GrokBoolFlag("-fatal", arg, &NACL_FLAGS_fatal)) {
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

int NaClRunValidator(int argc, const char* argv[],
                     NaClValidateLoad load,
                     NaClValidateAnalyze analyze) {
  clock_t clock_0;
  clock_t clock_l;
  clock_t clock_v;
  void* loaded_data;
  int return_value;

  argc = NaClGrokFlags(argc, argv);
  NaClLogModuleInit();

  if (NACL_FLAGS_warnings) {
    NaClLogSetVerbosity(LOG_WARNING);
  }
  if (NACL_FLAGS_errors) {
    NaClLogSetVerbosity(LOG_ERROR);
  }
  if (NACL_FLAGS_fatal) {
    NaClLogSetVerbosity(LOG_FATAL);
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

  clock_0 = clock();
  loaded_data = load(argc, argv);
  clock_l = clock();
  return_value = analyze(loaded_data);
  clock_v = clock();

  if (NACL_FLAGS_print_timing) {
    NaClValidatorMessage(
        LOG_INFO, NULL,
        "load time: %0.6f  analysis time: %0.6f\n",
        (double)(clock_l - clock_0) /  (double)CLOCKS_PER_SEC,
        (double)(clock_v - clock_l) /  (double)CLOCKS_PER_SEC);
  }
  NaClLogModuleFini();
  return return_value;
}
