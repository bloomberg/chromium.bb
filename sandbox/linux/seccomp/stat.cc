// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_stat(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_stat, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    int       sysnum;
    long long cookie;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_stat;
  request->cookie               = cookie();
  request->stat_req.sysnum      = __NR_stat;
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward stat() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_stat);
  return rc;
}

long Sandbox::sandbox_lstat(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_lstat, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    int       sysnum;
    long long cookie;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_lstat;
  request->cookie               = cookie();
  request->stat_req.sysnum      = __NR_lstat;
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward lstat() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_lstat);
  return rc;
}

#if defined(__NR_stat64)
long Sandbox::sandbox_stat64(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_stat64, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    int       sysnum;
    long long cookie;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_stat64;
  request->cookie               = cookie();
  request->stat_req.sysnum      = __NR_stat64;
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward stat64() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_stat64);
  return rc;
}

long Sandbox::sandbox_lstat64(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_lstat64, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    int       sysnum;
    long long cookie;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_lstat64;
  request->cookie               = cookie();
  request->stat_req.sysnum      = __NR_lstat64;
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward lstat64() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_lstat64);
  return rc;
}
#endif

bool Sandbox::process_stat(int parentMapsFd, int sandboxFd, int threadFdPub,
                           int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  Stat stat_req;
  if (read(sys, sandboxFd, &stat_req, sizeof(stat_req)) != sizeof(stat_req)) {
 read_parm_failed:
    die("Failed to read parameters for stat() [process]");
  }
  int   rc                  = -ENAMETOOLONG;
  if (stat_req.path_length >= (int)sizeof(mem->pathname)) {
    char buf[32];
    while (stat_req.path_length > 0) {
      size_t len            = stat_req.path_length > sizeof(buf) ?
                              sizeof(buf) : stat_req.path_length;
      ssize_t i             = read(sys, sandboxFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      stat_req.path_length -= i;
    }
    if (write(sys, threadFd, &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to return data from stat() [process]");
    }
    return false;
  }

  // We implement a strict policy of denying all file accesses
  char tmp[stat_req.path_length + 1];
  if (read(sys, sandboxFd, tmp, stat_req.path_length) !=
      (ssize_t)stat_req.path_length) {
    goto read_parm_failed;
  }
  tmp[stat_req.path_length] = '\000';
  Debug::message(("Denying access to \"" + std::string(tmp) + "\"").c_str());
  SecureMem::abandonSystemCall(threadFd, -EACCES);
  return false;
}

} // namespace
