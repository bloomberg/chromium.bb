/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyValidatorVerbosely API for the x86-64 architecture. */

#include "native_client/src/trusted/validator/ncvalidate.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/64/ncvalidate.h"
#include <assert.h>

/* Be sure the correct compile flags are defined for this. */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
# error("Can't compile, target is for x86-64")
#else
# if NACL_TARGET_SUBARCH != 64
#  error("Can't compile, target is for x86-64")
# endif
#endif

static NaClValidationStatus NaClApplyValidatorVerbosely_x86_64(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  struct NaClValidatorState *vstate;
  NaClValidationStatus status =
      NaClValidatorSetup_x86_64(guest_addr, size, FALSE, cpu_features, &vstate);
  if (status != NaClValidationSucceeded) return status;
  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);
  NaClValidatorStateSetMaxReportedErrors(vstate, -1);  /* Report all errors. */
  NaClValidatorStateSetErrorReporter(vstate, &kNaClVerboseErrorReporter);
  NaClValidateSegment(data, guest_addr, size, vstate);
  status =
      NaClValidatesOk(vstate) ? NaClValidationSucceeded : NaClValidationFailed;
  NaClValidatorStateDestroy(vstate);
  return status;
}

NaClValidationStatus NACL_SUBARCH_NAME(ApplyValidatorVerbosely, x86, 64)
    (uintptr_t guest_addr,
     uint8_t *data,
     size_t size,
     const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;

  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  return NaClApplyValidatorVerbosely_x86_64(
      guest_addr, data, size, f);
}
