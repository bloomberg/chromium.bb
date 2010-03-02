/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* @file
 *
 * Implementation of effector subclass used only for service runtime's
 * NaClAppDtor address space tear down.
 */

#include "native_client/src/trusted/desc/nacl_desc_effector_cleanup.h"
#include "native_client/src/shared/platform/nacl_log.h"

static struct NaClDescEffectorVtbl NaClDescEffectorCleanupVtbl;  /* fwd */

int NaClDescEffectorCleanupCtor(struct NaClDescEffectorCleanup *self) {
  self->base.vtbl = &NaClDescEffectorCleanupVtbl;
  return 1;
}

static void NaClDescEffCleanDtor(struct NaClDescEffector *vself) {
  vself->vtbl = (struct NaClDescEffectorVtbl *) NULL;
  return;
}

static int NaClDescEffCleanReturnCreatedDesc(struct NaClDescEffector *vself,
                                             struct NaClDesc         *ndp) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(ndp);
  NaClLog(LOG_FATAL, "Cleanup effector's ReturnCreatedDesc called\n");
  return 0;
}

static int NaClDescEffCleanUnmapMemory(struct NaClDescEffector  *vself,
                                       uintptr_t                sysaddr,
                                       size_t                   nbytes) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  NaClLog(LOG_FATAL, "Cleanup effector's UnmapMemory called\n");
  return 0;
}

static uintptr_t NaClDescEffCleanMapAnonMem(struct NaClDescEffector *vself,
                                            uintptr_t               sysaddr,
                                            size_t                  nbytes,
                                            int                     prot) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  UNREFERENCED_PARAMETER(prot);
  NaClLog(LOG_FATAL, "Cleanup effector's MapAnonMem called\n");
  return 0;
}

static struct NaClDescImcBoundDesc *NaClDescEffCleanSourceSock(
    struct NaClDescEffector *vself) {
  UNREFERENCED_PARAMETER(vself);
  NaClLog(LOG_FATAL, "Cleanup effector's SourceSock called\n");
  return 0;
}

static struct NaClDescEffectorVtbl NaClDescEffectorCleanupVtbl = {
  NaClDescEffCleanDtor,
  NaClDescEffCleanReturnCreatedDesc,
  NaClDescEffCleanUnmapMemory,
  NaClDescEffCleanMapAnonMem,
  NaClDescEffCleanSourceSock,
};
