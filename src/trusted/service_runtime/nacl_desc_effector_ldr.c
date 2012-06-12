/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* @file
 *
 * Implementation of service runtime effector subclass used for all
 * application threads.
 */
#include "native_client/src/shared/platform/nacl_host_desc.h"

#include "native_client/src/trusted/service_runtime/nacl_desc_effector_ldr.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_memory_object.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"


static struct NaClDescEffectorVtbl const NaClDescEffectorLdrVtbl;  /* fwd */

int NaClDescEffectorLdrCtor(struct NaClDescEffectorLdr *self,
                            struct NaClApp             *nap) {
  self->base.vtbl = &NaClDescEffectorLdrVtbl;
  self->nap = nap;
  return 1;
}

#if NACL_WINDOWS
static void NaClDescEffLdrUnmapMemory(struct NaClDescEffector  *vself,
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
    usraddr = NaClSysToUser(self->nap, addr);

    map_region = NaClVmmapFindPage(&self->nap->mem_map,
                                   usraddr >> NACL_PAGESHIFT);
    if (NULL == map_region || NULL == map_region->nmop) {
      /*
       * No memory in address space, and we have only MEM_RESERVE'd
       * the address space; or memory is in address space, but not
       * backed by a file.
       */
      if (0 == VirtualFree((void *) addr, 0, MEM_RELEASE)) {
        NaClLog(LOG_FATAL,
                ("NaClMMap: VirtualFree at user addr 0x%08"NACL_PRIxPTR
                 " (sys 0x%08"NACL_PRIxPTR") failed: windows error %d\n"),
                usraddr,
                addr,
                GetLastError());
      }
    } else {
      NaClDescUnmapUnsafe(map_region->nmop->ndp,
                          (void *) addr, NACL_MAP_PAGESIZE);
    }
  }
}

#else  /* NACL_WINDOWS */

static void NaClDescEffLdrUnmapMemory(struct NaClDescEffector  *vself,
                                      uintptr_t                sysaddr,
                                      size_t                   nbytes) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
}
#endif  /* NACL_WINDOWS */

static struct NaClDescEffectorVtbl const NaClDescEffectorLdrVtbl = {
  NaClDescEffLdrUnmapMemory,
};
