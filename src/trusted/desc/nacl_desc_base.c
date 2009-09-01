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
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.  Memory
 * mapping using descriptors.
 */

#include <stdlib.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

/*
 * This file contains base class code for NaClDesc.
 *
 * The implementation for following subclasses are elsewhere, but here
 * is an enumeration of them with a brief description:
 *
 * NaClDescIoDesc is the subclass that wraps host-OS descriptors
 * provided by NaClHostDesc (which gives an OS-independent abstraction
 * for host-OS descriptors).
 *
 * NaClDescImcDesc is the subclass that wraps IMC descriptors.
 *
 * NaClDescMutex and NaClDescCondVar are the subclasses that
 * wrap the non-transferrable synchronization objects.
 *
 * These NaClDesc objects are impure in that they know about the
 * virtual memory subsystem restriction of requiring mappings to occur
 * in NACL_MAP_PAGESIZE (64KB) chunks, so the Map and Unmap virtual
 * functions, at least, will enforce this restriction.
 */

int NaClDescCtor(struct NaClDesc *ndp) {
  /* this should be a compile-time test */
  if (0 != (sizeof(struct NaClInternalHeader) & 0xf)) {
    NaClLog(LOG_FATAL,
            "Internal error.  NaClInternalHeader size not a"
            " multiple of 16\n");
  }
  ndp->ref_count = 1;
  return NaClMutexCtor(&ndp->mu);
}

void NaClDescDtor(struct NaClDesc *ndp) {
  if (0 != ndp->ref_count) {
    NaClLog(LOG_FATAL, ("NaClDescDtor invoked on a generic descriptor"
                        " at 0x%08"PRIxPTR" with non-zero"
                        " reference count (%d)\n"),
            (uintptr_t) ndp,
            ndp->ref_count);
  }
  NaClLog(4, "NaClDescDtor(0x%08"PRIxPTR"), refcount 0, destroying.\n",
          (uintptr_t) ndp);
  NaClMutexDtor(&ndp->mu);
}

struct NaClDesc *NaClDescRef(struct NaClDesc *ndp) {
  NaClLog(4, "NaClDescRef(0x%08"PRIxPTR").\n",
          (uintptr_t) ndp);
  NaClXMutexLock(&ndp->mu);
  if (0 == ++ndp->ref_count) {
    NaClLog(LOG_FATAL, "NaClDescRef integer overflow\n");
  }
  NaClXMutexUnlock(&ndp->mu);
  return ndp;
}

void NaClDescUnref(struct NaClDesc *ndp) {
  int destroy;

  NaClLog(4, "NaClDescUnref(0x%08"PRIxPTR").\n",
          (uintptr_t) ndp);
  NaClXMutexLock(&ndp->mu);
  if (0 == ndp->ref_count) {
    NaClLog(LOG_FATAL,
            "NaClDescUnref on 0x%08"PRIxPTR", refcount already zero!\n",
            (uintptr_t) ndp);
  }
  destroy = (0 == --ndp->ref_count);
  NaClXMutexUnlock(&ndp->mu);
  if (destroy) {
    (*ndp->vtbl->Dtor)(ndp);
    free(ndp);
  }
}

int (*NaClDescInternalize[NACL_DESC_TYPE_MAX])(struct NaClDesc **,
                                               struct NaClDescXferState *) = {
  NaClDescDirInternalize,
  NaClDescIoInternalize,
  NaClDescConnCapInternalize,
  NaClDescImcBoundDescInternalize,
  NaClDescInternalizeNotImplemented,  /* connected abstract base class */
  NaClDescImcShmInternalize,
  NaClDescMutexInternalize,
  NaClDescCondVarInternalize,
  NaClDescInternalizeNotImplemented,  /* semaphore */
  NaClDescXferableDataDescInternalize,
  NaClDescInternalizeNotImplemented,  /* imc socket */
};

