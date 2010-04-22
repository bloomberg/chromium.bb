// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

#ifndef IPC_PRIVATE
#define IPC_PRIVATE 0
#endif
#ifndef IPC_RMID
#define IPC_RMID    0
#endif
#ifndef IPC_64
#define IPC_64      256
#endif

#if defined(__NR_shmget)
void* Sandbox::sandbox_shmat(int shmid, const void* shmaddr, int shmflg) {
  long long tm;
  Debug::syscall(&tm, __NR_shmat, "Executing handler");

  struct {
    int       sysnum;
    long long cookie;
    ShmAt     shmat_req;
  } __attribute__((packed)) request;
  request.sysnum             = __NR_shmat;
  request.cookie             = cookie();
  request.shmat_req.shmid    = shmid;
  request.shmat_req.shmaddr  = shmaddr;
  request.shmat_req.shmflg   = shmflg;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward shmat() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_shmat);
  return reinterpret_cast<void *>(rc);
}

long Sandbox::sandbox_shmctl(int shmid, int cmd, void* buf) {
  long long tm;
  Debug::syscall(&tm, __NR_shmctl, "Executing handler");

  struct {
    int       sysnum;
    long long cookie;
    ShmCtl    shmctl_req;
  } __attribute__((packed)) request;
  request.sysnum           = __NR_shmctl;
  request.cookie           = cookie();
  request.shmctl_req.shmid = shmid;
  request.shmctl_req.cmd   = cmd;
  request.shmctl_req.buf   = buf;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward shmctl() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_shmctl);
  return rc;
}

long Sandbox::sandbox_shmdt(const void* shmaddr) {
  long long tm;
  Debug::syscall(&tm, __NR_shmdt, "Executing handler");

  struct {
    int       sysnum;
    long long cookie;
    ShmDt     shmdt_req;
  } __attribute__((packed)) request;
  request.sysnum             = __NR_shmdt;
  request.cookie             = cookie();
  request.shmdt_req.shmaddr  = shmaddr;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward shmdt() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_shmdt);
  return rc;
}

long Sandbox::sandbox_shmget(int key, size_t size, int shmflg) {
  long long tm;
  Debug::syscall(&tm, __NR_shmget, "Executing handler");

  struct {
    int       sysnum;
    long long cookie;
    ShmGet    shmget_req;
  } __attribute__((packed)) request;
  request.sysnum            = __NR_shmget;
  request.cookie            = cookie();
  request.shmget_req.key    = key;
  request.shmget_req.size   = size;
  request.shmget_req.shmflg = shmflg;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward shmget() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_shmget);
  return rc;
}

bool Sandbox::process_shmat(int parentMapsFd, int sandboxFd, int threadFdPub,
                            int threadFd, SecureMem::Args* mem) {
  // Read request
  ShmAt shmat_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &shmat_req, sizeof(shmat_req)) !=
      sizeof(shmat_req)) {
    die("Failed to read parameters for shmat() [process]");
  }

  // We only allow attaching to the shm identifier that was returned by
  // the most recent call to shmget(IPC_PRIVATE)
  if (shmat_req.shmaddr || shmat_req.shmflg || shmat_req.shmid != mem->shmId) {
    mem->shmId = -1;
    SecureMem::abandonSystemCall(threadFd, -EINVAL);
    return false;
  }

  mem->shmId = -1;
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem,
                            __NR_shmat, shmat_req.shmid, shmat_req.shmaddr,
                            shmat_req.shmflg);
  return true;
}

bool Sandbox::process_shmctl(int parentMapsFd, int sandboxFd, int threadFdPub,
                             int threadFd, SecureMem::Args* mem) {
  // Read request
  ShmCtl shmctl_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &shmctl_req, sizeof(shmctl_req)) !=
      sizeof(shmctl_req)) {
    die("Failed to read parameters for shmctl() [process]");
  }

  // The only shmctl() operation that we need to support is removal. This
  // operation is generally safe.
  if ((shmctl_req.cmd & ~(IPC_64 | IPC_RMID)) || shmctl_req.buf) {
    mem->shmId = -1;
    SecureMem::abandonSystemCall(threadFd, -EINVAL);
    return false;
  }

  mem->shmId = -1;
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem,
                            __NR_shmctl, shmctl_req.shmid, shmctl_req.cmd,
                            shmctl_req.buf);
  return true;
}

bool Sandbox::process_shmdt(int parentMapsFd, int sandboxFd, int threadFdPub,
                            int threadFd, SecureMem::Args* mem) {
  // Read request
  ShmDt shmdt_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &shmdt_req, sizeof(shmdt_req)) !=
      sizeof(shmdt_req)) {
    die("Failed to read parameters for shmdt() [process]");
  }

  // Detaching shared memory segments it generally safe, but just in case
  // of a kernel bug, we make sure that the address does not fall into any
  // of the reserved memory regions.
  ProtectedMap::const_iterator iter = protectedMap_.lower_bound(
      (void *)shmdt_req.shmaddr);
  if (iter != protectedMap_.begin()) {
    --iter;
  }
  for (; iter != protectedMap_.end() && iter->first <= shmdt_req.shmaddr;
       ++iter){
    if (shmdt_req.shmaddr < reinterpret_cast<void *>(
            reinterpret_cast<char *>(iter->first) + iter->second) &&
        shmdt_req.shmaddr >= iter->first) {
      mem->shmId = -1;
      SecureMem::abandonSystemCall(threadFd, -EINVAL);
      return false;
    }
  }

  mem->shmId = -1;
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem,
                            __NR_shmdt, shmdt_req.shmaddr);
  return true;
}

