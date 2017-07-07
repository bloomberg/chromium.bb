// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_ANDROID_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_ANDROID_H_

#include <sys/types.h>

#include "base/macros.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// This class provides a Seccomp-BPF sandbox policy for programs that run
// in the Android Runtime (Java) environment. It builds upon the Linux
// BaselinePolicy, which would be suitable for Android shell-based programs,
// and adds allowances for the JVM.
//
// As with the Linux BaselinePolicy, the behavior is largely implementation
// defined.
//
// TODO(rsesek): This policy may currently have allowances for //content-level
// features. This needs an audit. https://crbug.com/739879
class SANDBOX_EXPORT BaselinePolicyAndroid : public BaselinePolicy {
 public:
  BaselinePolicyAndroid();
  ~BaselinePolicyAndroid() override;

  // sandbox::BaselinePolicy:
  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaselinePolicyAndroid);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_ANDROID_H_
