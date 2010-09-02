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

/* When >= 0, only print that many errors before quiting. When
 * < 0, print all errors.
 */
int NACL_FLAGS_max_reported_errors = 100;

Bool NACL_FLAGS_validator_trace_instructions = FALSE;

Bool NACL_FLAGS_validator_trace_inst_internals = FALSE;

void NaClValidatorFlagsSetTraceVerbose() {
  NACL_FLAGS_validator_trace_instructions = TRUE;
  NACL_FLAGS_validator_trace_inst_internals = TRUE;
}

int NaClValidatorStateGetMaxReportedErrors(NaClValidatorState* state) {
  return state->quit_after_error_count;
}

void NaClValidatorStateSetMaxReportedErrors(NaClValidatorState* state,
                                            int new_value) {
  state->quit_after_error_count = new_value;
  state->quit = NaClValidatorQuit(state);
}

Bool NaClValidatorStateGetPrintOpcodeHistogram(NaClValidatorState* state) {
  return state->print_opcode_histogram;
}

void NaClValidatorStateSetPrintOpcodeHistogram(NaClValidatorState* state,
                                               Bool new_value) {
  state->print_opcode_histogram = new_value;
}

Bool NaClValidatorStateGetTraceInstructions(NaClValidatorState* state) {
  return state->trace_instructions;
}

void NaClValidatorStateSetTraceInstructions(NaClValidatorState* state,
                                            Bool new_value) {
  state->trace_instructions = new_value;
}

Bool NaClValidatorStateGetTraceInstInternals(NaClValidatorState* state) {
  return state->trace_inst_internals;
}

void NaClValidatorStateSetTraceInstInternals(NaClValidatorState* state,
                                             Bool new_value) {
  state->trace_inst_internals = new_value;
}

Bool NaClValidatorStateTrace(NaClValidatorState* state) {
  return state->trace_instructions || state->trace_inst_internals;
}

void NaClValidatorStateSetTraceVerbose(NaClValidatorState* state) {
  state->trace_instructions = TRUE;
  state->trace_inst_internals = TRUE;
}

int NaClValidatorStateGetLogVerbosity(NaClValidatorState* state) {
  return state->log_verbosity;
}

void NaClValidatorStateSetLogVerbosity(NaClValidatorState* state,
                                       Bool new_value) {
  state->log_verbosity = new_value;
}


/* Returns true if an error message should be printed for the given level, in
 * the current validator state.
 * Parameters:
 *   state - The validator state (may be NULL).
 *   level - The log level of the validator message.
 */
