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
 * Implementation of service runtime effector subclass used for all
 * application threads.
 */
#include "native_client/src/shared/platform/nacl_host_desc.h"

#include "native_client/src/trusted/desc/nacl_desc_effector_ldr.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_memory_object.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"


static struct NaClDescEffectorVtbl NaClDescEffectorLdrVtbl;  /* fwd */

int NaClDescEffectorLdrCtor(struct NaClDescEffectorLdr *self,
                            struct NaClAppThread       *natp) {
  self->base.vtbl = &NaClDescEffectorLdrVtbl;
  self->natp = natp;
  return 1;
}

static void NaClDescEffLdrDtor(struct NaClDescEffector *vself) {
  struct NaClDescEffectorLdr *self = (struct NaClDescEffectorLdr *) vself;

  self->natp = NULL;
  /* we did not take ownership of natp */
  vself->vtbl = (struct NaClDescEffectorVtbl *) NULL;
}

static int NaClDescEffLdrReturnCreatedDesc(struct NaClDescEffector *vself,
                                           struct NaClDesc         *ndp) {
  struct NaClDescEffectorLdr  *self = (struct NaClDescEffectorLdr *) vself;
  int                         d;

  NaClLog(4,
          "NaClDescEffLdrReturnCreatedDesc(0x%08"PRIxPTR", 0x%08"PRIxPTR")\n",
          (uintptr_t) vself,
          (uintptr_t) ndp);
  d = NaClSetAvail(self->natp->nap, ndp);
  NaClLog(4, " returning %d\n", d);
  return d;
}

static void NaClDescEffLdrUpdateAddrMap(struct NaClDescEffector *vself,
                                        uintptr_t               sysaddr,
                                        size_t                  nbytes,
                                        int                     sysprot,
                                        struct NaClDesc         *backing_desc,
                                        size_t                  backing_bytes,
                                        off_t                   offset_bytes,
                                        int                     delete_mem) {
  struct NaClDescEffectorLdr  *self = (struct NaClDescEffectorLdr *) vself;

  NaClCommonUtilUpdateAddrMap(self->natp, sysaddr, nbytes, sysprot,
                              backing_desc, backing_bytes, offset_bytes,
                              delete_mem);
}

#if NACL_WINDOWS
static int NaClDescEffLdrUnmapMemory(struct NaClDescEffector  *vself,
                                     uintptr_t                sysaddr,
                                     size_t                   nbytes) {
  struct NaClDescEffectorLdr  *self = (struct NaClDescEffectorLdr *) vself;
  uintptr_t                   addr;
  uintptr_t                   endaddr;
  uintptr_t                   usraddr;
  struct NaClVmmapEntry const *map_region;

  for (addr = sysaddr, endaddr = sysaddr + nbytes;
       addr < endaddr;
       addr += NACL_MAP_PAGESIZE) {
    usraddr = NaClSysToUser(self->natp->nap, addr);

    map_region = NaClVmmapFindPage(&self->natp->nap->mem_map,
                                   usraddr >> NACL_PAGESHIFT);
    if (NULL == map_region || NULL == map_region->nmop) {
      /*
       * No memory in address space, and we have only MEM_RESERVE'd
       * the address space; or memory is in address space, but not
       * backed by a file.
       */
      if (0 == VirtualFree((void *) addr, 0, MEM_RELEASE)) {
        NaClLog(LOG_FATAL,
                ("NaClMMap: VirtualFree at user addr 0x%08"PRIxPTR
                 " (sys 0x%08"PRIxPTR") failed: windows error %d\n"),
                usraddr,
                addr,
                GetLastError());
      }
    } else {
      struct NaClDesc *backing_ndp;
      int             retval;

      backing_ndp = map_region->nmop->ndp;

      /*
       * UnmapUnsafe will never invoke the UpdateAddrMap method.
       */
      retval = (*backing_ndp->vtbl->UnmapUnsafe)(backing_ndp,
                                                 vself,
                                                 (void *) addr,
                                                 NACL_MAP_PAGESIZE);
      if (0 != retval) {
        NaClLog(LOG_FATAL,
                ("NaClMMap: UnmapUnsafe failed at user addr 0x%08"PRIxPTR
                 " (sys 0x%08"PRIxPTR") failed: syscall return %d\n"),
                addr,
                NaClUserToSys(self->natp->nap, addr),
                retval);
      }
    }
  }

  return 0;
}

#else  /* NACL_WINDOWS */

static int NaClDescEffLdrUnmapMemory(struct NaClDescEffector  *vself,
                                     uintptr_t                sysaddr,
                                     size_t                   nbytes) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  return 0;
}
#endif  /* NACL_WINDOWS */

static uintptr_t NaClDescEffLdrMapAnonMem(struct NaClDescEffector *vself,
                                          uintptr_t               sysaddr,
                                          size_t                  nbytes,
                                          int                     prot) {
  UNREFERENCED_PARAMETER(vself);
  return NaClHostDescMap((struct NaClHostDesc *) NULL,
                         (void *) sysaddr,
                         nbytes,
                         prot,
                         (NACL_ABI_MAP_PRIVATE |
                          NACL_ABI_MAP_ANONYMOUS |
                          NACL_ABI_MAP_FIXED),
                         (off_t) 0);
}

static struct NaClDescImcBoundDesc *NaClDescEffLdrSourceSock(
    struct NaClDescEffector *vself) {
  struct NaClDescEffectorLdr  *self = (struct NaClDescEffectorLdr *) vself;

  return (struct NaClDescImcBoundDesc *) self->natp->nap->service_port;
}

static struct NaClDescEffectorVtbl NaClDescEffectorLdrVtbl = {
  NaClDescEffLdrDtor,
  NaClDescEffLdrReturnCreatedDesc,
  NaClDescEffLdrUpdateAddrMap,
  NaClDescEffLdrUnmapMemory,
  NaClDescEffLdrMapAnonMem,
  NaClDescEffLdrSourceSock,
};
