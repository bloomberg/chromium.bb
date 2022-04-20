// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace screen_ai {

bool ScreenAIPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  // TODO(https://crbug.com/1278249): Ensure this is the same version of the
  // library that is used in ScreenAIService and component updater has not added
  // a newer version between the two steps.
  const base::FilePath library_path = screen_ai::GetLibraryFilePath();
  if (library_path.empty()) {
    VLOG(1) << "Screen AI library not found.";
  } else {
    void* screen_ai_library = dlopen(library_path.value().c_str(),
                                     RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
    // The library is delivered by the component updater. If it is not available
    // we cannot do anything about it here. The requests to the service will
    // fail later as the library does not exist.
    if (!screen_ai_library)
      VLOG(1) << dlerror();
    else
      VLOG(2) << "Screen AI library loaded pre-sandboxing:" << library_path;
  }

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/proc/meminfo")};

  instance->StartBrokerProcess(
      MakeBrokerCommandSet({sandbox::syscall_broker::COMMAND_ACCESS,
                            sandbox::syscall_broker::COMMAND_OPEN}),
      permissions, sandbox::policy::SandboxLinux::PreSandboxHook(), options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace screen_ai
