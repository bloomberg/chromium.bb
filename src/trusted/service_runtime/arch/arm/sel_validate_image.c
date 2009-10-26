/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  regionsize = nap->text_region_bytes;

  endp = memp + regionsize;
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

