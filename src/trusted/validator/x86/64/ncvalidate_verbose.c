/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the ApplyValidatorVerbosely API for the x86-64 architecture. */

#include "native_client/src/trusted/validator/ncvalidate.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/64/ncvalidate.h"

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
    Bool local_cpu) {
  CPUFeatures features;
  int is_ok;
  struct NaClValidatorState *vstate =
      NaClValidatorStateCreate(guest_addr, size, bundle_size, RegR15);
  if (vstate == NULL) return NaClValidationFailedOutOfMemory;
  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);
  if (!local_cpu) {
    NaClSetAllCPUFeatures(&features);
    NaClValidatorStateSetCPUFeatures(vstate, &features);
  }
  NaClValidatorStateSetErrorReporter(vstate, &kNaClVerboseErrorReporter);
  NaClValidateSegment(data, guest_addr, size, vstate);
  is_ok = NaClValidatesOk(vstate);
  NaClValidatorStateDestroy(vstate);
  return is_ok ? NaClValidationSucceeded : NaClValidationFailed;
}

NaClValidationStatus NACL_SUBARCH_NAME(ApplyValidatorVerbosely, x86, 64)
    (NaClApplyValidationKind kind,
     uintptr_t guest_addr,
     uint8_t *data,
     size_t size,
     int bundle_size,
     Bool local_cpu) {
  NaClValidationStatus status = NaClValidationFailedNotImplemented;

  if (bundle_size == 16 || bundle_size == 32) {
    if (local_cpu) {
      NaClCPUData cpu_data;
      NaClCPUDataGet(&cpu_data);
      if (!NaClArchSupported(&cpu_data))
        return NaClValidationFailedCpuNotSupported;
    }
    switch (kind) {
      case NaClApplyCodeValidation:
        status = NaClApplyValidatorVerbosely_x86_64(
            guest_addr, data, size, bundle_size, local_cpu);
        break;
      case NaClApplyValidationDoStubout:
        status = NaClApplyValidatorStubout_x86_64(
            guest_addr, data, size, bundle_size, local_cpu);
        break;
      default:
        /* If reached, it isn't implemented (yet). */
        break;
    }
  }
  return status;
}
