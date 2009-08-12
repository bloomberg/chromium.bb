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
 * NaCl Service Runtime.  Directory descriptor abstraction.
 */

#include "native_client/src/include/portability.h"

#include <stdlib.h>

#include "native_client/src/shared/imc/nacl_imc_c.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_dir.h"

#include "native_client/src/shared/platform/nacl_host_dir.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#include "native_client/src/trusted/service_runtime/include/sys/dirent.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"


/*
 * This file contains the implementation for the NaClDirDesc subclass
 * of NaClDesc.
 *
 * NaClDescDirDesc is the subclass that wraps host-OS directory information.
 */

/*
 * Takes ownership of hd, will close in Dtor.
 */
int NaClDescDirDescCtor(struct NaClDescDirDesc  *self,
                        struct NaClHostDir      *hd) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  self->hd = hd;
  basep->vtbl = &kNaClDescDirDescVtbl;
  return 1;
}

void NaClDescDirDescDtor(struct NaClDesc *vself) {
  struct NaClDescDirDesc *self = (struct NaClDescDirDesc *) vself;

  NaClLog(4, "NaClDescDirDescDtor(0x%08"PRIxPTR").\n",
          (uintptr_t) vself);
  NaClHostDirClose(self->hd);
  free(self->hd);
  self->hd = NULL;
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(&self->base);
}

struct NaClDescDirDesc *NaClDescDirDescMake(struct NaClHostDir *nhdp) {
  struct NaClDescDirDesc *ndp;

  ndp = malloc(sizeof *ndp);
  if (NULL == ndp) {
    NaClLog(LOG_FATAL,
            "NaClDescDirDescMake: no memory for 0x%08"PRIxPTR"\n",
            (uintptr_t) nhdp);
  }
  if (!NaClDescDirDescCtor(ndp, nhdp)) {
    NaClLog(LOG_FATAL,
            ("NaClDescDirDescMake:"
             " NaClDescDirDescCtor(0x%08"PRIxPTR",0x%08"PRIxPTR") failed\n"),
            (uintptr_t) ndp,
            (uintptr_t) nhdp);
  }
  return ndp;
}

struct NaClDescDirDesc *NaClDescDirDescOpen(char  *path) {
  struct NaClHostDir  *nhdp;

  nhdp = malloc(sizeof *nhdp);
  if (NULL == nhdp) {
    NaClLog(LOG_FATAL, "NaClDescDirDescOpen: no memory for %s\n", path);
  }
  if (!NaClHostDirOpen(nhdp, path)) {
    NaClLog(LOG_FATAL, "NaClDescDirDescOpen: NaClHostDirOpen failed for %s\n",
            path);
  }
  return NaClDescDirDescMake(nhdp);
}

ssize_t NaClDescDirDescRead(struct NaClDesc         *vself,
                            struct NaClDescEffector *effp,
                            void                    *buf,
                            size_t                  len) {
  /* NaClLog(LOG_ERROR, "NaClDescDirDescRead: Read not allowed on dir\n"); */
  return NaClDescDirDescGetdents(vself, effp, buf, len);
  /* return -NACL_ABI_EINVAL; */
}

ssize_t NaClDescDirDescGetdents(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp,
                                void                    *dirp,
                                size_t                  count) {
  struct NaClDescDirDesc *self = (struct NaClDescDirDesc *) vself;
  struct nacl_abi_dirent *direntp = (struct nacl_abi_dirent *) dirp;
  ssize_t retval;

  NaClLog(3, "NaClDescDirDescGetdents(0x%08"PRIxPTR", %"PRIuS"):\n",
          (uintptr_t) dirp, count);
  retval = NaClHostDirGetdents(self->hd, dirp, count);
  NaClLog(3,
          "NaClDescDirDescGetdents(d_ino=%"PRIuNACL_INO
          ", d_off=%"PRIuNACL_OFF", d_reclen=%u, "
          "d_name='%s')\n",
          direntp->nacl_abi_d_ino,
          direntp->nacl_abi_d_off,
          direntp->nacl_abi_d_reclen,
          direntp->nacl_abi_d_name);
  return retval;
}

int NaClDescDirDescClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp) {
  NaClDescUnref(vself);
  return 0;
}

int NaClDescDirDescExternalizeSize(struct NaClDesc *vself,
                                   size_t          *nbytes,
                                   size_t          *nhandles) {
  *nbytes = 0;
  *nhandles = 1;
  return 0;
}

struct NaClDescVtbl const kNaClDescDirDescVtbl = {
  NaClDescDirDescDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescDirDescRead,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescFstatNotImplemented,
  NaClDescDirDescClose,
  NaClDescDirDescGetdents,
  NACL_DESC_DIR,
  NaClDescDirDescExternalizeSize,
  NaClDescExternalizeNotImplemented,
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

int NaClDescDirInternalize(struct NaClDesc           **baseptr,
                           struct NaClDescXferState  *xfer) {
  NaClLog(LOG_ERROR, "NaClDescDirDescInternalize: not implemented for dir\n");
  return -NACL_ABI_EINVAL;
}
