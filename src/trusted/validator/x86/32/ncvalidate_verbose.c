/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyValidatorVerbosely API for the x86-32 architecture. */

#include "native_client/src/trusted/validator/ncvalidate.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_detailed.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"
#include <assert.h>

/* Be sure the correct compile flags are defined for this. */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
# error("Can't compile, target is for x86-32")
#else
# if NACL_TARGET_SUBARCH != 32
#  error("Can't compile, target is for x86-32")
# endif
#endif

static NaClValidationStatus NCApplyValidatorVerbosely_x86_32(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    const NaClCPUFeaturesX86 *cpu_features) {
  int validator_result = 0;
  struct NCValidatorState *vstate;

  vstate = NCValidateInitDetailed(guest_addr, size, cpu_features);
  if (vstate == NULL) return NaClValidationFailedOutOfMemory;
  NCValidateSetNumDiagnostics(vstate, -1);  /* Reports all errors. */
  NCValidateSetErrorReporter(vstate, &kNCVerboseErrorReporter);
  NCValidateSegment(data, guest_addr, size, vstate);
  validator_result = NCValidateFinish(vstate);
  NCValidateFreeState(&vstate);
  return (validator_result == 0)
      ? NaClValidationSucceeded : NaClValidationFailed;
}

NaClValidationStatus NACL_SUBARCH_NAME(ApplyValidatorVerbosely, x86, 32)
    (uintptr_t guest_addr,
     uint8_t *data,
     size_t size,
     const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  const NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;

  return NCApplyValidatorVerbosely_x86_32(
      guest_addr, data, size, cpu_features);
}
