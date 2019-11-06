// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/linux/bpf_gpu_policy_linux.h"

namespace service_manager {

// This policy is for Chrome OS ARM.
class SERVICE_MANAGER_SANDBOX_EXPORT CrosArmGpuProcessPolicy
    : public GpuProcessPolicy {
 public:
  explicit CrosArmGpuProcessPolicy(bool allow_shmat);
  ~CrosArmGpuProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
#if defined(__arm__) || defined(__aarch64__)
  const bool allow_shmat_;  // Allow shmat(2).
#endif
  DISALLOW_COPY_AND_ASSIGN(CrosArmGpuProcessPolicy);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_
