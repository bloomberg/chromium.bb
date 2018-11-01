// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"

#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/service_manager/sandbox/switches.h"

#if defined(OS_LINUX)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#endif

#if defined(OS_MACOSX)
#include "services/service_manager/public/cpp/standalone_service/mach_broker.h"
#endif

namespace service_manager {

void RunStandaloneService(const StandaloneServiceCallback& callback) {
  DCHECK(!base::MessageLoopCurrent::Get());

#if defined(OS_MACOSX)
  // Send our task port to the parent.
  MachBroker::SendTaskPortToParent();
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OS_LINUX)
  if (command_line.HasSwitch(switches::kServiceSandboxType)) {
    // Warm parts of base in the copy of base in the mojo runner.
    base::RandUint64();
    base::SysInfo::AmountOfPhysicalMemory();
    base::SysInfo::NumberOfProcessors();

    // Repeat steps normally performed by the zygote.
    SandboxLinux::Options sandbox_options;
    sandbox_options.engage_namespace_sandbox = true;

    Sandbox::Initialize(
        UtilitySandboxTypeFromString(
            command_line.GetSwitchValueASCII(switches::kServiceSandboxType)),
        SandboxLinux::PreSandboxHook(), sandbox_options);
  }
#endif

  mojo::core::Init();

  base::TaskScheduler::CreateAndStartWithDefaultParams("StandaloneService");
  base::Thread io_thread("io_thread");
  io_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  mojo::core::ScopedIPCSupport ipc_support(
      io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  auto invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          command_line));
  callback.Run(GetServiceRequestFromCommandLine(&invitation));
}

}  // namespace service_manager
