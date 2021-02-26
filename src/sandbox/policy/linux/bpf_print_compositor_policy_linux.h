// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_PRINT_COMPOSITOR_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_PRINT_COMPOSITOR_POLICY_LINUX_H_

#include "base/macros.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy can be used by print compositor utility processes.
class PrintCompositorProcessPolicy : public BPFBasePolicy {
 public:
  PrintCompositorProcessPolicy();
  ~PrintCompositorProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintCompositorProcessPolicy);
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_PRINT_COMPOSITOR_POLICY_LINUX_H_
