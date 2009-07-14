/*
 * Copyright 2008, Google Inc.
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
 * NaCl Service Runtime.  Secure RNG implementation.
 */

#include "native_client/src/trusted/platform/nacl_secure_random.h"

#include "native_client/src/trusted/platform/nacl_log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <wincrypt.h>
/* requires advapi32.lib */

static HCRYPTPROV hProv;

static struct NaClSecureRngVtbl const kNaClSecureRngVtbl;


void NaClSecureRngModuleInit(void) {
  /* is PROV_RSA_AES reasonable? */
  if (!CryptAcquireContext(&hProv,
                           NULL,
                           NULL,
                           PROV_RSA_AES,
                           CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
    NaClLog(LOG_FATAL, "CryptAcquireContext failed: error %d\n",
            GetLastError());
  }
}

void NaClSecureRngModuleFini(void) {
  (void) CryptReleaseContext(hProv, 0);
  return;
}

#if USE_CRYPTO
# error "We need to get AES code mapped in before this can be written/enabled"
#else

int NaClSecureRngCtor(struct NaClSecureRng *self) {
  self->base.vtbl = &kNaClSecureRngVtbl;
  self->nvalid = 0;
  return 1;
}

int NaClSecureRngTestingCtor(struct NaClSecureRng *self,
                             uint8_t              *seed_material,
                             size_t               seed_bytes) {
  /* CryptImportKey does not import unencrypted symmetric keys */
  self->base.vtbl = NULL;
  return 0;
}

void NaClSecureRngDtor(struct NaClSecureRngIf *vself) {
  vself->vtbl = NULL;
  return;
}

static void NaClSecureRngFilbuf(struct NaClSecureRng *self) {
  if (!CryptGenRandom(hProv, sizeof self->buf, (BYTE *) self->buf)) {
    NaClLog(LOG_FATAL, "CryptGenRandom failed, error %d\n",
            GetLastError());
  }
  self->nvalid = sizeof self->buf;
}

static uint8_t NaClSecureRngGenByte(struct NaClSecureRngIf *vself) {
  struct NaClSecureRng *self = (struct NaClSecureRng *) vself;

  if (0 <= self->nvalid) {
    NaClSecureRngFilbuf(self);
  }
  return self->buf[--self->nvalid];
}

#endif  /* USE_CRYPTO */

static struct NaClSecureRngVtbl const kNaClSecureRngVtbl = {
  NaClSecureRngDtor,
  NaClSecureRngGenByte,
  NaClSecureRngDefaultGenUint32,
  NaClSecureRngDefaultGenBytes,
  NaClSecureRngDefaultUniform,
};
