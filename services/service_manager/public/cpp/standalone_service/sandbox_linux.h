// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_SERVICE_SANDBOX_LINUX_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_SERVICE_SANDBOX_LINUX_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/syscall_broker/broker_process.h"

namespace service_manager {

// Encapsulates all tasks related to raising the sandbox for a standalone
// service.
class SandboxLinux {
 public:
  explicit SandboxLinux(
      const std::vector<sandbox::syscall_broker::BrokerFilePermission>&
          permissions);
  ~SandboxLinux();

  // Grabs a file descriptor to /proc.
  void Warmup();

  // Puts the user in a new PID namespace.
  void EngageNamespaceSandbox();

  // Starts a broker process and sets up seccomp-bpf to delegate decisions to
  // it.
  void EngageSeccompSandbox();

  // Performs the dropping of access to the outside world (drops the reference
  // to /proc acquired in Warmup().
  void Seal();

 private:
  bool warmed_up_;
  base::ScopedFD proc_fd_;
  std::unique_ptr<sandbox::syscall_broker::BrokerProcess> broker_;
  std::unique_ptr<sandbox::bpf_dsl::Policy> policy_;

  DISALLOW_COPY_AND_ASSIGN(SandboxLinux);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_SERVICE_SANDBOX_LINUX_H_
