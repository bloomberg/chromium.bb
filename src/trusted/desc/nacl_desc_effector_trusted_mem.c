/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* @file
 *
 * Implementation of effector subclass used only for service runtime's
 * gio_shm object for mapping/unmapping shared memory in *trusted*
 * address space.
 */

#include "native_client/src/trusted/desc/nacl_desc_effector_trusted_mem.h"
#include "native_client/src/shared/platform/nacl_log.h"

static struct NaClDescEffectorVtbl NaClDescEffectorTrustedMemVtbl;  /* fwd */

int NaClDescEffectorTrustedMemCtor(struct NaClDescEffectorTrustedMem *self) {
  self->base.vtbl = &NaClDescEffectorTrustedMemVtbl;
  return 1;
}

static void NaClDescEffTrustedMemDtor(struct NaClDescEffector *vself) {
  vself->vtbl = (struct NaClDescEffectorVtbl *) NULL;
  return;
}

static int NaClDescEffTrustedMemReturnCreatedDesc(
    struct NaClDescEffector *vself,
    struct NaClDesc         *ndp) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(ndp);
  NaClLog(LOG_FATAL, "TrustedMem effector's ReturnCreatedDesc called\n");
  return 0;
}

static int NaClDescEffTrustedMemUnmapMemory(struct NaClDescEffector  *vself,
                                            uintptr_t                sysaddr,
                                            size_t                   nbytes) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  NaClLog(4, "TrustedMem effector's UnmapMemory called, nothing to do\n");
  return 0;
}

static uintptr_t NaClDescEffTrustedMemMapAnonMem(
    struct NaClDescEffector *vself,
    uintptr_t               sysaddr,
    size_t                  nbytes,
    int                     prot) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  UNREFERENCED_PARAMETER(prot);
  NaClLog(LOG_FATAL, "TrustedMem effector's MapAnonMem called\n");
  return 0;
}

static struct NaClDescImcBoundDesc *NaClDescEffTrustedMemSourceSock(
    struct NaClDescEffector *vself) {
  UNREFERENCED_PARAMETER(vself);
  NaClLog(LOG_FATAL, "TrustedMem effector's SourceSock called\n");
  return 0;
}

static struct NaClDescEffectorVtbl NaClDescEffectorTrustedMemVtbl = {
  NaClDescEffTrustedMemDtor,
  NaClDescEffTrustedMemReturnCreatedDesc,
  NaClDescEffTrustedMemUnmapMemory,
  NaClDescEffTrustedMemMapAnonMem,
  NaClDescEffTrustedMemSourceSock,
};
