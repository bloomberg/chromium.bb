// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/bootstrap_sandbox.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"

#include "sandbox/mac/launchd_interception_server.h"

namespace sandbox {

const int kNotAPolicy = -1;

// static
scoped_ptr<BootstrapSandbox> BootstrapSandbox::Create() {
  scoped_ptr<BootstrapSandbox> sandbox(new BootstrapSandbox());
  sandbox->server_.reset(new LaunchdInterceptionServer(sandbox.get()));

  if (!sandbox->server_->Initialize()) {
    sandbox.reset();
  } else {
    // Note that the extern global bootstrap_port (in bootstrap.h) will not
    // be changed here. The parent only has its bootstrap port replaced
    // permanently because changing it repeatedly in a multi-threaded program
    // could lead to unsafe access patterns. In a single-threaded program,
    // the port would be restored after fork(). See the design document for
    // a larger discussion.
    //
    // By not changing the global bootstrap_port, users of the bootstrap port
    // in the parent can potentially skip an unnecessary indirection through
    // the sandbox server.
    kern_return_t kr = task_set_special_port(mach_task_self(),
        TASK_BOOTSTRAP_PORT, sandbox->server_->server_port());
    if (kr != KERN_SUCCESS)
      sandbox.reset();
  }

  return sandbox.Pass();
}

BootstrapSandbox::~BootstrapSandbox() {
  kern_return_t kr = task_set_special_port(mach_task_self(),
      TASK_BOOTSTRAP_PORT, real_bootstrap_port_);
  MACH_CHECK(kr == KERN_SUCCESS, kr);
}

void BootstrapSandbox::RegisterSandboxPolicy(
    int sandbox_policy_id,
    const BootstrapSandboxPolicy& policy) {
  CHECK(IsPolicyValid(policy));
  CHECK_GT(sandbox_policy_id, 0);
  base::AutoLock lock(lock_);
  DCHECK(policies_.find(sandbox_policy_id) == policies_.end());
  policies_.insert(std::make_pair(sandbox_policy_id, policy));
}

void BootstrapSandbox::PrepareToForkWithPolicy(int sandbox_policy_id) {
  base::AutoLock lock(lock_);

  CHECK(policies_.find(sandbox_policy_id) != policies_.end());
  CHECK_EQ(kNotAPolicy, effective_policy_id_)
      << "Cannot nest calls to PrepareToForkWithPolicy()";

  // Store the policy for the process we're about to create.
  effective_policy_id_ = sandbox_policy_id;
}

// TODO(rsesek): The |lock_| needs to be taken twice because
// base::LaunchProcess handles both fork+exec, and holding the lock for the
// duration would block servicing of other bootstrap messages. If a better
// LaunchProcess existed (do arbitrary work without layering violations), this
// could be avoided.

void BootstrapSandbox::FinishedFork(base::ProcessHandle handle) {
  base::AutoLock lock(lock_);

  CHECK_NE(kNotAPolicy, effective_policy_id_)
      << "Must PrepareToForkWithPolicy() before FinishedFork()";

  if (handle != base::kNullProcessHandle) {
    const auto& existing_process = sandboxed_processes_.find(handle);
    CHECK(existing_process == sandboxed_processes_.end());
    sandboxed_processes_.insert(std::make_pair(handle, effective_policy_id_));
    VLOG(3) << "Bootstrap sandbox enforced for pid " << handle;
  }

  effective_policy_id_ = kNotAPolicy;
}

void BootstrapSandbox::ChildDied(base::ProcessHandle handle) {
  base::AutoLock lock(lock_);
  const auto& it = sandboxed_processes_.find(handle);
  CHECK(it != sandboxed_processes_.end());
  sandboxed_processes_.erase(it);
}

const BootstrapSandboxPolicy* BootstrapSandbox::PolicyForProcess(
    pid_t pid) const {
  base::AutoLock lock(lock_);
  const auto& process = sandboxed_processes_.find(pid);

  // The new child could send bootstrap requests before the parent calls
  // FinishedFork().
  int policy_id = effective_policy_id_;
  if (process != sandboxed_processes_.end()) {
    policy_id = process->second;
  }

  if (policy_id == kNotAPolicy)
    return NULL;

  return &policies_.find(policy_id)->second;
}

BootstrapSandbox::BootstrapSandbox()
    : real_bootstrap_port_(MACH_PORT_NULL),
      effective_policy_id_(kNotAPolicy) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = task_get_special_port(
      mach_task_self(), TASK_BOOTSTRAP_PORT, &port);
  MACH_CHECK(kr == KERN_SUCCESS, kr);
  real_bootstrap_port_.reset(port);
}

}  // namespace sandbox
