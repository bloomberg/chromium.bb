/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Run the validator within the set up environment.
 */
#include <errno.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>

#include "native_client/src/trusted/validator_x86/ncval_driver.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_cpu_checks.h"
#include "native_client/src/trusted/validator_x86/nc_illegal.h"
#include "native_client/src/trusted/validator_x86/nc_jumps.h"
#include "native_client/src/trusted/validator_x86/nc_opcode_histogram.h"
#include "native_client/src/trusted/validator_x86/nc_protect_base.h"
#include "native_client/src/trusted/validator_x86/nc_store_protect.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

Bool NACL_FLAGS_print_timing = FALSE;

Bool NACL_FLAGS_opcode_histogram = FALSE;

int NACL_FLAGS_block_alignment = 32;

Bool NACL_FLAGS_quit_on_error = FALSE;

Bool NACL_FLAGS_warnings = FALSE;
Bool NACL_FLAGS_errors = FALSE;
Bool NACL_FLAGS_fatal = FALSE;

NaClOpKind nacl_base_register =
    (64 == NACL_TARGET_SUBARCH ? RegR15 : RegUnknown);

/* Generates NaClErrorMapping for error level suffix. */
#define NaClError(s) { #s , LOG_## s}

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
static int NaClGrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  char* error_level = NULL;
  if (argc == 0) return 0;
  new_argc = 1;
  /* TODO(karl) Allow command line option to set base register. */
  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (GrokBoolFlag("-histogram", arg, &NACL_FLAGS_opcode_histogram) ||
        GrokBoolFlag("-time", arg, &NACL_FLAGS_print_timing) ||
        GrokBoolFlag("-warnings", arg, &NACL_FLAGS_warnings) ||
        GrokBoolFlag("-errors", arg, &NACL_FLAGS_errors) ||
        GrokBoolFlag("-fatal", arg, &NACL_FLAGS_fatal) ||
        GrokIntFlag("-alignment", arg, &NACL_FLAGS_block_alignment) ||
        GrokBoolFlag("--quit-on-error", arg, &NACL_FLAGS_quit_on_error) ||
        GrokBoolFlag("--identity_mask", arg, &NACL_FLAGS_identity_mask)) {
      continue;
    } else if (GrokCstringFlag("-error_level", arg, &error_level)) {
      int i;
      static struct {
        const char* name;
        int level;
      } map[] = {
        NaClError(INFO),
        NaClError(WARNING),
        NaClError(ERROR),
        NaClError(FATAL)
      };
      for (i = 0; i < NACL_ARRAY_SIZE(map); ++i) {
        if (0 == strcmp(error_level, map[i].name)) {
          NaClLogSetVerbosity(map[i].level);
          break;
        }
      }
      if (i == NACL_ARRAY_SIZE(map)) {
        NaClValidatorMessage(LOG_FATAL, NULL,
                           "-error_level=%s not defined!\n", error_level);
      }
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

Bool NaClRunValidator(int argc, const char* argv[],
                      NaClRunValidatorData data,
                      NaClValidateLoad load,
                      NaClValidateAnalyze analyze) {
  clock_t clock_0;
  clock_t clock_l;
  clock_t clock_v;
  Bool return_value;

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

  NaClRegisterValidatorClear();

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
  return_value = load(argc, argv, data);
  if (!return_value) {
    NaClValidatorMessage(LOG_FATAL, NULL, "Unable to load code to validate");
  }
  clock_l = clock();
  return_value = analyze(data);
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

Bool NaClValidateNoLoad(int argc, const char* argv[],
                        NaClRunValidatorData data) {
  return TRUE;
}

typedef struct NaClValidateBytes {
  uint8_t* bytes;
  NaClMemorySize num_bytes;
  NaClPcAddress base;
} NaClValidateBytes;

static Bool NaClValidateAnalyzeBytes(NaClValidateBytes* data) {
  NaClValidatorState* state;
  Bool return_value = FALSE;
  state = NaClValidatorStateCreate(data->base,
                                   data->num_bytes,
                                   (uint8_t) NACL_FLAGS_block_alignment,
                                   nacl_base_register,
                                   NACL_FLAGS_quit_on_error,
                                   stdout);
  if (NULL == state) {
    NaClValidatorMessage(LOG_FATAL, NULL, "Unable to create validator state");
  }
  NaClValidateSegment(data->bytes, data->base, data->num_bytes, state);
  NaClValidatorStatePrintStats(stdout, state);
  return_value = NaClValidatesOk(state);
  NaClValidatorStateDestroy(state);
  return return_value;
}

Bool NaClRunValidatorBytes(int argc,
                           const char* argv[],
                           uint8_t* bytes,
                           NaClMemorySize num_bytes,
                           NaClPcAddress base) {
  NaClValidateBytes data;
  data.bytes = bytes;
  data.num_bytes = num_bytes;
  data.base = base;
  return NaClRunValidator(argc, argv, &data,
                          (NaClValidateLoad) NaClValidateNoLoad,
                          (NaClValidateAnalyze) NaClValidateAnalyzeBytes);
}
