/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

# if NACL_TARGET_SUBARCH == 32

#  include "native_client/src/trusted/validator_x86/ncvalidate.h"

int NaClValidateCode(struct NaClApp *nap, uintptr_t guest_addr,
                     uint8_t *data, size_t size) {
  struct NCValidatorState *vstate;
  int validator_result;

  if (nap->validator_stub_out_mode) {
    /* In stub out mode, we do two passes.  The second pass acts as a
       sanity check that bad instructions were indeed overwritten with
       allowable HLTs. */
    vstate = NCValidateInit(guest_addr, guest_addr + size, nap->bundle_size);
    if (vstate == NULL) {
      return LOAD_BAD_FILE;
    }
    NCValidateSetStubOutMode(vstate, 1);
    NCValidateSegment(data, guest_addr, size, vstate);
    NCValidateFinish(vstate);
    NCValidateFreeState(&vstate);
  }

  vstate = NCValidateInit(guest_addr, guest_addr + size, nap->bundle_size);
  if (vstate == NULL) {
    return LOAD_BAD_FILE;
  }
  NCValidateSegment(data, guest_addr, size, vstate);
  validator_result = NCValidateFinish(vstate);
  NCValidateFreeState(&vstate);
  if (validator_result != 0) {
    return LOAD_VALIDATION_FAILED;
  }
  return LOAD_OK;
}

# else

#  include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

int NaClValidateCode(struct NaClApp *nap, uintptr_t guest_addr,
                     uint8_t *data, size_t size) {
  struct NaClValidatorState *vstate;
  int is_ok;

  vstate = NaClValidatorStateCreate(guest_addr, size, nap->bundle_size,
                                    RegR15, 1, stderr);
  if (vstate == NULL) {
    return LOAD_BAD_FILE;
  }
  NaClValidateSegment(data, guest_addr, size, vstate);
  is_ok = NaClValidatesOk(vstate);
  NaClValidatorStateDestroy(vstate);
  if (!is_ok) {
    return LOAD_VALIDATION_FAILED;
  }
  return LOAD_OK;
}

# endif

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

# include "native_client/src/trusted/validator_arm/v2/ncvalidate.h"

int NaClValidateCode(struct NaClApp *nap, uintptr_t guest_addr,
                     uint8_t *data, size_t size) {
  UNREFERENCED_PARAMETER(nap);

  if (NCValidateSegment(data, guest_addr, size) == 0) {
    return LOAD_OK;
  }
  else {
    return LOAD_VALIDATION_FAILED;
  }
}

#endif

NaClErrorCode NaClValidateImage(struct NaClApp  *nap) {
  uintptr_t               memp;
  uintptr_t               endp;
  size_t                  regionsize;
  NaClErrorCode           rcode;

  memp = nap->mem_start + NACL_TRAMPOLINE_END;
  endp = nap->mem_start + nap->static_text_end;
  regionsize = endp - memp;
  if (endp < memp) {
    return LOAD_NO_MEMORY;
  }

  rcode = NaClValidateCode(nap, NACL_TRAMPOLINE_END,
                           (uint8_t *) memp, regionsize);
  if (LOAD_OK != rcode) {
    if (nap->ignore_validator_result) {
      NaClLog(LOG_ERROR, "VALIDATION FAILED: continuing anyway...\n");
      rcode = LOAD_OK;
    } else {
      NaClLog(LOG_ERROR, "VALIDATION FAILED.\n");
      NaClLog(LOG_ERROR,
              "Run sel_ldr in debug mode to ignore validation failure.\n");
      NaClLog(LOG_ERROR,
              "Run ncval <module-name> for validation error details.\n");
      rcode = LOAD_VALIDATION_FAILED;
    }
  }
  return rcode;
}
