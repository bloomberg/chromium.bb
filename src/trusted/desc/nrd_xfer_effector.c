/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NRD xfer effector for trusted code use.
 */

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

#include "native_client/src/shared/platform/nacl_log.h"

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
             " got new return descriptor at 0x%08"NACL_PRIxPTR", but"
             " previously returned one at 0x%08"NACL_PRIxPTR" not retrieved"
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

static int NaClNrdXferEffectorUnmapMemory(struct NaClDescEffector *vself,
                                          uintptr_t               sysaddr,
                                          size_t                  nbytes) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  return 0;
}

static uintptr_t NaClNrdXferEffectorMapAnonymousMemory(
    struct NaClDescEffector *vself,
    uintptr_t               sysaddr,
    size_t                  nbytes,
    int                     prot) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  UNREFERENCED_PARAMETER(prot);
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
  NaClNrdXferEffectorUnmapMemory,
  NaClNrdXferEffectorMapAnonymousMemory,
  NaClNrdXferEffectorSourceSock,
};
