/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NACL_BASE_NACL_REFCOUNT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NACL_BASE_NACL_REFCOUNT_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_sync.h"

struct NaClRefCountVtbl;

struct NaClRefCount {
  struct NaClRefCountVtbl const *vtbl;
  /* private */
  struct NaClMutex              mu;
  size_t                        ref_count;
};

struct NaClRefCountVtbl {
  void (*Dtor)(struct NaClRefCount  *vself);
};

/*
 * Placement new style ctor; creates w/ ref_count of 1.
 *
 * The subclasses' ctor must call this base class ctor during their
 * contruction.
 */
int NaClRefCountCtor(struct NaClRefCount *nrcp) NACL_WUR;

struct NaClRefCount *NaClRefCountRef(struct NaClRefCount *nrcp);

/* when ref_count reaches zero, will call dtor and free */
void NaClRefCountUnref(struct NaClRefCount *nrcp);

/*
 * NaClRefCountSafeUnref is just like NaCRefCountUnref, except that
 * nrcp may be NULL (in which case this is a noop).
 *
 * Used in failure cleanup of initialization code, esp in Ctors that
 * can fail.
 */
void NaClRefCountSafeUnref(struct NaClRefCount *nrcp);

extern struct NaClRefCountVtbl const kNaClRefCountVtbl;

#endif
