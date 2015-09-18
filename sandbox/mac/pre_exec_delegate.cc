// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/pre_exec_delegate.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "sandbox/mac/bootstrap_sandbox.h"

namespace sandbox {

PreExecDelegate::PreExecDelegate(
    const std::string& sandbox_server_bootstrap_name,
    uint64_t sandbox_token)
    : sandbox_server_bootstrap_name_(sandbox_server_bootstrap_name),
      sandbox_server_bootstrap_name_ptr_(
          sandbox_server_bootstrap_name_.c_str()),
      sandbox_token_(sandbox_token),
      is_yosemite_or_later_(base::mac::IsOSYosemiteOrLater()) {
}

PreExecDelegate::~PreExecDelegate() {}

void PreExecDelegate::RunAsyncSafe() {
  mach_port_t sandbox_server_port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_look_up(bootstrap_port,
      sandbox_server_bootstrap_name_ptr_, &sandbox_server_port);
  if (kr != KERN_SUCCESS)
    RAW_LOG(FATAL, "Failed to look up bootstrap sandbox server port.");

  mach_port_t new_bootstrap_port = MACH_PORT_NULL;
  if (!BootstrapSandbox::ClientCheckIn(sandbox_server_port,
                                       sandbox_token_,
                                       &new_bootstrap_port)) {
    RAW_LOG(FATAL, "Failed to check in with sandbox server.");
  }

  kr = task_set_bootstrap_port(mach_task_self(), new_bootstrap_port);
  if (kr != KERN_SUCCESS)
    RAW_LOG(FATAL, "Failed to replace bootstrap port.");

  // On OS X 10.10 and higher, libxpc uses the port stash to transfer the
  // XPC root port. This is effectively the same connection as the Mach
  // bootstrap port, but not transferred using the task special port.
  // Therefore, stash the replacement bootstrap port, so that on 10.10 it
  // will be retrieved by the XPC code and used as a replacement for the
  // XPC root port as well.
  if (is_yosemite_or_later_) {
    kr = mach_ports_register(mach_task_self(), &new_bootstrap_port, 1);
    if (kr != KERN_SUCCESS)
      RAW_LOG(ERROR, "Failed to register replacement bootstrap port.");
  }
}

}  // namespace sandbox
