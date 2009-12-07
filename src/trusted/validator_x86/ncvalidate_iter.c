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
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

FILE* NcValidatorStateLogFile(NcValidatorState* state) {
  return state->log_file;
}

#define MAX_FORMAT_SIZE 1024

static const char *LogLevelLabel(int level) {
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
static INLINE int RecordIfValidatorError(NcValidatorState* state, int level) {
  switch (level) {
    case LOG_ERROR:
      if (state != NULL && state->quit_after_first_error) level = LOG_FATAL;
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

void NcValidatorMessage(int level,
                        NcValidatorState* state,
                        const char* format,
                        ...) {
  level = RecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    va_list ap;

    NaClLogLock();
    NaClLog_mu(level, "%s", LogLevelLabel(level));
    va_start(ap, format);
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
  }
}

void NcValidatorVarargMessage(int level,
                              NcValidatorState* state,
                              const char* format,
                              va_list ap) {
  level = RecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    NaClLogLock();
    NaClLog_mu(level, "%s", LogLevelLabel(level));
    NaClLogV_mu(level, format, ap);
    NaClLogUnlock();
  }
}

void NcValidatorPcAddressMessage(int level,
                                 NcValidatorState* state,
                                 PcAddress addr,
                                 const char* format,
                                 ...) {
  level = RecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    va_list ap;

    NaClLogLock();
    NaClLog_mu(level, "At address %"PRIxPcAddress":\n", addr);
    NaClLogTagNext_mu();
    NaClLog_mu(level, "%s", LogLevelLabel(level));
    va_start(ap, format);
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
  }
}

void NcValidatorInstMessage(int level,
                            NcValidatorState* state,
                            NcInstState* inst,
                            const char* format,
                            ...) {
  level = RecordIfValidatorError(state, level);
  if (level <= NaClLogGetVerbosity()) {
    va_list ap;

    NaClLogLock();

    /* TODO(karl): empty fmt strings not allowed */
    NaClLog_mu(level, "%s", "");
    /* TODO(karl) - Make printing of instruction state possible via format. */
    PrintNcInstStateInstruction(NcValidatorStateLogFile(state), inst);

    va_start(ap, format);
    NaClLog_mu(level, "%s", LogLevelLabel(level));
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
  }
}

/* Holds the registered definition for a validator. */
typedef struct ValidatorDefinition {
  /* The validator function to apply. */
  NcValidator validator;
  /* The corresponding statistic print function associated with the validator
   * function (may be NULL).
   */
  NcValidatorPrintStats print_stats;
  /* The corresponding memory creation fuction associated with the validator
   * function (may be NULL).
   */
  NcValidatorMemoryCreate create_memory;
  /* The corresponding memory clean up function associated with the validator
   * function (may be NULL).
   */
  NcValidatorMemoryDestroy destroy_memory;
} ValidatorDefinition;

/* Defines the set of registerd validators. */
static ValidatorDefinition validators[MAX_NCVALIDATORS];

/* Defines the current number of registered validators. */
static int g_num_validators = 0;

void NcRegisterNcValidator(NcValidator validator,
                           NcValidatorPrintStats print_stats,
                           NcValidatorMemoryCreate create_memory,
                           NcValidatorMemoryDestroy destroy_memory) {
  ValidatorDefinition* defn;
  assert(NULL != validator);
  if (g_num_validators >= MAX_NCVALIDATORS) {
    NcValidatorMessage(
        LOG_FATAL, NULL,
        "Too many validators specified, can't register. Quitting!\n");
  }
  defn = &validators[g_num_validators++];
  defn->validator = validator;
  defn->print_stats = print_stats;
  defn->create_memory = create_memory;
  defn->destroy_memory = destroy_memory;
}

NcValidatorState* NcValidatorStateCreate(const PcAddress vbase,
                                         const MemorySize sz,
                                         const uint8_t alignment,
                                         const OperandKind base_register,
                                         Bool quit_after_first_error,
                                         FILE* log_file) {
  NcValidatorState* state;
  NcValidatorState* return_value = NULL;
  PcAddress vlimit = vbase + sz;
  DEBUG(printf("Validator Create: vbase = %"PRIxPcAddress
               ", sz = %"PRIxMemorySize", alignment = %u, vlimit = %"
               PRIxPcAddress"\n",
               vbase, sz, alignment, vlimit));
  if (vlimit <= vbase) return NULL;
  if (alignment != 16 && alignment != 32) return NULL;
  state = (NcValidatorState*) malloc(sizeof(NcValidatorState));
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
    state->number_validators = g_num_validators;
    for (i = 0; i < state->number_validators; ++i) {
      ValidatorDefinition* defn = &validators[i];
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
      NcValidatorStateDestroy(state);
    }
  }
  return return_value;
}

/* Given we are at the instruction defined by the instruction iterator, for
 * a segment, apply all applicable validator functions.
 */
static void ApplyValidators(NcValidatorState* state, NcInstIter* iter) {
  int i;
  DEBUG(PrintNcInstStateInstruction(stdout, NcInstIterGetState(iter)));
  for (i = 0; i < state->number_validators; ++i) {
    validators[i].validator(state, iter, state->local_memory[i]);
  }
}

/* The maximum lookback for the instruction iterator of the segment. */
static const size_t kLookbackSize = 4;

void NcValidateSegment(uint8_t* mbase, PcAddress vbase, MemorySize size,
                       NcValidatorState* state) {
  NcSegment segment;
  NcInstIter* iter;
  NcSegmentInitialize(mbase, vbase, size, &segment);
  for (iter = NcInstIterCreateWithLookback(&segment, kLookbackSize);
       NcInstIterHasNext(iter);
       NcInstIterAdvance(iter)) {
    ApplyValidators(state, iter);
  }
  NcInstIterDestroy(iter);
}

Bool NcValidatesOk(NcValidatorState* state) {
  return state->validates_ok;
}

void NcValidatorStatePrintStats(FILE* file, NcValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; i++) {
    ValidatorDefinition* defn = &validators[i];
    if (defn->print_stats != NULL) {
      defn->print_stats(file, state, state->local_memory[i]);
    }
  }
}

void NcValidatorStateDestroy(NcValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; ++i) {
    ValidatorDefinition* defn = &validators[i];
    void* defn_memory = state->local_memory[i];
    if (defn->destroy_memory != NULL && defn_memory != NULL) {
      defn->destroy_memory(state, defn_memory);
    }
  }
  NaClLogSetGio(state->old_log_stream);
  free(state);
}

void* NcGetValidatorLocalMemory(NcValidator validator,
                                const NcValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; ++i) {
    if (validators[i].validator == validator) {
      return state->local_memory[i];
    }
  }
  return NULL;
}
