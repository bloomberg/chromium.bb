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
 * NaCl Service Runtime.  Directory descriptor abstraction.
 */

#include "native_client/src/include/portability.h"

#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

static int NaClDescInvalidExternalizeSize(struct NaClDesc      *vself,
                                          size_t               *nbytes,
                                          size_t               *nhandles) {
  UNREFERENCED_PARAMETER(vself);
  *nbytes = 0;
  *nhandles = 0;
  return 0;
}

static int NaClDescInvalidExternalize(struct NaClDesc          *vself,
                                      struct NaClDescXferState *xfer) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(xfer);
  return 0;
}

/*
 * NaClDescInvalid is the subclass for the singleton invalid descriptor.
 */

static struct NaClDescVtbl const kNaClDescInvalidVtbl = {
  NaClDescDtorNotImplemented,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescFstatNotImplemented,
  NaClDescCloseNotImplemented,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_INVALID,
  NaClDescInvalidExternalizeSize,
  NaClDescInvalidExternalize,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnectAddrNotImplemented,
  NaClDescAcceptConnNotImplemented,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

struct NaClDescInvalid {
  struct NaClDesc base;
};

static struct NaClMutex *mutex = NULL;
static struct NaClDescInvalid *singleton;

void NaClDescInvalidInit() {
  mutex = (struct NaClMutex *) malloc(sizeof(*mutex));
  if (NULL == mutex) {
    NaClLog(LOG_FATAL, "Cannot allocate NaClDescInvalid mutex\n");
  }
  if (!NaClMutexCtor(mutex)) {
    free(mutex);
    mutex = NULL;
    NaClLog(LOG_FATAL, "Cannot construct NaClDescInvalid mutex\n");
  }
}

void NaClDescInvalidFini() {
  if (NULL != mutex) {
    NaClMutexDtor(mutex);
  }
}

struct NaClDescInvalid const *NaClDescInvalidMake() {
  NaClXMutexLock(mutex);
  if (NULL == singleton) {
    do {
      /* Allocate an instance. */
      singleton = (struct NaClDescInvalid *) malloc(sizeof(*singleton));
      if (NULL == singleton) {
        break;
      }
      /* Do the base class construction. */
      if (!NaClDescCtor(&(singleton->base))) {
        free(singleton);
        singleton = NULL;
        break;
      }
      /* Construct the derived class (simply set the vtbl). */
      singleton->base.vtbl = &kNaClDescInvalidVtbl;
    } while (0);
  }
  NaClXMutexUnlock(mutex);
  /* If we reached this point and still have NULL == singleton there was an
   * error in allocation or construction.  Return NULL to indicate error.
   */
  if (NULL == singleton) {
    return NULL;
  }

  return (struct NaClDescInvalid *) NaClDescRef(&(singleton->base));
}

int NaClDescInvalidInternalize(struct NaClDesc           **baseptr,
                               struct NaClDescXferState  *xfer) {
  UNREFERENCED_PARAMETER(xfer);

  *baseptr = (struct NaClDesc *) NaClDescInvalidMake();

  return 0;
}
