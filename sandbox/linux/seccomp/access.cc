// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_access(const char *pathname, int mode) {
  long long tm;
  Debug::syscall(&tm, __NR_access, "Executing handler");
  size_t len                      = strlen(pathname);
  struct Request {
    int       sysnum;
    long long cookie;
    Access    access_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                         = reinterpret_cast<struct Request*>(data);
  request->sysnum                 = __NR_access;
  request->cookie                 = cookie();
  request->access_req.path_length = len;
  request->access_req.mode        = mode;
  memcpy(request->pathname, pathname, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward access() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_access);
  return rc;
}

bool Sandbox::process_access(int parentMapsFd, int sandboxFd, int threadFdPub,
                           int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  Access access_req;
  if (read(sys, sandboxFd, &access_req, sizeof(access_req)) !=
      sizeof(access_req)) {
 read_parm_failed:
    die("Failed to read parameters for access() [process]");
  }
  int   rc                    = -ENAMETOOLONG;
  if (access_req.path_length >= sizeof(mem->pathname)) {
    char buf[32];
    while (access_req.path_length > 0) {
      size_t len              = access_req.path_length > sizeof(buf) ?
                                sizeof(buf) : access_req.path_length;
      ssize_t i               = read(sys, sandboxFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      access_req.path_length -= i;
    }
    if (write(sys, threadFd, &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to return data from access() [process]");
    }
    return false;
  }
  SecureMem::lockSystemCall(parentMapsFd, mem);
  if (read(sys, sandboxFd, mem->pathname, access_req.path_length) !=
      (ssize_t)access_req.path_length) {
    goto read_parm_failed;
  }
  mem->pathname[access_req.path_length] = '\000';

  // TODO(markus): Implement sandboxing policy
  Debug::message(("Allowing access to \"" + std::string(mem->pathname) +
                  "\"").c_str());

  // Tell trusted thread to access the file.
  SecureMem::sendSystemCall(threadFdPub, true, parentMapsFd, mem, __NR_access,
                            mem->pathname - (char*)mem + (char*)mem->self,
                            access_req.mode);
  return true;
}

} // namespace
