// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/linux/sandbox_bpf_base_policy_linux.h"

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT GpuProcessPolicy
    : public SandboxBPFBasePolicy {
 public:
  GpuProcessPolicy();
  ~GpuProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

  sandbox::syscall_broker::BrokerProcess* broker_process() const {
    return broker_process_;
  }

  void set_broker_process(
      std::unique_ptr<sandbox::syscall_broker::BrokerProcess> broker_process) {
    DCHECK(!broker_process_);
    broker_process_ = broker_process.release();
  }

 private:
  // A BrokerProcess is a helper that is started before the sandbox is engaged
  // and will serve requests to access files over an IPC channel. The client of
  // this runs from a SIGSYS handler triggered by the seccomp-bpf sandbox.
  // This should never be destroyed, as after the sandbox is started it is
  // vital to the process.
  sandbox::syscall_broker::BrokerProcess* broker_process_;  // Leaked as global.

  DISALLOW_COPY_AND_ASSIGN(GpuProcessPolicy);
};

class SERVICE_MANAGER_SANDBOX_EXPORT GpuBrokerProcessPolicy
    : public GpuProcessPolicy {
 public:
  GpuBrokerProcessPolicy();
  ~GpuBrokerProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuBrokerProcessPolicy);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_
