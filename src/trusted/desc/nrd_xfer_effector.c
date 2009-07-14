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
 * NRD xfer effector for trusted code use.
 */

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

#include "native_client/src/trusted/platform/nacl_log.h"

/* fwd */
static struct NaClDescEffectorVtbl NaClNrdXferEffectorVtbl;

int NaClNrdXferEffectorCtor(struct NaClNrdXferEffector  *self,
                            struct NaClDesc             *src_desc) {
  self->src_desc = src_desc;
  NaClDescRef(src_desc);
  self->out_desc = NULL;

  self->base.vtbl = &NaClNrdXferEffectorVtbl;
  return 1;
}

static void NaClNrdXferEffectorDtor(struct NaClDescEffector *vself) {
  struct NaClNrdXferEffector *self = (struct NaClNrdXferEffector *) vself;

  NaClDescUnref(self->src_desc);
  self->src_desc = NULL;
  if (NULL != self->out_desc) {
    NaClLog(LOG_WARNING,
            ("NaClNrdXferEffectorDtor: called with out_desc non-NULL,"
             " unref'ing\n"));
    NaClDescUnref(self->out_desc);
    self->out_desc = NULL;
  }
  return;
}

struct NaClDesc *NaClNrdXferEffectorTakeDesc(struct NaClNrdXferEffector *self) {
  struct NaClDesc *ndp = self->out_desc;
  self->out_desc = NULL;
  return ndp;
}

static int NaClNrdXferEffectorReturnCreatedDesc(struct NaClDescEffector *vself,
                                                struct NaClDesc         *ndp) {
  struct NaClNrdXferEffector *self = (struct NaClNrdXferEffector *) vself;
  if (NULL != self->out_desc) {
    NaClLog(LOG_WARNING,
            ("NaClNrdXferEffectorReturnCreatedDesc: "
             " got new return descriptor at 0x%08"PRIxPTR", but"
             " previously returned one at 0x%08"PRIxPTR" not retrieved"
             " (unref'ing)\n"),
            (uintptr_t) ndp,
            (uintptr_t) self->out_desc);
    NaClDescUnref(self->out_desc);
  }
  if (NULL == ndp) {
    NaClLog(LOG_WARNING,
            "NaClNrdXferEffectorReturnCreatedDesc: "
            " called with NULL returned NaClDesc object\n");
  }
  self->out_desc = ndp;
  return 0;
}

static void NaClNrdXferEffectorUpdateAddrMap(struct NaClDescEffector  *vself,
                                             uintptr_t                sysaddr,
                                             size_t                   nbytes,
                                             int                      sysprot,
                                             struct NaClDesc          *bkobj,
                                             size_t                   bksize,
                                             off_t                    offbytes,
                                             int                      del_mem) {
  /* No records are kept */
  return;
}

static int NaClNrdXferEffectorUnmapMemory(struct NaClDescEffector *vself,
                                          uintptr_t               sysaddr,
                                          size_t                  nbytes) {
  return 0;
}

static int NaClNrdXferEffectorMapAnonymousMemory(
    struct NaClDescEffector *vself,
    uintptr_t               sysaddr,
    size_t                  nbytes,
    int                     prot) {
  return 0;
}

static struct NaClDescImcBoundDesc *NaClNrdXferEffectorSourceSock(
    struct NaClDescEffector *vself) {
  struct NaClNrdXferEffector *self = (struct NaClNrdXferEffector *) vself;

  return (struct NaClDescImcBoundDesc *) self->src_desc;
  /* does not inc refcount */
}

static struct NaClDescEffectorVtbl NaClNrdXferEffectorVtbl = {
  NaClNrdXferEffectorDtor,
  NaClNrdXferEffectorReturnCreatedDesc,
  NaClNrdXferEffectorUpdateAddrMap,
  NaClNrdXferEffectorUnmapMemory,
  NaClNrdXferEffectorMapAnonymousMemory,
  NaClNrdXferEffectorSourceSock,
};
