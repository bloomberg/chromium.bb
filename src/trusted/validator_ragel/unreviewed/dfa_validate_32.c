/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the Validator API for the x86-32 architecture. */
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


static Bool ProcessError(const uint8_t *begin, const uint8_t *end,
                         uint32_t info, void *callback_data) {
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback_data);
  return FALSE;
}

NaClValidationStatus ApplyDfaValidator_x86_32(
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
  if (ValidateChunkIA32(data, size, 0 /*options*/, cpu_features,
                        ProcessError, NULL)) {
    return NaClValidationSucceeded;
  }
  return NaClValidationFailed;
}

static NaClValidationStatus ValidatorCopyNotImplemented(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *cpu_features,
    NaClCopyInstructionFunc copy_func) {
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(data_old);
  UNREFERENCED_PARAMETER(data_new);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(cpu_features);
  UNREFERENCED_PARAMETER(copy_func);
  return NaClValidationFailedNotImplemented;
}

static NaClValidationStatus ValidatorCodeReplacementNotImplemented(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *cpu_features) {
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(data_old);
  UNREFERENCED_PARAMETER(data_new);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(cpu_features);
  return NaClValidationFailedNotImplemented;
}

static const struct NaClValidatorInterface validator = {
  ApplyDfaValidator_x86_32,
  ValidatorCopyNotImplemented,
  ValidatorCodeReplacementNotImplemented,
};

const struct NaClValidatorInterface *NaClDfaValidatorCreate_x86_32() {
  return &validator;
}
