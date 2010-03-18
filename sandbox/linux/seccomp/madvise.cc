// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_madvise(void* start, size_t length, int advice) {
  long long tm;
  Debug::syscall(&tm, __NR_madvise, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
    MAdvise   madvise_req;
  } __attribute__((packed)) request;
  request.sysnum             = __NR_madvise;
  request.cookie             = cookie();
  request.madvise_req.start  = start;
  request.madvise_req.len    = length;
  request.madvise_req.advice = advice;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward madvise() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_madvise);
  return static_cast<int>(rc);
}

bool Sandbox::process_madvise(int parentMapsFd, int sandboxFd, int threadFdPub,
                              int threadFd, SecureMem::Args* mem) {
  // Read request
  MAdvise madvise_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &madvise_req, sizeof(madvise_req)) !=
      sizeof(madvise_req)) {
    die("Failed to read parameters for madvise() [process]");
  }
  int rc = -EINVAL;
  switch (madvise_req.advice) {
    case MADV_NORMAL:
    case MADV_RANDOM:
    case MADV_SEQUENTIAL:
    case MADV_WILLNEED:
    ok:
      SecureMem::sendSystemCall(threadFdPub, false, -1, mem, __NR_madvise,
                                madvise_req.start, madvise_req.len,
                                madvise_req.advice);
      return true;
    default:
      // All other flags to madvise() are potential dangerous (as opposed to
      // merely affecting overall performance). Do not allow them on memory
      // ranges that were part of the original mappings.
      void *stop = reinterpret_cast<void *>(
          (char *)madvise_req.start + madvise_req.len);
      ProtectedMap::const_iterator iter = protectedMap_.lower_bound(
          (void *)madvise_req.start);
      if (iter != protectedMap_.begin()) {
        --iter;
      }
      for (; iter != protectedMap_.end() && iter->first < stop; ++iter) {
        if (madvise_req.start < reinterpret_cast<void *>(
                reinterpret_cast<char *>(iter->first) + iter->second) &&
            stop > iter->first) {
          SecureMem::abandonSystemCall(threadFd, rc);
          return false;
        }
      }

      // Changing attributes on memory regions that were newly mapped inside of
      // the sandbox is OK.
      goto ok;
  }
}

} // namespace
