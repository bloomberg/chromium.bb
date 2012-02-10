/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyValidatorVerbosely API for the x86-64 architecture. */

#include "native_client/src/trusted/validator/ncvalidate.h"

#include "native_client/src/shared/platform/nacl_log.h"
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
    int bundle_size,
    CPUFeatures *cpu_features) {
  struct NaClValidatorState *vstate;
  NaClValidationStatus status =
      NaClValidatorSetup_x86_64(guest_addr, size, bundle_size, cpu_features,
                                &vstate);
  if (status != NaClValidationSucceeded) return status;
  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);
  NaClValidatorStateSetMaxReportedErrors(vstate, -1);  /* Report all errors. */
  NaClValidatorStateSetErrorReporter(vstate, &kNaClVerboseErrorReporter);
  return NaClSegmentValidate_x86_64(guest_addr, data, size, vstate)
      ? NaClValidationSucceeded : NaClValidationFailed;
}

NaClValidationStatus NACL_SUBARCH_NAME(ApplyValidatorVerbosely, x86, 64)
    (enum NaClSBKind sb_kind,
     NaClApplyValidationKind kind,
     uintptr_t guest_addr,
     uint8_t *data,
     size_t size,
     int bundle_size,
     Bool local_cpu) {
  NaClValidationStatus status = NaClValidationFailedNotImplemented;
  assert(NACL_SB_DEFAULT == sb_kind);
  if (bundle_size == 16 || bundle_size == 32) {
    CPUFeatures cpu_features;
    NaClValidatorGetCPUFeatures(local_cpu, &cpu_features);
    if (!NaClArchSupported(&cpu_features))
      return NaClValidationFailedCpuNotSupported;
    switch (kind) {
      case NaClApplyCodeValidation:
        status = NaClApplyValidatorVerbosely_x86_64(
            guest_addr, data, size, bundle_size, &cpu_features);
        break;
      case NaClApplyValidationDoStubout:
        status = NaClApplyValidatorStubout_x86_64(
            guest_addr, data, size, bundle_size, &cpu_features);
        break;
      default:
        /* If reached, it isn't implemented (yet). */
        break;
    }
  }
  return status;
}
