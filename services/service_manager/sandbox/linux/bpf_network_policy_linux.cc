// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/bpf_network_policy_linux.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_seccomp_bpf_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace service_manager {

NetworkProcessPolicy::NetworkProcessPolicy() {}

NetworkProcessPolicy::~NetworkProcessPolicy() {}

ResultExpr NetworkProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
// Allowed syscalls:
#if defined(__NR_accept)
    case __NR_accept:
#endif
#if defined(__NR_bind)
    case __NR_bind:
#endif
#if defined(__NR_connect)
    case __NR_connect:
#endif
#if defined(__NR_getsockname)
    case __NR_getsockname:
#endif
#if defined(__NR_inotify_add_watch)
    case __NR_inotify_add_watch:
#endif
#if defined(__NR_inotify_init)
    case __NR_inotify_init:
#endif
#if defined(__NR_inotify_init1)
    case __NR_inotify_init1:
#endif
#if defined(__NR_inotify_rm_watch)
    case __NR_inotify_rm_watch:
#endif
#if defined(__NR_ioctl)
    // TODO(tsepez): restrict based on arguments.
    case __NR_ioctl:
#endif
#if defined(__NR_listen)
    case __NR_listen:
#endif
#if defined(__NR_socket)
    case __NR_socket:
#endif
#if defined(__NR_uname)
    case __NR_uname:
#endif
#if defined(__NR_sysinfo)
    case __NR_sysinfo:
#endif
      return Allow();

// Restricted syscalls:
#if defined(__NR_fcntl)
    case __NR_fcntl:
      return sandbox::RestrictFcntlCommands();
#endif

// Trapped syscalls:
#if defined(__NR_access)
    case __NR_access:
#endif
#if defined(__NR_faccessat)
    case __NR_faccessat:
#endif
#if defined(__NR_mkdir)
    case __NR_mkdir:
#endif
#if defined(__NR_mkdirat)
    case __NR_mkdirat:
#endif
#if defined(__NR_open)
    case __NR_open:
#endif
#if defined(__NR_openat)
    case __NR_openat:
#endif
#if defined(__NR_readlink)
    case __NR_readlink:
#endif
#if defined(__NR_readlinkat)
    case __NR_readlinkat:
#endif
#if defined(__NR_rmdir)
    case __NR_rmdir:
#endif
#if defined(__NR_rename)
    case __NR_rename:
#endif
#if defined(__NR_renameat)
    case __NR_renameat:
#endif
#if defined(__NR_stat)
    case __NR_stat:
#endif
#if defined(__NR_stat64)
    case __NR_stat64:
#endif
#if defined(__NR_fstatat)
    case __NR_fstatat:
#endif
#if defined(__NR_newfstatat)
    case __NR_newfstatat:
#endif
#if defined(__NR_unlink)
    case __NR_unlink:
#endif
#if defined(__NR_unlinkat)
    case __NR_unlinkat:
#endif
      return Trap(BrokerProcess::SIGSYS_Handler,
                  SandboxLinux::GetInstance()->broker_process());

    default:
      return BPFBasePolicy::EvaluateSyscall(errno);
  }
}

}  // namespace service_manager