static Bool NaClPrintValidatorMessages(NaClValidatorState* state, int level) {
  if (NULL == state) {
    /* Validator not defined yet, only used log verbosity to decide if message
     * should be printed.
     */
    return level <= NaClLogGetVerbosity();
  } else {
    return (state->quit_after_error_count != 0) &&
        (level <= state->log_verbosity) &&
        (level <= NaClLogGetVerbosity());
  }
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

/* Records that an error message has just been reported.
 * Parameters:
 *   state - The validator state (may be NULL).
 *   level - The log level of the validator message.
 */
static void NaClRecordErrorReported(NaClValidatorState* state, int level) {
  if ((state != NULL) && ((level == LOG_ERROR) || (level == LOG_FATAL)) &&
      (state->quit_after_error_count > 0)) {
    --(state->quit_after_error_count);
    state->quit = NaClValidatorQuit(state);
    if (state->quit_after_error_count == 0) {
      NaClLog(LOG_INFO,
              "%sError limit reached. Validator quitting!\n",
              NaClLogLevelLabel(LOG_INFO));
    }
  }
}

/* Records the number of error validator messages generated for the state.
 * Parameters:
 *   state - The validator state (may be NULL).
 *   level - The log level of the validator message.
 * Returns - Updated error level, based on state.
 */
static int NaClRecordIfValidatorError(NaClValidatorState* state,
                                             int level) {
  if (((level == LOG_ERROR) || (level == LOG_FATAL)) &&
      (NULL != state)) {
    state->validates_ok = FALSE;
    state->quit = NaClValidatorQuit(state);
  }
  return level;
}

void NaClValidatorMessage(int level,
                          NaClValidatorState* state,
                          const char* format,
                          ...) {
  level = NaClRecordIfValidatorError(state, level);
  if (NaClPrintValidatorMessages(state, level)) {
    va_list ap;
    NaClLogLock();
    NaClLog_mu(level, "VALIDATOR: %s", NaClLogLevelLabel(level));
    va_start(ap, format);
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
    NaClRecordErrorReported(state, level);
  }
}

void NaClValidatorVarargMessage(int level,
                                NaClValidatorState* state,
                                const char* format,
                                va_list ap) {
  level = NaClRecordIfValidatorError(state, level);
  if (NaClPrintValidatorMessages(state, level)) {
    NaClLogLock();
    NaClLog_mu(level, "VALIDATOR: %s", NaClLogLevelLabel(level));
    NaClLogV_mu(level, format, ap);
    NaClLogUnlock();
    NaClRecordErrorReported(state, level);
  }
}

void NaClValidatorPcAddressMessage(int level,
                                   NaClValidatorState* state,
                                   NaClPcAddress addr,
                                   const char* format,
                                   ...) {
  level = NaClRecordIfValidatorError(state, level);
  if (NaClPrintValidatorMessages(state, level)) {
    va_list ap;

    NaClLogLock();
    NaClLog_mu(level, "VALIDATOR: At address %"NACL_PRIxNaClPcAddress":\n",
               addr);
    NaClLogTagNext_mu();
    NaClLog_mu(level, "VALIDATOR: %s", NaClLogLevelLabel(level));
    va_start(ap, format);
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
    NaClRecordErrorReported(state, level);
  }
}

void NaClValidatorInstMessage(int level,
                              NaClValidatorState* state,
                              NaClInstState* inst,
                              const char* format,
                              ...) {
  level = NaClRecordIfValidatorError(state, level);
  if (NaClPrintValidatorMessages(state, level)) {
    va_list ap;
    struct Gio* g = NaClLogGetGio();

    NaClLogLock();
    /* TODO(karl): empty fmt strings not allowed */
    NaClLog_mu(level, "VALIDATOR: %s", "");
    /* TODO(karl) - Make printing of instruction state possible via format. */
    NaClInstStateInstPrint(g, inst);

    va_start(ap, format);
    NaClLog_mu(level, "VALIDATOR: %s", NaClLogLevelLabel(level));
    NaClLogV_mu(level, format, ap);
    va_end(ap);
    NaClLogUnlock();
    NaClRecordErrorReported(state, level);
  }
}

Bool NaClValidatorQuit(NaClValidatorState* state) {
  return !state->validates_ok && (state->quit_after_error_count == 0);
}

void NaClRegisterValidator(
    NaClValidatorState* state,
    NaClValidator validator,
    NaClValidatorPostValidate post_validate,
    NaClValidatorPrintStats print_stats,
    NaClValidatorMemoryCreate create_memory,
    NaClValidatorMemoryDestroy destroy_memory) {
  NaClValidatorDefinition* defn;
  assert(NULL != validator);
  if (state->number_validators >= NACL_MAX_NCVALIDATORS) {
    NaClValidatorMessage(
        LOG_FATAL, state,
        "Too many validators specified, can't register. Quitting!\n");
  }
  defn = &state->validators[state->number_validators++];
  defn->validator = validator;
  defn->post_validate = post_validate;
  defn->print_stats = print_stats;
  defn->create_memory = create_memory;
  defn->destroy_memory = destroy_memory;
}

NaClValidatorState* NaClValidatorStateCreate(const NaClPcAddress vbase,
                                             const NaClMemorySize sz,
                                             const uint8_t alignment,
                                             const NaClOpKind base_register) {
  NaClValidatorState* state;
  NaClValidatorState* return_value = NULL;
  NaClPcAddress vlimit = vbase + sz;
  DEBUG(NaClLog(LOG_INFO,
                "Validator Create: vbase = %"NACL_PRIxNaClPcAddress", "
                "sz = %"NACL_PRIxNaClMemorySize", alignment = %u, vlimit = %"
                NACL_PRIxNaClPcAddress"\n",
                vbase, sz, alignment, vlimit));
  if (vlimit <= vbase) return NULL;
  if (alignment != 16 && alignment != 32) return NULL;
  state = (NaClValidatorState*) malloc(sizeof(NaClValidatorState));
  if (state != NULL) {
    return_value = state;
    state->vbase = vbase;
    state->sz = sz;
    state->alignment = alignment;
    state->vlimit = vlimit;
    state->alignment_mask = alignment - 1;
    GetCPUFeatures(&(state->cpu_features));
    state->base_register = base_register;
    state->validates_ok = TRUE;
    state->number_validators = 0;
    state->quit_after_error_count = NACL_FLAGS_max_reported_errors;
    state->print_opcode_histogram = NACL_FLAGS_opcode_histogram;
    state->trace_instructions = NACL_FLAGS_validator_trace_instructions;
    state->trace_inst_internals = NACL_FLAGS_validator_trace_inst_internals;
    state->log_verbosity = LOG_INFO;
    state->cur_inst_state = NULL;
    state->cur_inst = NULL;
    state->cur_inst_vector = NULL;
    state->quit = NaClValidatorQuit(return_value);
  }
  return return_value;
}

/* Add validators to validator state if missing. Assumed to be called just
 * before analyzing a code segment.
 */
static void NaClValidatorStateInitializeValidators(NaClValidatorState* state) {
  /* Note: Need to lazy initialize validators until after the call to
   * NaClValidateSegment, so that the user of the API can change flags.
   * Also, we must lazy initialize so that we don't break all callers,
   * who have followed the previous API.
   */
  if (0 == state->number_validators) {
    int i;
    NaClValidatorRulesInit(state);
    for (i = 0; i < state->number_validators; ++i) {
      NaClValidatorDefinition* defn = &state->validators[i];
      if (defn->create_memory == NULL) {
        state->local_memory[i] = NULL;
      } else {
        void* defn_memory = defn->create_memory(state);
        if (defn_memory == NULL) {
          NaClValidatorMessage(
              LOG_FATAL, state,
              "Unable to create local memory for validator function!");
          state->local_memory[i] = NULL;
        } else {
          state->local_memory[i] = defn_memory;
        }
      }
    }
  }
}

/* Given we are at the instruction defined by the instruction iterator, for
 * a segment, apply all applicable validator functions.
 */
static void NaClApplyValidators(NaClValidatorState* state, NaClInstIter* iter) {
  int i;
  DEBUG(NaClLog(LOG_INFO, "iter state:\n");
        NaClInstStateInstPrint(NaClLogGetGio(), NaClInstIterGetState(iter)));
  if (state->quit) return;
  for (i = 0; i < state->number_validators; ++i) {
    state->validators[i].validator(state, iter, state->local_memory[i]);
    if (state->quit) return;
  }
}

/* Given that we have just iterated through all instructions in a segment,
 * apply post validators rules (before we collect the iterator).
 */
static void NaClApplyPostValidators(NaClValidatorState* state,
                                    NaClInstIter* iter) {
  int i;
  DEBUG(NaClLog(LOG_INFO, "applying post validators...\n"));
  if (state->quit) return;
  for (i = 0; i < state->number_validators; ++i) {
    if (NULL != state->validators[i].post_validate) {
      state->validators[i].post_validate(state, iter, state->local_memory[i]);
      if (state->quit) return;
    }
  }
}

static void NaClValidatorStatePrintStats(NaClValidatorState* state);

/* The maximum lookback for the instruction iterator of the segment. */
static const size_t kLookbackSize = 4;

void NaClValidateSegment(uint8_t* mbase, NaClPcAddress vbase,
                         NaClMemorySize size, NaClValidatorState* state) {
  NaClSegment segment;
  NaClInstIter* iter;
  NaClValidatorStateInitializeValidators(state);
  NaClSegmentInitialize(mbase, vbase, size, &segment);
  for (iter = NaClInstIterCreateWithLookback(&segment, kLookbackSize);
       NaClInstIterHasNext(iter);
       NaClInstIterAdvance(iter)) {
    state->cur_inst_state = NaClInstIterGetState(iter);
    state->cur_inst = NaClInstStateInst(state->cur_inst_state);
    state->cur_inst_vector = NaClInstStateExpVector(state->cur_inst_state);
    NaClApplyValidators(state, iter);
    if (state->quit) break;
  }
  state->cur_inst_state = NULL;
  state->cur_inst = NULL;
  state->cur_inst_vector = NULL;
  NaClApplyPostValidators(state, iter);
  NaClInstIterDestroy(iter);
  NaClValidatorStatePrintStats(state);
}

Bool NaClValidatesOk(NaClValidatorState* state) {
  return state->validates_ok;
}

static void NaClValidatorStatePrintStats(NaClValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; i++) {
    NaClValidatorDefinition* defn = &state->validators[i];
    if (defn->print_stats != NULL) {
      defn->print_stats(state, state->local_memory[i]);
    }
  }
}

void NaClValidatorStateDestroy(NaClValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; ++i) {
    NaClValidatorDefinition* defn = &state->validators[i];
    void* defn_memory = state->local_memory[i];
    if (defn->destroy_memory != NULL && defn_memory != NULL) {
      defn->destroy_memory(state, defn_memory);
    }
  }
  free(state);
}

void* NaClGetValidatorLocalMemory(NaClValidator validator,
                                  const NaClValidatorState* state) {
  int i;
  for (i = 0; i < state->number_validators; ++i) {
    if (state->validators[i].validator == validator) {
      return state->local_memory[i];
    }
  }
  return NULL;
}
