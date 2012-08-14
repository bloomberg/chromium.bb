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
  enum NaClVmmapEntryType     entry_type;

  for (addr = sysaddr, endaddr = sysaddr + nbytes;
       addr < endaddr;
       addr += NACL_MAP_PAGESIZE) {
    usraddr = NaClSysToUser(self->nap, addr);

    map_region = NaClVmmapFindPage(&self->nap->mem_map,
                                   usraddr >> NACL_PAGESHIFT);
    if (NULL == map_region) {
      entry_type = NACL_VMMAP_ENTRY_ANONYMOUS;
    } else {
      entry_type = map_region->vmmap_type;
    }

    switch (entry_type) {
      case NACL_VMMAP_ENTRY_ANONYMOUS:
        /*
         * No memory in address space, and we have only MEM_RESERVE'd
         * the address space; or memory is in address space, but not
         * backed by a file.
         */
        if (!VirtualFree((void *) addr, 0, MEM_RELEASE)) {
          NaClLog(LOG_FATAL,
                  ("NaClDescEffLdrUnmapMemory: VirtualFree at user addr"
                   " 0x%08"NACL_PRIxPTR" (sys 0x%08"NACL_PRIxPTR") failed:"
                   " error %d\n"),
                  usraddr, addr, GetLastError());
        }
        break;
      case NACL_VMMAP_ENTRY_MAPPED:
        if (!UnmapViewOfFile((void *) addr)) {
          NaClLog(LOG_FATAL,
                  ("NaClDescEffLdrUnmapMemory: UnmapViewOfFile failed"
                   " addr 0x%08x, error %d\n"),
                  addr, GetLastError());
        }
        break;
      default:
        NaClLog(LOG_FATAL, "NaClDescEffLdrUnmapMemory: invalid vmmap_type\n");
        break;
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
