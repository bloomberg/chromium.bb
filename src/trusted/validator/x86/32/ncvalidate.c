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

NaClValidationStatus NACL_SUBARCH_NAME(ApplyValidator, NACL_TARGET_ARCH, 32) (
    enum NaClSBKind sb_kind,
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int bundle_size,
    int stubout_mode,
    int readonly_text,
    const NaClCPUFeaturesX86 *cpu_features,
    struct NaClValidationCache *cache) {
  struct NCValidatorState *vstate;
  int validator_result = 0;
  void *query = NULL;

  /* Check that the given parameter values are supported. */
  if (sb_kind != NACL_SB_DEFAULT)
    return NaClValidationFailedNotImplemented;

  if (bundle_size != 16 && bundle_size != 32)
    return NaClValidationFailedNotImplemented;

  if (stubout_mode && readonly_text)
    return NaClValidationFailedNotImplemented;

  if (!NaClArchSupported(cpu_features))
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
    cache->AddData(query, data, size);
    if (cache->QueryKnownToValidate(query)) {
      cache->DestroyQuery(query);
      return NaClValidationSucceeded;
    }
  }

  /* Init then validator state. */
  /* TODO(ncbray) make "detailed" a parameter. */
  if (stubout_mode) {
    vstate = NCValidateInitDetailed(guest_addr, size, bundle_size,
                                    cpu_features);
  } else {
    vstate = NCValidateInit(guest_addr, size, bundle_size, readonly_text,
                            cpu_features);
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

NaClValidationStatus NACL_SUBARCH_NAME(ApplyValidatorCodeReplacement, x86, 32)
    (enum NaClSBKind sb_kind,
     uintptr_t guest_addr,
     uint8_t *data_old,
     uint8_t *data_new,
     size_t size,
     int bundle_size,
     const NaClCPUFeaturesX86 *cpu_features) {
  /* Check that the given parameter values are supported. */
  if (sb_kind != NACL_SB_DEFAULT)
    return NaClValidationFailedNotImplemented;

  if (bundle_size != 16 && bundle_size != 32)
    return NaClValidationFailedNotImplemented;

  if (!NaClArchSupported(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  return NCValidateSegmentPair(data_old, data_new, guest_addr,
                               size, bundle_size, cpu_features)
      ? NaClValidationSucceeded : NaClValidationFailed;
}
