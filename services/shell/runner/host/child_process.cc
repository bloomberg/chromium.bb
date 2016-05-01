// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/child_process.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/runner/host/child_process_base.h"
#include "services/shell/runner/host/native_application_support.h"
#include "services/shell/runner/init.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "services/shell/runner/host/linux_sandbox.h"
#endif

#if defined(OS_MACOSX)
#include "services/shell/runner/host/mach_broker.h"
#endif

namespace shell {

namespace {

#if defined(OS_LINUX) && !defined(OS_ANDROID)
std::unique_ptr<LinuxSandbox> InitializeSandbox() {
  using sandbox::syscall_broker::BrokerFilePermission;
  // Warm parts of base in the copy of base in the mojo runner.
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::MaxSharedMemorySize();
  base::SysInfo::NumberOfProcessors();

  // TODO(erg,jln): Allowing access to all of /dev/shm/ makes it easy to
  // spy on other shared memory using processes. This is a temporary hack
  // so that we have some sandbox until we have proper shared memory
  // support integrated into mojo.
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(
      BrokerFilePermission::ReadWriteCreateUnlinkRecursive("/dev/shm/"));
  std::unique_ptr<LinuxSandbox> sandbox(new LinuxSandbox(permissions));
  sandbox->Warmup();
  sandbox->EngageNamespaceSandbox();
  sandbox->EngageSeccompSandbox();
  sandbox->Seal();
  return sandbox;
}
#endif

void RunNativeLibrary(base::NativeLibrary app_library,
                      mojom::ShellClientRequest shell_client_request) {
  if (!RunNativeApplication(app_library, std::move(shell_client_request))) {
    LOG(ERROR) << "Failure to RunNativeApplication()";
  }
}

}  // namespace

int ChildProcessMain() {
  DVLOG(2) << "ChildProcessMain()";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  std::unique_ptr<LinuxSandbox> sandbox;
#endif
  base::NativeLibrary app_library = 0;
  // Load the application library before we engage the sandbox.
  base::FilePath app_library_path =
      command_line.GetSwitchValuePath(switches::kChildProcess);
  if (!app_library_path.empty())
    app_library = LoadNativeApplication(app_library_path);
  base::i18n::InitializeICU();
  if (app_library)
    CallLibraryEarlyInitialization(app_library);

#if defined(OS_MACOSX)
  // Send our task port to the parent.
  MachBroker::SendTaskPortToParent();
#endif

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping just before initializing sandbox to make
  // sure symbol names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kEnableSandbox))
    sandbox = InitializeSandbox();
#endif

  ChildProcessMain(base::Bind(&RunNativeLibrary, app_library));

  return 0;
}

}  // namespace shell
