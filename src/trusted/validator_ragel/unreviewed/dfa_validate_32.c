/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyDfaValidator API for the x86-32 architecture. */
#include <assert.h>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/validation_cache.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

/* Be sure the correct compile flags are defined for this. */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
# error("Can't compile, target is for x86-32")
#else
# if NACL_TARGET_SUBARCH != 32
#  error("Can't compile, target is for x86-32")
# endif
#endif


static void ProcessError(const uint8_t *ptr, void *userdata) {
  UNREFERENCED_PARAMETER(ptr);
  UNREFERENCED_PARAMETER(userdata);
}

NaClValidationStatus NACL_SUBARCH_NAME(ApplyDfaValidator, x86, 32) (
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int stubout_mode,
    int readonly_text,
    const NaClCPUFeaturesX86 *cpu_features,
    struct NaClValidationCache *cache) {
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(cache);

  if (stubout_mode || readonly_text) {
    return NaClValidationFailedNotImplemented;
  }
  if (!NaClArchSupported(cpu_features)) {
    return NaClValidationFailedCpuNotSupported;
  }
  if (ValidateChunkIA32(data, size, cpu_features, ProcessError, 0) == 0) {
    return NaClValidationSucceeded;
  }
  return NaClValidationFailed;
}
