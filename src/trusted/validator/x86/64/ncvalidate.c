/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyValidator API for the x86-64 architecture. */
#include <assert.h>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/validation_cache.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state_internal.h"
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/validator/x86/nc_segment.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_decode_tables.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"

/* Be sure the correct compile flags are defined for this. */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
# error("Can't compile, target is for x86-64")
#else
# if NACL_TARGET_SUBARCH != 64
#  error("Can't compile, target is for x86-64")
# endif
#endif

NaClValidationStatus NaClValidatorSetup_x86_64(
    intptr_t guest_addr,
    size_t size,
    int readonly_text,
    const NaClCPUFeaturesX86 *cpu_features,
    struct NaClValidatorState** vstate_ptr) {
  *vstate_ptr = NaClValidatorStateCreate(guest_addr, size, RegR15,
                                         readonly_text, cpu_features);
  return (*vstate_ptr == NULL)
      ? NaClValidationFailedOutOfMemory
      : NaClValidationSucceeded;     /* or at least to this point! */
}

static NaClValidationStatus ApplyValidator_x86_64(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int stubout_mode,
    int readonly_text,
    const NaClCPUFeatures *f,
    const struct NaClValidationMetadata *metadata,
    struct NaClValidationCache *cache) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  struct NaClValidatorState *vstate;
  NaClValidationStatus status;
  void *query = NULL;

  /* Check that the given parameter values are supported. */
  if (stubout_mode && readonly_text)
    return NaClValidationFailedNotImplemented;

  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  /* Don't cache in stubout mode. */
  if (stubout_mode)
    cache = NULL;

  /* If the validation caching interface is available, perform a query. */
  if (cache != NULL)
    query = cache->CreateQuery(cache->handle);
  if (query != NULL) {
    const char validator_id[] = "x86-64";
    cache->AddData(query, (uint8_t *) validator_id, sizeof(validator_id));
    cache->AddData(query, (uint8_t *) cpu_features, sizeof(*cpu_features));
    AddCodeIdentity(data, size, metadata, cache, query);
    if (cache->QueryKnownToValidate(query)) {
      cache->DestroyQuery(query);
      return NaClValidationSucceeded;
    }
  }

  /* Init then validator state. */
  status = NaClValidatorSetup_x86_64(
      guest_addr, size, readonly_text, cpu_features, &vstate);
  if (status != NaClValidationSucceeded) {
    if (query != NULL)
      cache->DestroyQuery(query);
    return status;
  }
  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);
  NaClValidatorStateSetDoStubOut(vstate, stubout_mode);

  /* Validate. */
  NaClValidateSegment(data, guest_addr, size, vstate);
  status = (NaClValidatesOk(vstate) || stubout_mode) ?
      NaClValidationSucceeded : NaClValidationFailed;

  /* Cache the result if validation succeded and the code was not modified. */
  if (query != NULL) {
    /* Don't cache the result if the code is modified. */
    if (status == NaClValidationSucceeded && !NaClValidatorDidStubOut(vstate))
      cache->SetKnownToValidate(query);
    cache->DestroyQuery(query);
  }

  NaClValidatorStateDestroy(vstate);
  return status;
}

static NaClValidationStatus ApplyValidatorCodeReplacement_x86_64(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  NaClValidationStatus status;
  struct NaClValidatorState *vstate;

  /* Check that the given parameter values are supported. */
  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  /* Init then validator state. */
  status = NaClValidatorSetup_x86_64(guest_addr, size, FALSE,
                                     cpu_features, &vstate);
  if (status != NaClValidationSucceeded)
    return status;
  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);

  /* Validate. */
  NaClValidateSegmentPair(data_old, data_new, guest_addr, size, vstate);
  status = NaClValidatesOk(vstate) ?
      NaClValidationSucceeded : NaClValidationFailed;

  NaClValidatorStateDestroy(vstate);
  return status;
}

/* Copies code from src to dest in a thread safe way, returns 1 on success,
 * returns 0 on error. This will likely assert on error to avoid partially
 * copied code or undefined state.
 */
static int CopyCodeIter(uint8_t *dst, uint8_t *src,
                        NaClPcAddress vbase, size_t size,
                        NaClCopyInstructionFunc copy_func) {
  NaClSegment segment_old;
  NaClSegment segment_new;
  NaClInstIter *iter_old;
  NaClInstIter *iter_new;
  NaClInstState *istate_old;
  NaClInstState *istate_new;
  int still_good = 1;

  NaClSegmentInitialize(dst, vbase, size, &segment_old);
  NaClSegmentInitialize(src, vbase, size, &segment_new);

  iter_old = NaClInstIterCreate(kNaClValDecoderTables, &segment_old);
  if (NULL == iter_old) return 0;
  iter_new = NaClInstIterCreate(kNaClValDecoderTables, &segment_new);
  if (NULL == iter_new) {
    NaClInstIterDestroy(iter_old);
    return 0;
  }
  while (1) {
    /* March over every instruction, which means NaCl pseudo-instructions are
     * treated as multiple instructions.  Checks in NaClValidateCodeReplacement
     * guarantee that only valid replacements will happen, and no pseudo-
     * instructions should be touched.
     */
    if (!(NaClInstIterHasNext(iter_old) && NaClInstIterHasNext(iter_new))) {
      if (NaClInstIterHasNext(iter_old) || NaClInstIterHasNext(iter_new)) {
        NaClLog(LOG_ERROR,
                "Segment replacement: copy failed: iterators "
                "length mismatch\n");
        still_good = 0;
      }
      break;
    }
    istate_old = NaClInstIterGetState(iter_old);
    istate_new = NaClInstIterGetState(iter_new);
    if (istate_old->bytes.length != istate_new->bytes.length ||
        iter_old->memory.read_length != iter_new->memory.read_length ||
        istate_new->inst_addr != istate_old->inst_addr) {
      /* Sanity check: this should never happen based on checks in
       * NaClValidateInstReplacement.
       */
      NaClLog(LOG_ERROR,
              "Segment replacement: copied instructions misaligned\n");
      still_good = 0;
      break;
    }
    /* Replacing all modified instructions at once could yield a speedup here
     * as every time we modify instructions we must serialize all processors
     * twice.  Re-evaluate if code modification performance is an issue.
     */
    if (!copy_func(iter_old->memory.mpc, iter_new->memory.mpc,
                   iter_old->memory.read_length)) {
      NaClLog(LOG_ERROR,
              "Segment replacement: copy failed: unable to copy instruction\n");
      still_good = 0;
      break;
    }
    NaClInstIterAdvance(iter_old);
    NaClInstIterAdvance(iter_new);
  }

  NaClInstIterDestroy(iter_old);
  NaClInstIterDestroy(iter_new);
  return still_good;
}

static NaClValidationStatus ApplyValidatorCopy_x86_64(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *f,
    NaClCopyInstructionFunc copy_func) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  return (0 == CopyCodeIter(data_old, data_new, guest_addr, size, copy_func))
      ? NaClValidationFailed : NaClValidationSucceeded;
}

static const struct NaClValidatorInterface validator = {
  ApplyValidator_x86_64,
  ApplyValidatorCopy_x86_64,
  ApplyValidatorCodeReplacement_x86_64,
  sizeof(NaClCPUFeaturesX86),
  NaClSetAllCPUFeaturesX86,
  NaClGetCurrentCPUFeaturesX86,
  NaClFixCPUFeaturesX86,
};

const struct NaClValidatorInterface *NaClValidatorCreate_x86_64(void) {
  return &validator;
}
