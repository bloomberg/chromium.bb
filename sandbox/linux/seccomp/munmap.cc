// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_munmap(void* start, size_t length) {
  long long tm;
  Debug::syscall(&tm, __NR_munmap, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
    MUnmap    munmap_req;
  } __attribute__((packed)) request;
  request.sysnum            = __NR_munmap;
  request.cookie            = cookie();
  request.munmap_req.start  = start;
  request.munmap_req.length = length;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward munmap() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_munmap);
  return rc;
}

bool Sandbox::process_munmap(int parentMapsFd, int sandboxFd, int threadFdPub,
                             int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  MUnmap munmap_req;
  if (read(sys, sandboxFd, &munmap_req, sizeof(munmap_req)) !=
      sizeof(munmap_req)) {
    die("Failed to read parameters for munmap() [process]");
  }

  // Cannot unmap any memory region that was part of the original memory
  // mappings.
  int rc = -EINVAL;
  void *stop = reinterpret_cast<void *>(
      reinterpret_cast<char *>(munmap_req.start) + munmap_req.length);
  ProtectedMap::const_iterator iter = protectedMap_.lower_bound(
      munmap_req.start);
  if (iter != protectedMap_.begin()) {
    --iter;
  }
  for (; iter != protectedMap_.end() && iter->first < stop; ++iter) {
    if (munmap_req.start < reinterpret_cast<void *>(
            reinterpret_cast<char *>(iter->first) + iter->second) &&
        stop > iter->first) {
      SecureMem::abandonSystemCall(threadFd, rc);
      return false;
    }
  }

  // Unmapping memory regions that were newly mapped inside of the sandbox
  // is OK.
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem, __NR_munmap,
                            munmap_req.start, munmap_req.length);
  return true;
}

} // namespace
