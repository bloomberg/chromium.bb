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


/* The routine that loads the code segment(s) into memory, returning
 * any data to be passed to the analysis step.
 */
typedef void* (*ValidateLoad)(int argc, const char* argv[]);

/* The actual validation analysis, applied to the data returned by
 * ValidateLoad. Assume that this function also deallocates any memory
 * in loaded_data.
 */
typedef void (*ValidateAnalyze)(void* loaded_data);

/* make output deterministic */
static Bool FLAGS_print_timing = FALSE;

/* Command line flag controlling whether an opcode histogram is
 * collected while validating.
 */
static Bool FLAGS_opcode_histogram = FALSE;

/* Define flags to set log verbosity. */
static Bool FLAGS_warnings = FALSE;
static Bool FLAGS_errors = FALSE;
static Bool FLAGS_fatal = FALSE;

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
static int GrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  if (argc == 0) return 0;
  new_argc = 1;
  /* TODO(karl) Allow command line option to set base register. */
  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (GrokBoolFlag("-histogram", arg, &FLAGS_opcode_histogram) ||
        GrokBoolFlag("-time", arg, &FLAGS_print_timing) ||
        GrokBoolFlag("-warnings", arg, &FLAGS_warnings) ||
        GrokBoolFlag("-errors", arg, &FLAGS_errors) ||
        GrokBoolFlag("-fatal", arg, &FLAGS_fatal)) {
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

int NcRunValidator(int argc, const char* argv[],
                   NcValidateLoad load,
                   NcValidateAnalyze analyze) {
  clock_t clock_0;
  clock_t clock_l;
  clock_t clock_v;
  void* loaded_data;
  int return_value;

  argc = GrokFlags(argc, argv);
  NaClLogModuleInit();

  if (FLAGS_warnings) {
    NaClLogSetVerbosity(LOG_WARNING);
  }
  if (FLAGS_errors) {
    NaClLogSetVerbosity(LOG_ERROR);
  }
  if (FLAGS_fatal) {
    NaClLogSetVerbosity(LOG_FATAL);
  }

  NcRegisterNcValidator(
      (NcValidator) NcJumpValidator,
      (NcValidatorPrintStats) NcJumpValidatorSummarize,
      (NcValidatorMemoryCreate) NcJumpValidatorCreate,
      (NcValidatorMemoryDestroy) NcJumpValidatorDestroy);

  NcRegisterNcValidator(
      (NcValidator) NcCpuCheck,
      (NcValidatorPrintStats) NcCpuCheckSummary,
      (NcValidatorMemoryCreate) NcCpuCheckMemoryCreate,
      (NcValidatorMemoryDestroy) NcCpuCheckMemoryDestroy);

  NcRegisterNcValidator(
      (NcValidator) NcValidateInstructionLegal,
      (NcValidatorPrintStats) NULL,
      (NcValidatorMemoryCreate) NULL,
      (NcValidatorMemoryDestroy) NULL);

  NcRegisterNcValidator(
      (NcValidator) NcBaseRegisterValidator,
      (NcValidatorPrintStats) NcBaseRegisterSummarize,
      (NcValidatorMemoryCreate) NcBaseRegisterMemoryCreate,
      (NcValidatorMemoryDestroy) NcBaseRegisterMemoryDestroy);

  NcRegisterNcValidator(
      (NcValidator) NcStoreValidator,
      (NcValidatorPrintStats) NULL,
      (NcValidatorMemoryCreate) NULL,
      (NcValidatorMemoryDestroy) NULL);

  if (FLAGS_opcode_histogram) {
    NcRegisterNcValidator(
        (NcValidator) NcOpcodeHistogramRecord,
        (NcValidatorPrintStats) NcOpcodeHistogramPrintStats,
        (NcValidatorMemoryCreate) NcOpcodeHistogramMemoryCreate,
        (NcValidatorMemoryDestroy) NcOpcodeHistogramMemoryDestroy);
  }

  clock_0 = clock();
  loaded_data = load(argc, argv);
  clock_l = clock();
  return_value = analyze(loaded_data);
  clock_v = clock();

  if (FLAGS_print_timing) {
    NcValidatorMessage(
        LOG_INFO, NULL,
        "load time: %0.6f  analysis time: %0.6f\n",
        (double)(clock_l - clock_0) /  (double)CLOCKS_PER_SEC,
        (double)(clock_v - clock_l) /  (double)CLOCKS_PER_SEC);
  }
  NaClLogModuleFini();
  return return_value;
}
