/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Invoke validator for NaCl secure ELF loader (NaCl SEL).
 */

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/validator_x86/ncvalidate.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

static int g_ignore_validator_result = 0;
void NaClIgnoreValidatorResult() {
  g_ignore_validator_result = 1;
}

NaClErrorCode NaClValidateImage(struct NaClApp  *nap) {
  uintptr_t               memp;
  uintptr_t               endp;
#if NACL_TARGET_SUBARCH == 32
  struct NCValidatorState *vstate;
#else
  struct NaClValidatorState *vstate;
#endif
  size_t                  regionsize;
  NaClErrorCode           rcode = LOAD_BAD_FILE;

  memp = nap->mem_start + NACL_TRAMPOLINE_END;
  endp = nap->mem_start + nap->static_text_end;
  regionsize = endp - memp;
  if (endp < memp) {
    return LOAD_NO_MEMORY;
  }
#if NACL_TARGET_SUBARCH == 32
  vstate = NCValidateInit(memp, endp, nap->bundle_size);
  if (NULL == vstate) return LOAD_BAD_FILE;
  NCValidateSegment((uint8_t *)memp, memp, regionsize, vstate);
  if (NCValidateFinish(vstate) == 0) {
    rcode = LOAD_OK;
  }
  NCValidateFreeState(&vstate);
#else
  vstate = NaClValidatorStateCreate(memp,
                                    regionsize,
                                    nap->bundle_size,
                                    RegR15,
                                    1,
                                    stderr);
  NaClValidateSegment((uint8_t*)memp, memp, regionsize, vstate);
  if (NaClValidatesOk(vstate)) {
    rcode = LOAD_OK;
  }
  NaClValidatorStateDestroy(vstate);
#endif
  if (LOAD_OK != rcode) {
    if (g_ignore_validator_result) {
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
