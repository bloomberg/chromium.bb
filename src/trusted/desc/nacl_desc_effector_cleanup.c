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

static void NaClDescEffCleanUpdateAddrMap(struct NaClDescEffector *vself,
                                          uintptr_t               sysaddr,
                                          size_t                  nbytes,
                                          int                     sysprot,
                                          struct NaClDesc         *backing_desc,
                                          size_t                  backing_bytes,
                                          off_t                   offset_bytes,
                                          int                     delete_mem) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  UNREFERENCED_PARAMETER(sysprot);
  UNREFERENCED_PARAMETER(backing_desc);
  UNREFERENCED_PARAMETER(backing_bytes);
  UNREFERENCED_PARAMETER(offset_bytes);
  UNREFERENCED_PARAMETER(delete_mem);
  return;
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
  NaClDescEffCleanUpdateAddrMap,
  NaClDescEffCleanUnmapMemory,
  NaClDescEffCleanMapAnonMem,
  NaClDescEffCleanSourceSock,
};
