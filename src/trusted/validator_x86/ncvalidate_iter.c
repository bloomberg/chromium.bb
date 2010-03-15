/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncvalidate_iter.c
 * Validate x86 instructions for Native Client
 *
 */

#include <assert.h>
#include <string.h>

#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator_x86/ncvalidator_registry.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

FILE* NaClValidatorStateLogFile(NaClValidatorState* state) {
  return state->log_file;
}

static const char *NaClLogLevelLabel(int level) {
  switch (level) {
    case LOG_WARNING:
      return "WARNING: ";
    case LOG_ERROR:
      return "ERROR: ";
    case LOG_FATAL:
      return "FATAL: ";
    default:
      return "";
  }
}

/* Records the number of error validator messages generated for the state.
 * Parameters:
 *   state - The validator state (may be NULL).
 *   level - The log level of the validator message.
 * Returns - Updated error level, based on state.
 */
static INLINE int NaClRecordIfValidatorError(NaClValidatorState* state,
                                             int level) {
  switch (level) {
    case LOG_ERROR:
      /* Intentionally fall to next case, since if an error occurs, it doesn't
       * validate as ok.
       */
    case LOG_FATAL:
      if (state != NULL) state->validates_ok = FALSE;
      break;
    default:
      break;
  }
  return level;
}

void NaClValidatorMessage(int level,
                          NaClValidatorState* state,
                          const char* format,
                          ...) {
  level = NaClRecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    va_list ap;

    NaClLogLock();
    NaClLog_mu(level, "%s", NaClLogLevelLabel(level));
    va_start(ap, format);
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
  }
}

void NaClValidatorVarargMessage(int level,
                                NaClValidatorState* state,
                                const char* format,
                                va_list ap) {
  level = NaClRecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    NaClLogLock();
    NaClLog_mu(level, "%s", NaClLogLevelLabel(level));
    NaClLogV_mu(level, format, ap);
    NaClLogUnlock();
  }
}

void NaClValidatorPcAddressMessage(int level,
                                   NaClValidatorState* state,
                                   NaClPcAddress addr,
                                   const char* format,
                                   ...) {
  level = NaClRecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    va_list ap;

    NaClLogLock();
    NaClLog_mu(level, "At address %"NACL_PRIxNaClPcAddress":\n", addr);
    NaClLogTagNext_mu();
    NaClLog_mu(level, "%s", NaClLogLevelLabel(level));
    va_start(ap, format);
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
  }
}

void NaClValidatorInstMessage(int level,
                              NaClValidatorState* state,
                              NaClInstState* inst,
                              const char* format,
                              ...) {
  level = NaClRecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    va_list ap;

    NaClLogLock();

    /* TODO(karl): empty fmt strings not allowed */
    NaClLog_mu(level, "%s", "");
    /* TODO(karl) - Make printing of instruction state possible via format. */
    NaClInstStateInstPrint(NaClValidatorStateLogFile(state), inst);

    va_start(ap, format);
    NaClLog_mu(level, "%s", NaClLogLevelLabel(level));
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
  }
}

static INLINE Bool NaClValidatorQuit(NaClValidatorState* state) {
  return state->quit_after_first_error && !state->validates_ok;
}

/* Holds the registered definition for a validator. */
typedef struct NaClValidatorDefinition {
  /* The validator function to apply. */
  NaClValidator validator;
  /* The post iterator validator function to apply, after iterating
   * through all instructions in a segment. If non-null, called to
   * do corresponding post processing.
   */
  NaClValidatorPostValidate post_validate;
  /* The corresponding statistic print function associated with the validator
   * function (may be NULL).
   */
  NaClValidatorPrintStats print_stats;
  /* The corresponding memory creation fuction associated with the validator
   * function (may be NULL).
   */
  NaClValidatorMemoryCreate create_memory;
  /* The corresponding memory clean up function associated with the validator
   * function (may be NULL).
   */
  NaClValidatorMemoryDestroy destroy_memory;
} NaClValidatorDefinition;

/* Defines the set of registerd validators. */
static NaClValidatorDefinition validators[NACL_MAX_NCVALIDATORS];

/* Defines the current number of registered validators. */
static int nacl_g_num_validators = 0;

void NaClRegisterValidatorClear() {
  nacl_g_num_validators = 0;
}

void NaClRegisterValidator(NaClValidator validator,
                           NaClValidatorPostValidate post_validate,
                           NaClValidatorPrintStats print_stats,
                           NaClValidatorMemoryCreate create_memory,
                           NaClValidatorMemoryDestroy destroy_memory) {
  NaClValidatorDefinition* defn;
  assert(NULL != validator);
  if (nacl_g_num_validators >= NACL_MAX_NCVALIDATORS) {
    NaClValidatorMessage(
        LOG_FATAL, NULL,
        "Too many validators specified, can't register. Quitting!\n");
  }
  defn = &validators[nacl_g_num_validators++];
  defn->validator = validator;
  defn->post_validate = post_validate;
  defn->print_stats = print_stats;
  defn->create_memory = create_memory;
  defn->destroy_memory = destroy_memory;
}