char const *NaClDescTypeString(enum NaClDescTypeTag type_tag) {
  /* default functions for the vtable - return NOT_IMPLEMENTED */
  switch (type_tag) {
#define MAP(E) case E: do { return #E; } while (0)
    MAP(NACL_DESC_DIR);
    MAP(NACL_DESC_HOST_IO);
    MAP(NACL_DESC_CONN_CAP);
    MAP(NACL_DESC_BOUND_SOCKET);
    MAP(NACL_DESC_CONNECTED_SOCKET);
    MAP(NACL_DESC_SHM);
    MAP(NACL_DESC_MUTEX);
    MAP(NACL_DESC_CONDVAR);
    MAP(NACL_DESC_SEMAPHORE);
    MAP(NACL_DESC_TRANSFERABLE_DATA_SOCKET);
    MAP(NACL_DESC_IMC_SOCKET);
  }
  return "BAD TYPE TAG";
}
void NaClDescDtorNotImplemented(struct NaClDesc  *vself) {
  NaClLog(LOG_FATAL, "Must implement a destructor!\n");
}

uintptr_t NaClDescMapNotImplemented(struct NaClDesc         *vself,
                                    struct NaClDescEffector *effp,
                                    void                    *start_addr,
                                    size_t                  len,
                                    int                     prot,
                                    int                     flags,
                                    off_t                   offset) {
  NaClLog(LOG_ERROR,
          "Map method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescUnmapUnsafeNotImplemented(struct NaClDesc         *vself,
                                      struct NaClDescEffector *effp,
                                      void                    *start_addr,
                                      size_t                  len) {
  NaClLog(LOG_ERROR,
    "Map method is not implemented for object of type %s\n",
    NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescUnmapNotImplemented(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp,
                                void                    *start_addr,
                                size_t                  len) {
  NaClLog(LOG_ERROR,
          "Unmap method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

ssize_t NaClDescReadNotImplemented(struct NaClDesc          *vself,
                                   struct NaClDescEffector  *effp,
                                   void                     *buf,
                                   size_t                   len) {
  NaClLog(LOG_ERROR,
          "Read method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

ssize_t NaClDescWriteNotImplemented(struct NaClDesc         *vself,
                                    struct NaClDescEffector *effp,
                                    void const              *buf,
                                    size_t                  len) {
  NaClLog(LOG_ERROR,
          "Write method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescSeekNotImplemented(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp,
                               off_t                    offset,
                               int                      whence) {
  NaClLog(LOG_ERROR,
          "Seek method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescIoctlNotImplemented(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp,
                                int                     request,
                                void                    *arg) {
  NaClLog(LOG_ERROR,
          "Ioctl method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescFstatNotImplemented(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp,
                                struct nacl_abi_stat    *statbuf) {
  NaClLog(LOG_ERROR,
          "Fstat method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescCloseNotImplemented(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR,
          "Close method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

ssize_t NaClDescGetdentsNotImplemented(struct NaClDesc          *vself,
                                       struct NaClDescEffector  *effp,
                                       void                     *dirp,
                                       size_t                   count) {
  NaClLog(LOG_ERROR,
          "Getdents method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescExternalizeSizeNotImplemented(struct NaClDesc      *vself,
                                          size_t               *nbytes,
                                          size_t               *nhandles) {
  NaClLog(LOG_ERROR,
          "ExternalizeSize method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescExternalizeNotImplemented(struct NaClDesc          *vself,
                                      struct NaClDescXferState *xfer) {
  NaClLog(LOG_ERROR,
          "Externalize method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescLockNotImplemented(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp) {
  NaClLog(LOG_ERROR,
          "Lock method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescTryLockNotImplemented(struct NaClDesc         *vself,
                                  struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR,
          "TryLock method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescUnlockNotImplemented(struct NaClDesc          *vself,
                                 struct NaClDescEffector  *effp) {
  NaClLog(LOG_ERROR,
          "Unlock method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescWaitNotImplemented(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp,
                               struct NaClDesc          *mutex) {
  NaClLog(LOG_ERROR,
          "Wait method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescTimedWaitAbsNotImplemented(struct NaClDesc          *vself,
                                       struct NaClDescEffector  *effp,
                                       struct NaClDesc          *mutex,
                                       struct nacl_abi_timespec *ts) {
  NaClLog(LOG_ERROR,
          "TimedWaitAbs method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescSignalNotImplemented(struct NaClDesc          *vself,
                                 struct NaClDescEffector  *effp) {
  NaClLog(LOG_ERROR,
          "Signal method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescBroadcastNotImplemented(struct NaClDesc         *vself,
                                    struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR,
          "Broadcast method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescSendMsgNotImplemented(struct NaClDesc           *vself,
                                  struct NaClDescEffector   *effp,
                                  struct NaClMessageHeader  *dgram,
                                  int                       flags) {
  NaClLog(LOG_ERROR,
          "SendMsg method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescRecvMsgNotImplemented(struct NaClDesc           *vself,
                                  struct NaClDescEffector   *effp,
                                  struct NaClMessageHeader  *dgram,
                                  int                       flags) {
  NaClLog(LOG_ERROR,
          "RecvMsg method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescConnectAddrNotImplemented(struct NaClDesc         *vself,
                                      struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR,
          "ConnectAddr method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescAcceptConnNotImplemented(struct NaClDesc          *vself,
                                     struct NaClDescEffector  *effp) {
  NaClLog(LOG_ERROR,
          "AcceptConn method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescPostNotImplemented(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp) {
  NaClLog(LOG_ERROR,
          "Post method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescSemWaitNotImplemented(struct NaClDesc         *vself,
                                  struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR,
          "SemWait method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescGetValueNotImplemented(struct NaClDesc          *vself,
                                   struct NaClDescEffector  *effp) {
  NaClLog(LOG_ERROR,
          "GetValue method is not implemented for object of type %s\n",
          NaClDescTypeString(vself->vtbl->typeTag));
  return -NACL_ABI_EINVAL;
}

int NaClDescInternalizeNotImplemented(struct NaClDesc **baseptr,
                                      struct NaClDescXferState *xfer) {
  NaClLog(LOG_ERROR,
          "Attempted transfer of non-transferable descriptor\n");
  return -NACL_ABI_EIO;
}

int NaClDescMapDescriptor(struct NaClDesc *desc,
                          struct NaClDescEffector *effector,
                          void** addr,
                          size_t* size) {
  struct nacl_abi_stat st;
  size_t rounded_size = 0;
  const int kMaxTries = 10;
  int tries = 0;
  void* map_addr = NULL;
  int rval;
  uintptr_t rval_ptr;

  *addr = NULL;
  *size = 0;

  rval = desc->vtbl->Fstat(desc,
                           effector,
                           &st);
  if (0 != rval) {
    /* Failed to get the size - return failure. */
    return rval;
  }

  /*
   * When probing by VirtualAlloc/mmap, use the same granularity
   * as the Map virtual function (64KB).
   */
  rounded_size = NaClRoundAllocPage(st.nacl_abi_st_size);

  /* Find an address range to map the object into. */
  do {
    ++tries;
#if NACL_WINDOWS
    map_addr = VirtualAlloc(NULL, rounded_size, MEM_RESERVE, PAGE_READWRITE);
    if (NULL == map_addr ||!VirtualFree(map_addr, 0, MEM_RELEASE)) {
      continue;
    }
#else
    map_addr = mmap(NULL,
                    rounded_size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS,
                    0,
                    0);
    if (MAP_FAILED == map_addr || munmap(map_addr, rounded_size)) {
      map_addr = NULL;
      continue;
    }
#endif
    rval = desc->vtbl->Map(desc,
                           effector,
                           map_addr,
                           rounded_size,
                           NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                           NACL_ABI_MAP_SHARED,
                           0);
    if (!NaClIsNegErrno(rval)) {
      rval_ptr = (uintptr_t) rval;
      map_addr = (void*)(rval_ptr);
      break;
    }
  } while (NULL == map_addr && tries < kMaxTries);

  if (NULL == map_addr) {
    return 1;
  }

  *addr = map_addr;
  *size = rounded_size;
  return 0;
}
