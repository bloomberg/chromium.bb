// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_NETWORK_POLICY_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_NETWORK_POLICY_LINUX_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT NetworkProcessPolicy
    : public BPFBasePolicy {
 public:
  NetworkProcessPolicy();
  ~NetworkProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

  std::unique_ptr<BPFBasePolicy> GetBrokerSandboxPolicy() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkProcessPolicy);
};

// A network-broker policy is the same as a network policy with access, open,
// openat and in the non-Chrome OS case unlink allowed.
// TODO(tsepez): probably should not inherit from NetworkProceesPolicy,
// since that may include socket syscalls that this does not need.
class SERVICE_MANAGER_SANDBOX_EXPORT NetworkBrokerProcessPolicy
    : public NetworkProcessPolicy {
 public:
  NetworkBrokerProcessPolicy();
  ~NetworkBrokerProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkBrokerProcessPolicy);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_NETWORK_POLICY_LINUX_H_
