// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_FORWARD_H_
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_FORWARD_H_

#include "base/callback_forward.h"

namespace playground2 {

class Sandbox;
class ErrorCode;
typedef ErrorCode BpfSandboxPolicy(
    Sandbox* sandbox_compiler,
    int system_call_number,
    void* aux);

typedef base::Callback<BpfSandboxPolicy> BpfSandboxPolicyCallback;

}  // namespace playground2

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_FORWARD_H_
