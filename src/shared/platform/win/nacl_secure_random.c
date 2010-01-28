/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  UNREFERENCED_PARAMETER(seed_material);
  UNREFERENCED_PARAMETER(seed_bytes);
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

