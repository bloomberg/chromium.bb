// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/bpf_network_policy_linux.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
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

using sandbox::arch_seccomp_data;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::BrokerProcess;
using sandbox::SyscallSets;

namespace service_manager {
namespace {

intptr_t NetworkSIGSYS_Handler(const struct arch_seccomp_data& args,
                               void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  BrokerProcess* broker_process =
      static_cast<BrokerProcess*>(aux_broker_process);
  switch (args.nr) {
#if !defined(__aarch64__)
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
    case __NR_open:
#if defined(MEMORY_SANITIZER)
      // http://crbug.com/372840
      __msan_unpoison_string(reinterpret_cast<const char*>(args.args[0]));
#endif
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Access(
            reinterpret_cast<const char*>(args.args[1]),
            static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

}  // namespace

NetworkProcessPolicy::NetworkProcessPolicy() {}

NetworkProcessPolicy::~NetworkProcessPolicy() {}

ResultExpr NetworkProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat: {
      auto* broker_process = SandboxLinux::GetInstance()->broker_process();
      return Trap(NetworkSIGSYS_Handler, broker_process);
    }
    default:
      // TODO(tsepez): FIX this.
      return Allow();
  }
}

std::unique_ptr<BPFBasePolicy> NetworkProcessPolicy::GetBrokerSandboxPolicy() {
  return std::make_unique<NetworkBrokerProcessPolicy>();
}

NetworkBrokerProcessPolicy::NetworkBrokerProcessPolicy() {}

NetworkBrokerProcessPolicy::~NetworkBrokerProcessPolicy() {}

ResultExpr NetworkBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
#if !defined(OS_CHROMEOS) && !defined(__aarch64__)
    // The broker process needs to able to unlink temporary files it creates.
    case __NR_unlink:
#endif
      return Allow();
    default:
      return NetworkProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace service_manager
