// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_exit(int status) {
  long long tm;
  Debug::syscall(&tm, __NR_exit, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
  } __attribute__((packed)) request;
  request.sysnum = __NR_exit;
  request.cookie = cookie();

  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request)) {
    die("Failed to forward exit() request [sandbox]");
  }
  for (;;) {
    sys._exit(status);
  }
}

bool Sandbox::process_exit(int parentMapsFd, int sandboxFd, int threadFdPub,
                           int threadFd, SecureMem::Args* mem) {
  SecureMem::lockSystemCall(parentMapsFd, mem);
  SecureMem::sendSystemCall(threadFdPub, true, parentMapsFd, mem,
                            __NR_exit, 0);
  return true;
}

} // namespace
