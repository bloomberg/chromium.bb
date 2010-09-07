/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/base/nacl_refcount.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

int NaClRefCountCtor(struct NaClRefCount *nrcp) {
  nrcp->ref_count = 1;
  nrcp->vtbl = (struct NaClRefCountVtbl *) NULL;
  if (NaClMutexCtor(&nrcp->mu)) {
    nrcp->vtbl = &kNaClRefCountVtbl;
    return 1;
  }
  return 0;
}

static void NaClRefCountDtor(struct NaClRefCount *nrcp) {
  if (0 != nrcp->ref_count) {
    NaClLog(LOG_FATAL, ("NaClRefCountDtor invoked on a generic descriptor"
                        " at 0x%08"NACL_PRIxPTR" with non-zero"
                        " reference count (%"NACL_PRIdS")\n"),
            (uintptr_t) nrcp,
            nrcp->ref_count);
  }
  NaClLog(4, "NaClRefCountDtor(0x%08"NACL_PRIxPTR"), refcount 0, destroying.\n",
          (uintptr_t) nrcp);
  NaClMutexDtor(&nrcp->mu);
  nrcp->vtbl = (struct NaClRefCountVtbl const *) NULL;
}

struct NaClRefCountVtbl const kNaClRefCountVtbl = {
  NaClRefCountDtor,
};

struct NaClRefCount *NaClRefCountRef(struct NaClRefCount *nrcp) {
  NaClLog(4, "NaClRefCountRef(0x%08"NACL_PRIxPTR").\n",
          (uintptr_t) nrcp);
  NaClXMutexLock(&nrcp->mu);
  if (0 == ++nrcp->ref_count) {
    NaClLog(LOG_FATAL, "NaClRefCountRef integer overflow\n");
  }
  NaClXMutexUnlock(&nrcp->mu);
  return nrcp;
}

void NaClRefCountUnref(struct NaClRefCount *nrcp) {
  int destroy;

  NaClLog(4, "NaClRefCountUnref(0x%08"NACL_PRIxPTR").\n",
          (uintptr_t) nrcp);
  NaClXMutexLock(&nrcp->mu);
  if (0 == nrcp->ref_count) {
    NaClLog(LOG_FATAL,
            ("NaClRefCountUnref on 0x%08"NACL_PRIxPTR
             ", refcount already zero!\n"),
            (uintptr_t) nrcp);
  }
  destroy = (0 == --nrcp->ref_count);
  NaClXMutexUnlock(&nrcp->mu);
  if (destroy) {
    (*nrcp->vtbl->Dtor)(nrcp);
    free(nrcp);
  }
}

void NaClRefCountSafeUnref(struct NaClRefCount *nrcp) {
  NaClLog(4, "NaClRefCountSafeUnref(0x%08"NACL_PRIxPTR").\n",
          (uintptr_t) nrcp);
  if (NULL == nrcp) {
    return;
  }
  NaClRefCountUnref(nrcp);
}
