// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/bpf_broker_policy_linux.h"

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

namespace service_manager {

BrokerProcessPolicy::BrokerProcessPolicy() {}

BrokerProcessPolicy::~BrokerProcessPolicy() {}

ResultExpr BrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if defined(__NR_access)
    case __NR_access:
#endif
#if defined(__NR_faccessat)
    case __NR_faccessat:
#endif
#if defined(__NR_open)
    case __NR_open:
#endif
#if defined(__NR_openat)
    case __NR_openat:
#endif
#if defined(__NR_unlink)
    case __NR_unlink:
#endif
#if defined(__NR_rename)
    case __NR_rename:
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
#if defined(__NR_readlink)
    case __NR_readlink:
#endif
#if defined(__NR_readlinkat)
    case __NR_readlinkat:
#endif
      return Allow();
    default:
      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace service_manager
