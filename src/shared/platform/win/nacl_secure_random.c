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
#include <stdlib.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_secure_random.h"


void NaClSecureRngDtor(struct NaClSecureRngIf *vself);
static uint8_t NaClSecureRngGenByte(struct NaClSecureRngIf *vself);

static struct NaClSecureRngVtbl const kNaClSecureRngVtbl = {
  NaClSecureRngDtor,
  NaClSecureRngGenByte,
  NaClSecureRngDefaultGenUint32,
  NaClSecureRngDefaultGenBytes,
  NaClSecureRngDefaultUniform,
};

void NaClSecureRngModuleInit(void) {
  return;
}

void NaClSecureRngModuleFini(void) {
  return;
}

int NaClSecureRngCtor(struct NaClSecureRng *self) {
  self->base.vtbl = &kNaClSecureRngVtbl;
  self->nvalid = 0;
  return 1;
}

int NaClSecureRngTestingCtor(struct NaClSecureRng *self,
                             uint8_t              *seed_material,
                             size_t               seed_bytes) {
  self->base.vtbl = NULL;
  self->nvalid = 0;
  return 0;
}

void NaClSecureRngDtor(struct NaClSecureRngIf *vself) {
  vself->vtbl = NULL;
  return;
}

static void NaClSecureRngFilbuf(struct NaClSecureRng *self) {
  int bytes_filled = 0;
  int bytes_to_copy = 0;
  unsigned int random_int;
  for (bytes_filled = 0;
       bytes_filled < sizeof self->buf;
       bytes_filled += bytes_to_copy) {
    if (rand_s(&random_int)) {
      NaClLog(LOG_FATAL, "rand_s failed: error %d\n", GetLastError());
    }
    bytes_to_copy = min(sizeof(self->buf) - bytes_filled, sizeof(random_int));
    memcpy(self->buf + bytes_filled, &random_int, bytes_to_copy);
  }
  self->nvalid = sizeof self->buf;
}

static uint8_t NaClSecureRngGenByte(struct NaClSecureRngIf *vself) {
  struct NaClSecureRng *self = (struct NaClSecureRng *) vself;
  if (0 > self->nvalid) {
    NaClLog(LOG_FATAL,
            "NaClSecureRngGenByte: illegal buffer state, nvalid = %d\n",
            self->nvalid);
  }
  if (0 == self->nvalid) {
    NaClSecureRngFilbuf(self);
  }
  return self->buf[--self->nvalid];
}

