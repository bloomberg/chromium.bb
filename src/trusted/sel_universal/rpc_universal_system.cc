/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#if (NACL_LINUX)
#include <sys/ipc.h>
#include <sys/shm.h>
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#endif

#include <map>
#include <string>
#include <sstream>

using std::stringstream;

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
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
#if (NACL_LINUX)
    typedef map<NaClDesc*, uintptr_t>::iterator IT;
    for (IT it = map_.begin(); it != map_.end(); ++it) {
      shmctl(reinterpret_cast<NaClDescSysvShm*>(it->first)->id, IPC_RMID, NULL);
    }
#endif
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
  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc = factory.MakeShm(size);
  if (NULL == desc) {
    NaClLog(LOG_ERROR, "cound not create shmem desc\n");
    return kNaClSrpcInvalidImcDesc;
  }

  // now map it
  void* addr;
  size_t dummy_size;
  int result = desc->Map(&addr, &dummy_size);
  if (0 > result) {
    NaClLog(LOG_ERROR, "error mapping shmem area\n");
  }

  GlobalAddressMap.Add(desc->desc(), reinterpret_cast<uintptr_t>(addr));
  return desc->desc();
}

}  // namespace

bool HandlerShmem(NaClCommandLoop* ncl, const vector<string>& args) {
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


bool HandlerReadonlyFile(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc = factory.OpenHostFile(args[2].c_str(), O_RDONLY, 0);
  if (NULL == desc) {
    NaClLog(LOG_ERROR, "cound not create file desc for %s\n", args[2].c_str());
    return false;
  }
  ncl->AddDesc(desc->desc(), args[1]);
  return true;
}


bool HandlerSleep(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 2) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }
  const int secs = strtol(args[1].c_str(), 0, 0);
#if (NACL_LINUX || NACL_OSX)
  sleep(secs);
#elif NACL_WINDOWS
  Sleep(secs * 1000);
#else
#error "Please specify platform as NACL_LINUX, NACL_OSX or NACL_WINDOWS"
#endif
  return true;
}
