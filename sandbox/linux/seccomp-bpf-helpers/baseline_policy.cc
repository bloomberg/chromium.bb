// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/services/linux_syscalls.h"

// Changing this implementation will have an effect on *all* policies.
// Currently this means: Renderer/Worker, GPU, Flash and NaCl.

namespace sandbox {

namespace {

bool IsBaselinePolicyAllowed(int sysno) {
  return SyscallSets::IsAllowedAddressSpaceAccess(sysno) ||
         SyscallSets::IsAllowedBasicScheduler(sysno) ||
         SyscallSets::IsAllowedEpoll(sysno) ||
         SyscallSets::IsAllowedFileSystemAccessViaFd(sysno) ||
         SyscallSets::IsAllowedGeneralIo(sysno) ||
         SyscallSets::IsAllowedGetOrModifySocket(sysno) ||
         SyscallSets::IsAllowedGettime(sysno) ||
         SyscallSets::IsAllowedPrctl(sysno) ||
         SyscallSets::IsAllowedProcessStartOrDeath(sysno) ||
         SyscallSets::IsAllowedSignalHandling(sysno) ||
         SyscallSets::IsFutex(sysno) ||
         SyscallSets::IsGetSimpleId(sysno) ||
         SyscallSets::IsKernelInternalApi(sysno) ||
#if defined(__arm__)
         SyscallSets::IsArmPrivate(sysno) ||
#endif
         SyscallSets::IsKill(sysno) ||
         SyscallSets::IsAllowedOperationOnFd(sysno);
}

// System calls that will trigger the crashing SIGSYS handler.
bool IsBaselinePolicyWatched(int sysno) {
  return SyscallSets::IsAdminOperation(sysno) ||
         SyscallSets::IsAdvancedScheduler(sysno) ||
         SyscallSets::IsAdvancedTimer(sysno) ||
         SyscallSets::IsAsyncIo(sysno) ||
         SyscallSets::IsDebug(sysno) ||
         SyscallSets::IsEventFd(sysno) ||
         SyscallSets::IsExtendedAttributes(sysno) ||
         SyscallSets::IsFaNotify(sysno) ||
         SyscallSets::IsFsControl(sysno) ||
         SyscallSets::IsGlobalFSViewChange(sysno) ||
         SyscallSets::IsGlobalProcessEnvironment(sysno) ||
         SyscallSets::IsGlobalSystemStatus(sysno) ||
         SyscallSets::IsInotify(sysno) ||
         SyscallSets::IsKernelModule(sysno) ||
         SyscallSets::IsKeyManagement(sysno) ||
         SyscallSets::IsMessageQueue(sysno) ||
         SyscallSets::IsMisc(sysno) ||
#if defined(__x86_64__)
         SyscallSets::IsNetworkSocketInformation(sysno) ||
#endif
         SyscallSets::IsNuma(sysno) ||
         SyscallSets::IsProcessGroupOrSession(sysno) ||
#if defined(__i386__)
         SyscallSets::IsSocketCall(sysno) ||
#endif
#if defined(__arm__)
         SyscallSets::IsArmPciConfig(sysno) ||
#endif
         SyscallSets::IsTimer(sysno);
}

// |fs_denied_errno| is the errno return for denied filesystem access.
ErrorCode EvaluateSyscallImpl(int fs_denied_errno, SandboxBPF* sandbox,
                              int sysno) {
  if (IsBaselinePolicyAllowed(sysno)) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

#if defined(__x86_64__) || defined(__arm__)
  if (sysno == __NR_socketpair) {
    // Only allow AF_UNIX, PF_UNIX. Crash if anything else is seen.
    COMPILE_ASSERT(AF_UNIX == PF_UNIX, af_unix_pf_unix_different);
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, AF_UNIX,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
                         sandbox->Trap(CrashSIGSYS_Handler, NULL));
  }
#endif

  if (sysno == __NR_madvise) {
    // Only allow MADV_DONTNEED (aka MADV_FREE).
    return sandbox->Cond(2, ErrorCode::TP_32BIT,
                         ErrorCode::OP_EQUAL, MADV_DONTNEED,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
                         ErrorCode(EPERM));
  }

#if defined(__i386__) || defined(__x86_64__)
  if (sysno == __NR_mmap)
    return RestrictMmapFlags(sandbox);
#endif

#if defined(__i386__) || defined(__arm__)
  if (sysno == __NR_mmap2)
    return RestrictMmapFlags(sandbox);
#endif

  if (sysno == __NR_mprotect)
    return RestrictMprotectFlags(sandbox);

  if (sysno == __NR_fcntl)
    return RestrictFcntlCommands(sandbox);

#if defined(__i386__) || defined(__arm__)
  if (sysno == __NR_fcntl64)
    return RestrictFcntlCommands(sandbox);
#endif

  if (SyscallSets::IsFileSystem(sysno) ||
      SyscallSets::IsCurrentDirectory(sysno)) {
    return ErrorCode(fs_denied_errno);
  }

  if (SyscallSets::IsAnySystemV(sysno)) {
    return ErrorCode(EPERM);
  }

  if (SyscallSets::IsUmask(sysno) ||
      SyscallSets::IsDeniedFileSystemAccessViaFd(sysno) ||
      SyscallSets::IsDeniedGetOrModifySocket(sysno) ||
      SyscallSets::IsProcessPrivilegeChange(sysno)) {
    return ErrorCode(EPERM);
  }

#if defined(__i386__)
  if (SyscallSets::IsSocketCall(sysno))
    return RestrictSocketcallCommand(sandbox);
#endif

  if (IsBaselinePolicyWatched(sysno)) {
    // Previously unseen syscalls. TODO(jln): some of these should
    // be denied gracefully right away.
    return sandbox->Trap(CrashSIGSYS_Handler, NULL);
  }
  // In any other case crash the program with our SIGSYS handler.
  return sandbox->Trap(CrashSIGSYS_Handler, NULL);
}

}  // namespace.

// Unfortunately C++03 doesn't allow delegated constructors.
// Call other constructor when C++11 lands.
BaselinePolicy::BaselinePolicy()
    : fs_denied_errno_(EPERM) {}

BaselinePolicy::BaselinePolicy(int fs_denied_errno)
    : fs_denied_errno_(fs_denied_errno) {}

BaselinePolicy::~BaselinePolicy() {}

ErrorCode BaselinePolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                          int sysno) const {
  return EvaluateSyscallImpl(fs_denied_errno_, sandbox, sysno);
}

}  // namespace sandbox.
