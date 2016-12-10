// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"

#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"

#if defined(OS_LINUX)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "services/service_manager/public/cpp/standalone_service/linux_sandbox.h"
#endif

#if defined(OS_MACOSX)
#include "services/service_manager/public/cpp/standalone_service/mach_broker.h"
#endif

namespace service_manager {
namespace {

#if defined(OS_LINUX)
std::unique_ptr<LinuxSandbox> InitializeSandbox() {
  using sandbox::syscall_broker::BrokerFilePermission;
  // Warm parts of base in the copy of base in the mojo runner.
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
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

// Should be created and initialized on the main thread and kept alive as long
// a Service is running in the current process.
class ScopedAppContext : public mojo::edk::ProcessDelegate {
 public:
  ScopedAppContext()
      : io_thread_("io_thread"),
        wait_for_shutdown_event_(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
    mojo::edk::Init();
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    mojo::edk::InitIPCSupport(this, io_thread_.task_runner());
    mojo::edk::SetParentPipeHandleFromCommandLine();
  }

  ~ScopedAppContext() override {
    mojo::edk::ShutdownIPCSupport();
    wait_for_shutdown_event_.Wait();
  }

 private:
  // ProcessDelegate implementation.
  void OnShutdownComplete() override {
    wait_for_shutdown_event_.Signal();
  }

  base::Thread io_thread_;

  // Used to unblock the main thread on shutdown.
  base::WaitableEvent wait_for_shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppContext);
};

}  // namespace

void RunStandaloneService(const StandaloneServiceCallback& callback) {
  DCHECK(!base::MessageLoop::current());

#if defined(OS_MACOSX)
  // Send our task port to the parent.
  MachBroker::SendTaskPortToParent();
#endif

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping just before initializing sandbox to make
  // sure symbol names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif
#if defined(OS_LINUX)
  std::unique_ptr<LinuxSandbox> sandbox;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableSandbox))
    sandbox = InitializeSandbox();
#endif

  ScopedAppContext app_context;
  callback.Run(GetServiceRequestFromCommandLine());
}

}  // namespace service_manager
