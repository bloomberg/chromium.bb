// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_H_

#include "build/build_config.h"

// These are helpers to build seccomp-bpf policies, i.e. policies for a
// sandbox that reduces the Linux kernel's attack surface. Given their
// nature, they don't have any clear semantics and are completely
// "implementation-defined".

namespace sandbox {

bool IsKill(int sysno);
bool IsAllowedGettime(int sysno);
bool IsCurrentDirectory(int sysno);
bool IsUmask(int sysno);
// System calls that directly access the file system. They might acquire
// a new file descriptor or otherwise perform an operation directly
// via a path.
bool IsFileSystem(int sysno);
bool IsAllowedFileSystemAccessViaFd(int sysno);
bool IsDeniedFileSystemAccessViaFd(int sysno);
bool IsGetSimpleId(int sysno);
bool IsProcessPrivilegeChange(int sysno);
bool IsProcessGroupOrSession(int sysno);
bool IsAllowedSignalHandling(int sysno);
bool IsAllowedOperationOnFd(int sysno);
bool IsKernelInternalApi(int sysno);
// This should be thought through in conjunction with IsFutex().
bool IsAllowedProcessStartOrDeath(int sysno);
// It's difficult to restrict those, but there is attack surface here.
bool IsFutex(int sysno);
bool IsAllowedEpoll(int sysno);
bool IsAllowedGetOrModifySocket(int sysno);
bool IsDeniedGetOrModifySocket(int sysno);

#if defined(__i386__)
// Big multiplexing system call for sockets.
bool IsSocketCall(int sysno);
#endif

#if defined(__x86_64__) || defined(__arm__)
bool IsNetworkSocketInformation(int sysno);
#endif

bool IsAllowedAddressSpaceAccess(int sysno);
bool IsAllowedGeneralIo(int sysno);
bool IsAllowedPrctl(int sysno);
bool IsAllowedBasicScheduler(int sysno);
bool IsAdminOperation(int sysno);
bool IsKernelModule(int sysno);
bool IsGlobalFSViewChange(int sysno);
bool IsFsControl(int sysno);
bool IsNuma(int sysno);
bool IsMessageQueue(int sysno);
bool IsGlobalProcessEnvironment(int sysno);
bool IsDebug(int sysno);
bool IsGlobalSystemStatus(int sysno);
bool IsEventFd(int sysno);
// Asynchronous I/O API.
bool IsAsyncIo(int sysno);
bool IsKeyManagement(int sysno);
#if defined(__x86_64__) || defined(__arm__)
bool IsSystemVSemaphores(int sysno);
#endif
#if defined(__x86_64__) || defined(__arm__)
// These give a lot of ambient authority and bypass the setuid sandbox.
bool IsSystemVSharedMemory(int sysno);
#endif

#if defined(__x86_64__) || defined(__arm__)
#endif

#if defined(__i386__)
// Big system V multiplexing system call.
bool IsSystemVIpc(int sysno);
#endif

bool IsAnySystemV(int sysno);
bool IsAdvancedScheduler(int sysno);
bool IsInotify(int sysno);
bool IsFaNotify(int sysno);
bool IsTimer(int sysno);
bool IsAdvancedTimer(int sysno);
bool IsExtendedAttributes(int sysno);
bool IsMisc(int sysno);
#if defined(__arm__)
bool IsArmPciConfig(int sysno);
#endif  // defined(__arm__)

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_H_
