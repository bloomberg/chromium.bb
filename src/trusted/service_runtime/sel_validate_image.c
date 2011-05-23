/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

# include "native_client/src/trusted/validator_x86/nccopycode.h"
# include "native_client/src/trusted/validator_x86/nacl_cpuid.h"

# if NACL_TARGET_SUBARCH == 32

#  include "native_client/src/trusted/validator_x86/ncvalidate.h"

int NaClValidateCode(struct NaClApp *nap, uintptr_t guest_addr,
                     uint8_t *data, size_t size) {
  struct NCValidatorState *vstate;
  int validator_result = 0;
  NaClCPUData cpu_data;

  NaClCPUDataGet(&cpu_data);
  if (!NaClArchSupported(&cpu_data)) return LOAD_VALIDATION_FAILED;

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

int NaClValidateCodeReplacement(struct NaClApp *nap, uintptr_t guest_addr,
                                uint8_t *data_old, uint8_t *data_new,
                                size_t size) {
  NaClCPUData cpu_data;

  NaClCPUDataGet(&cpu_data);
  if (!NaClArchSupported(&cpu_data)) return LOAD_VALIDATION_FAILED;

  if (nap->validator_stub_out_mode) {
    NaClLog(1, "NaClValidateCodeReplacement:  "
               "stub_out_mode not supported for code replacement\n");
    return LOAD_BAD_FILE;
  }

  if ((guest_addr % nap->bundle_size) != 0 ||
            (size % nap->bundle_size) != 0) {
    NaClLog(1, "NaClValidateCodeReplacement:  "
               "code replacement is not properly bundle-aligned\n");
    return LOAD_BAD_FILE;
  }

  if (!NCValidateSegmentPair(data_old, data_new, guest_addr, size,
                             nap->bundle_size)) {
    return LOAD_VALIDATION_FAILED;
  }
  return LOAD_OK;
}

int NaClCopyCode(struct NaClApp *nap, uintptr_t guest_addr,
                 uint8_t *data_old, uint8_t *data_new,
                 size_t size) {
  int result;
  NaClCPUData cpu_data;

  NaClCPUDataGet(&cpu_data);
  if (!NaClArchSupported(&cpu_data)) return LOAD_UNLOADABLE;

  result = NCCopyCode(data_old, data_new, guest_addr, size, nap->bundle_size);
  if (0 == result) {
    return LOAD_UNLOADABLE;
  }
  return LOAD_OK;
}

# else

#  include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

int NaClValidateCode(struct NaClApp *nap, uintptr_t guest_addr,
                     uint8_t *data, size_t size) {
  struct NaClValidatorState *vstate;
  int is_ok;
  NaClCPUData cpu_data;

  NaClCPUDataGet(&cpu_data);
  if (!NaClArchSupported(&cpu_data)) return LOAD_VALIDATION_FAILED;

  vstate = NaClValidatorStateCreate(guest_addr, size, nap->bundle_size,
                                    RegR15);
  if (vstate == NULL) {
    return LOAD_BAD_FILE;
  }

  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);

  if (nap->validator_stub_out_mode) {
    /* In stub out mode, we do two passes. The second pass acts as a sanity
     * check, after illegal instructions have been stubbed out with allowable
     * HLTs.
     * Start pass one to find errors, and stub out illegal instructions.
     */
    NaClValidatorStateSetDoStubOut(vstate, TRUE);
    NaClValidateSegment(data, guest_addr, size, vstate);
    NaClValidatorStateDestroy(vstate);
    vstate = NaClValidatorStateCreate(guest_addr, size, nap->bundle_size,
                                      RegR15);
    NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);
  }
  NaClValidateSegment(data, guest_addr, size, vstate);
  is_ok = NaClValidatesOk(vstate);
  NaClValidatorStateDestroy(vstate);
  if (!is_ok) {
    return LOAD_VALIDATION_FAILED;
  }
  return LOAD_OK;
}

int NaClValidateCodeReplacement(struct NaClApp *nap, uintptr_t guest_addr,
                                uint8_t *data_old, uint8_t *data_new,
                                size_t size) {
  struct NaClValidatorState *vstate;
  NaClCPUData cpu_data;
  int is_ok;

  NaClCPUDataGet(&cpu_data);
  if (!NaClArchSupported(&cpu_data)) return LOAD_VALIDATION_FAILED;

  if (nap->validator_stub_out_mode) {
    NaClLog(1, "NaClValidateCodeReplacement:  "
               "stub_out_mode not supported for code replacement\n");
    return LOAD_BAD_FILE;
  }

  if ((guest_addr % nap->bundle_size) != 0 ||
            (size % nap->bundle_size) != 0) {
    NaClLog(1, "NaClValidateCodeReplacement:  "
               "code replacement is not properly bundle-aligned\n");
    return LOAD_BAD_FILE;
  }

  vstate = NaClValidatorStateCreate(guest_addr, size, nap->bundle_size,
                                    RegR15);
  if (NULL == vstate) {
    return LOAD_BAD_FILE;
  }
  NaClValidatorStateSetLogVerbosity(vstate, LOG_ERROR);

  NaClValidateSegmentPair(data_old, data_new, guest_addr, size, vstate);
  is_ok = NaClValidatesOk(vstate);
  NaClValidatorStateDestroy(vstate);
  if (!is_ok) {
    return LOAD_VALIDATION_FAILED;
  }
  return LOAD_OK;
}

int NaClCopyCode(struct NaClApp *nap, uintptr_t guest_addr,
                 uint8_t *data_old, uint8_t *data_new,
                 size_t size) {
  int result;
  NaClCPUData cpu_data;
  UNREFERENCED_PARAMETER(nap);

  NaClCPUDataGet(&cpu_data);
  if (!NaClArchSupported(&cpu_data)) return LOAD_UNLOADABLE;

  result = NaClCopyCodeIter(data_old, data_new, guest_addr, size);
  if (0 == result) {
    return LOAD_UNLOADABLE;
  }
  return LOAD_OK;
}

# endif

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

# include "native_client/src/trusted/validator_arm/ncvalidate.h"

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

int NaClValidateCodeReplacement(struct NaClApp *nap, uintptr_t guest_addr,
                                uint8_t *data_old, uint8_t *data_new,
                                size_t size) {
  UNREFERENCED_PARAMETER(nap);
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(data_old);
  UNREFERENCED_PARAMETER(data_new);
  UNREFERENCED_PARAMETER(size);
  NaClLog(1, "NaClValidateCodeReplacement: "
             "code replacement not yet supported on ARM\n");
  return LOAD_UNIMPLEMENTED;
}

int NaClCopyCode(struct NaClApp *nap, uintptr_t guest_addr,
                 uint8_t *data_old, uint8_t *data_new,
                 size_t size) {
  UNREFERENCED_PARAMETER(nap);
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(data_old);
  UNREFERENCED_PARAMETER(data_new);
  UNREFERENCED_PARAMETER(size);
  NaClLog(1, "NaClCopyCode: "
             "code replacement not yet supported on ARM\n");
  return LOAD_UNIMPLEMENTED;
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

  if (nap->skip_validator) {
    NaClLog(LOG_ERROR, "VALIDATION SKIPPED.\n");
    return LOAD_OK;
  } else {
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
  }
  return rcode;
}
