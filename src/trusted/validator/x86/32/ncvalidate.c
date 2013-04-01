/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyValidator API for the x86-32 architecture. */

#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/validation_cache.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_detailed.h"
/* HACK to get access to didstubout */
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_internaltypes.h"
#include <assert.h>

/* Be sure the correct compile flags are defined for this. */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
# error("Can't compile, target is for x86-32")
#else
# if NACL_TARGET_SUBARCH != 32
#  error("Can't compile, target is for x86-32")
# endif
#endif

static NaClValidationStatus ApplyValidator_x86_32(
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
  struct NCValidatorState *vstate;
  int validator_result = 0;
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
    const char validator_id[] = "x86-32";
    cache->AddData(query, (uint8_t *) validator_id, sizeof(validator_id));
    cache->AddData(query, (uint8_t *) cpu_features, sizeof(*cpu_features));
    AddCodeIdentity(data, size, metadata, cache, query);
    if (cache->QueryKnownToValidate(query)) {
      cache->DestroyQuery(query);
      return NaClValidationSucceeded;
    }
  }

  /* Init then validator state. */
  /* TODO(ncbray) make "detailed" a parameter. */
  if (stubout_mode) {
    vstate = NCValidateInitDetailed(guest_addr, size, cpu_features);
  } else {
    vstate = NCValidateInit(guest_addr, size, readonly_text, cpu_features);
  }
  if (vstate == NULL) {
    if (query != NULL)
      cache->DestroyQuery(query);
    return NaClValidationFailedOutOfMemory;
  }
  NCValidateSetStubOutMode(vstate, stubout_mode);

  /* Validate. */
  NCValidateSegment(data, guest_addr, size, vstate);
  validator_result = NCValidateFinish(vstate);

  /* Cache the result if validation succeeded and the code was not modified. */
  if (query != NULL) {
    if (validator_result == 0 && !NCValidatorDidStubOut(vstate))
      cache->SetKnownToValidate(query);
    cache->DestroyQuery(query);
  }

  NCValidateFreeState(&vstate);
  return (validator_result == 0 || stubout_mode)
      ? NaClValidationSucceeded : NaClValidationFailed;
}

static NaClValidationStatus ApplyValidatorCodeReplacement_x86_32(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;

  /* Check that the given parameter values are supported. */
  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  return NCValidateSegmentPair(data_old, data_new, guest_addr,
                               size, cpu_features)
      ? NaClValidationSucceeded : NaClValidationFailed;
}

/* Copy a single instruction, avoiding the possibility of other threads
 * executing a partially changed instruction.
 */
static Bool CopyInstruction(NCDecoderStatePair *self,
                            NCDecoderInst *dinst_old,
                            NCDecoderInst *dinst_new) {
  NCRemainingMemory* mem_old = &dinst_old->dstate->memory;
  NCRemainingMemory* mem_new = &dinst_new->dstate->memory;

  return self->copy_func(mem_old->mpc, mem_new->mpc, mem_old->read_length);
}

/* Copies code from src to dest in a thread safe way, returns 1 on success,
 * returns 0 on error. This will likely assert on error to avoid partially
 * copied code or undefined state.
 */
static int NCCopyCode(uint8_t *dst, uint8_t *src, NaClPcAddress vbase,
                      size_t sz, NaClCopyInstructionFunc copy_func) {
  NCDecoderState dst_dstate;
  NCDecoderInst  dst_inst;
  NCDecoderState src_dstate;
  NCDecoderInst  src_inst;
  NCDecoderStatePair pair;
  int result = 0;

  NCDecoderStateConstruct(&dst_dstate, dst, vbase, sz, &dst_inst, 1);
  NCDecoderStateConstruct(&src_dstate, src, vbase, sz, &src_inst, 1);
  NCDecoderStatePairConstruct(&pair, &dst_dstate, &src_dstate, copy_func);
  pair.action_fn = CopyInstruction;
  if (NCDecoderStatePairDecode(&pair)) result = 1;
  NCDecoderStatePairDestruct(&pair);
  NCDecoderStateDestruct(&src_dstate);
  NCDecoderStateDestruct(&dst_dstate);

  return result;
}

static NaClValidationStatus ApplyValidatorCopy_x86_32(
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

  return ((0 == NCCopyCode(data_old, data_new, guest_addr, size, copy_func))
            ? NaClValidationFailed : NaClValidationSucceeded);
}

static const struct NaClValidatorInterface validator = {
  ApplyValidator_x86_32,
  ApplyValidatorCopy_x86_32,
  ApplyValidatorCodeReplacement_x86_32,
  sizeof(NaClCPUFeaturesX86),
  NaClSetAllCPUFeaturesX86,
  NaClGetCurrentCPUFeaturesX86,
  NaClFixCPUFeaturesX86,
};

const struct NaClValidatorInterface *NaClValidatorCreate_x86_32(void) {
  return &validator;
}