bool Sandbox::process_shmget(int parentMapsFd, int sandboxFd, int threadFdPub,
                             int threadFd, SecureMem::Args* mem) {
  // Read request
  ShmGet shmget_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &shmget_req, sizeof(shmget_req)) !=
      sizeof(shmget_req)) {
    die("Failed to read parameters for shmget() [process]");
  }

  // We do not want to allow the sandboxed application to access arbitrary
  // shared memory regions. We only allow it to access regions that it
  // created itself.
  if (shmget_req.key != IPC_PRIVATE || shmget_req.shmflg & ~0777) {
    mem->shmId = -1;
    SecureMem::abandonSystemCall(threadFd, -EINVAL);
    return false;
  }

  mem->shmId = -1;
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem,
                            __NR_shmget, shmget_req.key, shmget_req.size,
                            shmget_req.shmflg);
  return true;
}
#endif

#if defined(__NR_ipc)
#ifndef SHMAT
#define SHMAT       21
#endif
#ifndef SHMDT
#define SHMDT       22
#endif
#ifndef SHMGET
#define SHMGET      23
#endif
#ifndef SHMCTL
#define SHMCTL      24
#endif

long Sandbox::sandbox_ipc(unsigned call, int first, int second, int third,
                         void* ptr, long fifth) {
  long long tm;
  Debug::syscall(&tm, __NR_ipc, "Executing handler", call);
  struct {
    int       sysnum;
    long long cookie;
    IPC       ipc_req;
  } __attribute__((packed)) request;
  request.sysnum         = __NR_ipc;
  request.cookie         = cookie();
  request.ipc_req.call   = call;
  request.ipc_req.first  = first;
  request.ipc_req.second = second;
  request.ipc_req.third  = third;
  request.ipc_req.ptr    = ptr;
  request.ipc_req.fifth  = fifth;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward ipc() request [sandbox]");
  }
  Debug::elapsed(tm, __NR_ipc, call);
  return rc;
}

bool Sandbox::process_ipc(int parentMapsFd, int sandboxFd, int threadFdPub,
                          int threadFd, SecureMem::Args* mem) {
  // Read request
  IPC ipc_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &ipc_req, sizeof(ipc_req)) != sizeof(ipc_req)) {
    die("Failed to read parameters for ipc() [process]");
  }

  // We do not support all of the SysV IPC calls. In fact, we only support
  // the minimum feature set necessary for Chrome's renderers to share memory
  // with the X server.
  switch (ipc_req.call) {
    case SHMAT: {
      // We only allow attaching to the shm identifier that was returned by
      // the most recent call to shmget(IPC_PRIVATE)
      if (ipc_req.ptr || ipc_req.second || ipc_req.first != mem->shmId) {
        goto deny;
      }
    accept:
      mem->shmId = -1;
      SecureMem::sendSystemCall(threadFdPub, false, -1, mem,
                                __NR_ipc, ipc_req.call, ipc_req.first,
                                ipc_req.second, ipc_req.third, ipc_req.ptr,
                                ipc_req.fifth);
      return true;
    }
    case SHMCTL:
      // The only shmctl() operation that we need to support is removal. This
      // operation is generally safe.
      if ((ipc_req.second & ~(IPC_64 | IPC_RMID)) || ipc_req.ptr) {
        goto deny;
      } else {
        goto accept;
      }
    case SHMDT: {
      // Detaching shared memory segments it generally safe, but just in case
      // of a kernel bug, we make sure that the address does not fall into any
      // of the reserved memory regions.
      ProtectedMap::const_iterator iter = protectedMap_.lower_bound(
          (void *)ipc_req.ptr);
      if (iter != protectedMap_.begin()) {
        --iter;
      }
      for (; iter != protectedMap_.end() && iter->first <=ipc_req.ptr; ++iter){
        if (ipc_req.ptr < reinterpret_cast<void *>(
                reinterpret_cast<char *>(iter->first) + iter->second) &&
            ipc_req.ptr >= iter->first) {
          goto deny;
        }
      }
      goto accept;
    }
    case SHMGET:
      // We do not want to allow the sandboxed application to access arbitrary
      // shared memory regions. We only allow it to access regions that it
      // created itself.
      if (ipc_req.first != IPC_PRIVATE || ipc_req.third & ~0777) {
        goto deny;
      } else {
        goto accept;
      }
    default:
      // Other than SysV shared memory, we do not actually need to support any
      // other SysV IPC calls.
    deny:
      mem->shmId = -1;
      SecureMem::abandonSystemCall(threadFd, -EINVAL);
      return false;
  }
}
#endif

} // namespace
