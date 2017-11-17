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
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
#if !defined(OS_CHROMEOS) && !defined(__aarch64__)
    // The broker process needs to able to unlink the temporary
    // files that it may create.
    case __NR_unlink:
#endif
      return Allow();
    default:
      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace service_manager
