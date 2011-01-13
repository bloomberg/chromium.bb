/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
// TODO(bsy): including src/trusted/service_runtime/include/sys/mman.h
//     causes C++ compiler errors:
//     "declaration of 'int munmap(void*, size_t)' throws different exceptions"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

#include "native_client/src/trusted/sel_universal/rpc_universal.h"



static void* kSysvShmAddr = (void*) (intptr_t) -1;
static struct NaClDescSysvShm* shm_desc = NULL;


static void RemoveShmId() {
  if (NULL != shm_desc) {
    shmctl(shm_desc->id, IPC_RMID, NULL);
  }
}

static NaClSrpcImcDescType SysvShmDesc() {
  const size_t k64KBytes = 0x10000;
  const size_t kShmSize = k64KBytes;
  const char* kInitString = "Hello SysV shared memory.";
  void* mapaddr = MAP_FAILED;
  uintptr_t aligned;
  void* aligned_mapaddr = MAP_FAILED;

  /* Allocate a descriptor node. */
  shm_desc = static_cast<NaClDescSysvShm*>(malloc(sizeof *shm_desc));
  if (NULL == shm_desc) {
    goto cleanup;
  }
  /* Construct the descriptor with a new shared memory region. */
  if (!NaClDescSysvShmCtor(shm_desc, (nacl_off64_t) kShmSize)) {
    free(shm_desc);
    shm_desc = NULL;
    goto cleanup;
  }
  /* register free of shm id */
  atexit(RemoveShmId);
  /*
   * Find a hole to map the shared memory into.  Since we need a 64K aligned
   * address, we begin by allocating 64K more than we need, then we map to
   * an aligned address within that region.
   */
  mapaddr = mmap(NULL,
                 kShmSize + k64KBytes,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1,
                 0);
  if (MAP_FAILED == mapaddr) {
    goto cleanup;
  }
  /* Round mapaddr to next 64K. */
  aligned = ((uintptr_t) mapaddr + kShmSize - 1) & kShmSize;
  /* Map the aligned region. */
  aligned_mapaddr = mmap((void*) aligned,
                         kShmSize,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE |MAP_ANONYMOUS,
                         -1,
                         0);
  if (MAP_FAILED == aligned_mapaddr) {
    goto cleanup;
  }
  /*
   * Attach to the region.  There is no explicit detach, because the Linux
   * man page says one will be done at process exit.
   */
  kSysvShmAddr = (void *) (*((struct NaClDescVtbl *)
                             ((struct NaClRefCount *) shm_desc)->vtbl)->Map)(
      (struct NaClDesc *) shm_desc,
      (struct NaClDescEffector*) NULL,
      aligned_mapaddr,
      kShmSize,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
      0);
  if (aligned_mapaddr != kSysvShmAddr) {
    goto cleanup;
  }
  /* Initialize the region with a string for comparisons. */
  strcpy(static_cast<char*>(kSysvShmAddr), kInitString);
  /* Return successfully created descriptor. */
  return (NaClSrpcImcDescType) shm_desc;

 cleanup:
  if (MAP_FAILED != aligned_mapaddr) {
    munmap(aligned_mapaddr, kShmSize);
  }
  if (MAP_FAILED != mapaddr) {
    munmap(mapaddr, kShmSize + k64KBytes);
  }
  NaClDescUnref((struct NaClDesc*) shm_desc);
  return kNaClSrpcInvalidImcDesc;
}


bool HandleSysv(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 2) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }
  ncl->AddDesc(SysvShmDesc(), args[1]);
  return true;
}
