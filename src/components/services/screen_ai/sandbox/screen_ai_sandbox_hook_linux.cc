// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "ui/accessibility/accessibility_features.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace screen_ai {

bool ScreenAIPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  base::FilePath library_path = screen_ai::GetLatestLibraryFilePath();
  if (library_path.empty()) {
    VLOG(1) << "Screen AI library not found.";
  } else {
    void* screen_ai_library = dlopen(library_path.value().c_str(),
                                     RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
    // The library is delivered by the component updater. If it is not available
    // we cannot do anything about it here. The requests to the service will
    // fail later as the library does not exist.
    if (!screen_ai_library) {
      VLOG(1) << dlerror();
      library_path.clear();
    } else {
      VLOG(2) << "Screen AI library loaded pre-sandboxing:" << library_path;
    }
  }
  screen_ai::SetPreloadedLibraryFilePath(library_path);

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/proc/meminfo")};

  // The models are in the same folder as the library, and the library requires
  // read access for them.
  if (!library_path.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        library_path.DirName().MaybeAsASCII() + base::FilePath::kSeparators));
  }

  if (features::IsScreenAIDebugModeEnabled()) {
    permissions.push_back(
        BrokerFilePermission::ReadWriteCreateRecursive("/tmp/"));
  }

  instance->StartBrokerProcess(
      MakeBrokerCommandSet({sandbox::syscall_broker::COMMAND_ACCESS,
                            sandbox::syscall_broker::COMMAND_OPEN}),
      permissions, sandbox::policy::SandboxLinux::PreSandboxHook(), options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace screen_ai
