/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_runtime_host_interface.h"

#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

int NaClRuntimeHostInterfaceCtor_protected(
    struct NaClRuntimeHostInterface *self) {
  NaClLog(4, "Entered NaClRuntimeHostInterfaceCtor_protected\n");
  if (!NaClRefCountCtor((struct NaClRefCount *) self)) {
    NaClLog(4,
            "NaClRuntimeHostInterfaceCtor_protected: "
            "NaClRefCountCtor base class ctor failed\n");
    return 0;
  }
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl const *) &kNaClRuntimeHostInterfaceVtbl;
  NaClLog(4,
          "Leaving NaClRuntimeHostInterfaceCtor_protected, returning 1\n");
  return 1;
}

void NaClRuntimeHostInterfaceDtor(struct NaClRefCount *vself) {
  struct NaClRuntimeHostInterface *self =
      (struct NaClRuntimeHostInterface *) vself;

  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClRuntimeHostInterfaceStartupInitializationCompleteNotImplemented(
    struct NaClRuntimeHostInterface *self) {
  NaClLog(LOG_ERROR,
          ("NaClRuntimeHostInterfaceStartupInitializationComplete(0x%08"
           NACL_PRIxPTR")\n"),
          (uintptr_t) self);
  return -NACL_ABI_EINVAL;
}

struct NaClRuntimeHostInterfaceVtbl const kNaClRuntimeHostInterfaceVtbl = {
  {
    NaClRuntimeHostInterfaceDtor,
  },
  NaClRuntimeHostInterfaceStartupInitializationCompleteNotImplemented,
};