NaClValidatorState* NaClValidatorStateCreate(const NaClPcAddress vbase,
                                             const NaClMemorySize sz,
                                             const uint8_t alignment,
                                             const NaClOpKind base_register,
                                             Bool quit_after_first_error,
                                             FILE* log_file) {
  NaClValidatorState* state;
  NaClValidatorState* return_value = NULL;
  NaClPcAddress vlimit = vbase + sz;
  NaClValidatorInit();
  DEBUG(printf("Validator Create: vbase = %"NACL_PRIxNaClPcAddress", "
               "sz = %"NACL_PRIxNaClMemorySize", alignment = %u, vlimit = %"
               NACL_PRIxNaClPcAddress"\n",
               vbase, sz, alignment, vlimit));
  if (vlimit <= vbase) return NULL;
  if (alignment != 16 && alignment != 32) return NULL;
  state = (NaClValidatorState*) malloc(sizeof(NaClValidatorState));
  if (state != NULL) {
    int i;
    return_value = state;
    state->log_file = log_file;
    state->old_log_stream = NaClLogGetGio();
    GioFileRefCtor(&state->log_stream, log_file);
    NaClLogSetGio((struct Gio*) &state->log_stream);
    state->vbase = vbase;
    state->sz = sz;
    state->alignment = alignment;
    state->vlimit = vlimit;
    state->alignment_mask = alignment - 1;
    GetCPUFeatures(&(state->cpu_features));
    state->base_register = base_register;
    state->validates_ok = TRUE;
    state->quit_after_first_error = quit_after_first_error;
    state->number_validators = nacl_g_num_validators;
    for (i = 0; i < state->number_validators; ++i) {
      NaClValidatorDefinition* defn = &validators[i];
      if (defn->create_memory == NULL) {
        state->local_memory[i] = NULL;
      } else {
        void* defn_memory = defn->create_memory(state);
        if (defn_memory == NULL) {
          return_value = NULL;
          state->local_memory[i] = NULL;
        } else {
          state->local_memory[i] = defn_memory;
        }
      }
    }
    if (return_value == NULL) {
      NaClValidatorStateDestroy(state);
    }
  }
  return return_value;
}

/* Given we are at the instruction defined by the instruction iterator, for
 * a segment, apply all applicable validator functions.
 */
static void NaClApplyValidators(NaClValidatorState* state, NaClInstIter* iter) {
  int i;
  DEBUG(NaClInstStateInstPrint(stdout, NaClInstIterGetState(iter)));
  if (NaClValidatorQuit(state)) return;
  for (i = 0; i < state->number_validators; ++i) {
    validators[i].validator(state, iter, state->local_memory[i]);
    if (NaClValidatorQuit(state)) return;
  }
}

/* Given that we have just iterated through all instructions in a segment,
 * apply post validators rules (before we collect the iterator).
 */
static void NaClApplyPostValidators(NaClValidatorState* state,
                                    NaClInstIter* iter) {
  int i;
  DEBUG(printf("applying post validators...\n"));
  if (NaClValidatorQuit(state)) return;
  for (i = 0; i < state->number_validators; ++i) {
    if (NULL != validators[i].post_validate) {
      validators[i].post_validate(state, iter, state->local_memory[i]);
      if (NaClValidatorQuit(state)) return;
    }
  }
}

/* The maximum lookback for the instruction iterator of the segment. */
static const size_t kLookbackSize = 4;

void NaClValidateSegment(uint8_t* mbase, NaClPcAddress vbase,
                         NaClMemorySize size, NaClValidatorState* state) {
  NaClSegment segment;
  NaClInstIter* iter;
  NaClSegmentInitialize(mbase, vbase, size, &segment);
  for (iter = NaClInstIterCreateWithLookback(&segment, kLookbackSize);
       NaClInstIterHasNext(iter);
       NaClInstIterAdvance(iter)) {
    NaClApplyValidators(state, iter);
    if (NaClValidatorQuit(state)) break;
  }
  NaClApplyPostValidators(state, iter);
  NaClInstIterDestroy(iter);
}

Bool NaClValidatesOk(NaClValidatorState* state) {
  return state->validates_ok;
}

void NaClValidatorStatePrintStats(FILE* file, NaClValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; i++) {
    NaClValidatorDefinition* defn = &validators[i];
    if (defn->print_stats != NULL) {
      defn->print_stats(file, state, state->local_memory[i]);
    }
  }
}

void NaClValidatorStateDestroy(NaClValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; ++i) {
    NaClValidatorDefinition* defn = &validators[i];
    void* defn_memory = state->local_memory[i];
    if (defn->destroy_memory != NULL && defn_memory != NULL) {
      defn->destroy_memory(state, defn_memory);
    }
  }
  NaClLogSetGio(state->old_log_stream);
  free(state);
}

void* NaClGetValidatorLocalMemory(NaClValidator validator,
                                  const NaClValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; ++i) {
    if (validators[i].validator == validator) {
      return state->local_memory[i];
    }
  }
  return NULL;
}
