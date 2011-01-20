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
#include <unistd.h>
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"

#include <map>
#include <string>
#include <sstream>

using std::stringstream;

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


namespace {

const uintptr_t k64KBytes = 0x10000;

// The main point of this class is to ensure automatic cleanup.
// If the destructor is not invoked you need to manually cleanup
// the shared memory descriptors via "ipcs -m" and "ipcrm -m <id>"
class AddressMap {
 public:
  AddressMap() {}

  ~AddressMap() {
    // NOTE: you CANNOT call NaClLog - this is called too late
    // NaClLog(1, "cleanup\n");
    typedef map<NaClDesc*, uintptr_t>::iterator IT;
    for (IT it = map_.begin(); it != map_.end(); ++it) {
      shmctl(reinterpret_cast<NaClDescSysvShm*>(it->first)->id, IPC_RMID, NULL);
    }
  }

  void Add(NaClDesc* desc, uintptr_t addr) { map_[desc] = addr; }

  uintptr_t Get(NaClDesc* desc) { return map_[desc]; }

 private:
  map<NaClDesc*, uintptr_t> map_;
};

AddressMap GlobalAddressMap;

uintptr_t Align(uintptr_t start, uintptr_t alignment) {
  return (start + alignment - 1) & ~(alignment - 1);
}

NaClDesc* SysvShmDesc(int size) {
  // small leak
  NaClDescSysvShm* shm_desc = new NaClDescSysvShm;
  // alias to prevent several typecasts
  NaClDesc* desc = reinterpret_cast<NaClDesc*>(shm_desc);

  /* Construct the descriptor with a new shared memory region. */
  if (!NaClDescSysvShmCtor(shm_desc, (nacl_off64_t) size)) {
    NaClLog(LOG_ERROR, "could not allocate nacl shm\n");
    return kNaClSrpcInvalidImcDesc;
  }

  // Find a hole to map the shared memory into.  Since we need a 64K aligned
  // address, we begin by allocating 64K more than we need, then we map to
  // an aligned address within that region.
  const void* mapaddr = mmap(NULL,
                             size + k64KBytes,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS,
                             -1,
                             0);
  if (MAP_FAILED == mapaddr) {
    NaClLog(LOG_ERROR, "could not mmap local address range\n");
    NaClDescUnref(desc);
    return kNaClSrpcInvalidImcDesc;
  } else {
    NaClLog(1, "containing area start: %p\n", mapaddr);
  }

  // Round mapaddr to next 64K
  const uintptr_t aligned = Align((uintptr_t) mapaddr, k64KBytes);
  NaClLog(1, "aligned area start: %p\n", (void*) aligned);

  // Attach to the region.  There is no explicit detach, because the Linux
  // man page says one will be done at process exit.
  const NaClDescVtbl* vtable = reinterpret_cast<const NaClDescVtbl*>(
    reinterpret_cast<NaClRefCount*>(desc)->vtbl);

  const uintptr_t actual = vtable->Map(
      desc,
      reinterpret_cast<NaClDescEffector*>(NULL),
      reinterpret_cast<void*>(aligned),
      size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
      0);

  if (aligned != actual) {
    NaClLog(LOG_ERROR, "did not map to expected address\n");
    NaClDescUnref(desc);
    return kNaClSrpcInvalidImcDesc;
  }

  // Record successfully created descriptor for cleanup
  GlobalAddressMap.Add(desc, actual);
  return desc;
}

}  // namespace

bool HandlerSysv(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 4) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const int size = strtol(args[3].c_str(), 0, 0);
  NaClDesc* desc = SysvShmDesc(size);
  if (kNaClSrpcInvalidImcDesc == desc) {
    NaClLog(LOG_ERROR, "cound not create shm\n");
    return false;
  }
  ncl->AddDesc(desc, args[1]);
  stringstream str;
  str << "0x" << std::hex << GlobalAddressMap.Get(desc);
  ncl->SetVariable(args[2], str.str());
  return true;
}


bool HandlerSleep(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 2) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  sleep(strtol(args[1].c_str(), 0, 0));
  return true;
}
