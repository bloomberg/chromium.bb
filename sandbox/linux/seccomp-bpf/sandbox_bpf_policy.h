// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_H_
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_H_

#include "base/basictypes.h"

namespace playground2 {

class ErrorCode;
class Sandbox;

// This is the interface to implement to define a BPF sandbox policy.
class SandboxBpfPolicy {
 public:
  SandboxBpfPolicy() {}
  virtual ~SandboxBpfPolicy() {}

  // The EvaluateSyscall method is called with the system call number. It can
  // decide to allow the system call unconditionally by returning ERR_ALLOWED;
  // it can deny the system call unconditionally by returning an appropriate
  // "errno" value; or it can request inspection of system call argument(s) by
  // returning a suitable ErrorCode.
  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxBpfPolicy);
};

}  // namespace playground2

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_H_
