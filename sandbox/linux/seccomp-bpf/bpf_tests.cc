// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

using playground2::Die;
using playground2::Sandbox;

namespace sandbox {

void BpfTests::TestWrapper(void *void_arg) {
  TestArgs *arg = reinterpret_cast<TestArgs *>(void_arg);
  Die::EnableSimpleExit();
  if (Sandbox::supportsSeccompSandbox(-1) ==
      Sandbox::STATUS_AVAILABLE) {
    // Ensure the the sandbox is actually available at this time
    int proc_fd;
    BPF_ASSERT((proc_fd = open("/proc", O_RDONLY|O_DIRECTORY)) >= 0);
    BPF_ASSERT(Sandbox::supportsSeccompSandbox(proc_fd) ==
               Sandbox::STATUS_AVAILABLE);

    // Initialize and then start the sandbox with our custom policy
    Sandbox::setProcFd(proc_fd);
    Sandbox::setSandboxPolicy(arg->policy(), NULL);
    Sandbox::startSandbox();
    arg->test()();
  } else {
    // TODO(markus): (crbug.com/141545) Call the compiler and verify the
    //   policy. That's the least we can do, if we don't have kernel support.
    Sandbox::setSandboxPolicy(arg->policy(), NULL);
  }
}

}  // namespace
