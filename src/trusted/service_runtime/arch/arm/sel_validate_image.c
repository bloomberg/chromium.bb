/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Invoke validator for NaCl secure ELF loader (NaCl SEL).
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/validator_arm/v2/ncvalidate.h"

static int g_ignore_validator_result = 0;
void NaClIgnoreValidatorResult() {
  g_ignore_validator_result = 1;
}

NaClErrorCode NaClValidateImage(struct NaClApp  *nap) {
  uintptr_t               memp;
  uintptr_t               endp;
  size_t                  regionsize;

  memp = nap->mem_start + NACL_TRAMPOLINE_END;
  endp = nap->mem_start + nap->static_text_end;
  regionsize = endp - memp;
  if (endp < memp) {
    return LOAD_NO_MEMORY;
  }

  if (NCValidateSegment((uint8_t *)memp, memp, regionsize) == 0) {
    return LOAD_OK;
  } else {
    if (g_ignore_validator_result) {
      NaClLog(LOG_ERROR, "VALIDATION FAILED: continuing anyway...\n");
      return LOAD_OK;
    } else {
      NaClLog(LOG_ERROR, "VALIDATION FAILED.\n");
      NaClLog(LOG_ERROR,
              "Run sel_ldr in debug mode to ignore validation failure.\n");
      NaClLog(LOG_ERROR,
              "Run ncval <module-name> for validation error details.\n");
      return LOAD_VALIDATION_FAILED;
    }
  }
}

