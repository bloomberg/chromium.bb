// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/ipc_fuzzer/message_replay/replay_process.h"

#include <limits.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/child_process.mojom-test-utils.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "services/service_manager/embedder/descriptors.h"
#endif

namespace ipc_fuzzer {

namespace {

// Used to simulate a basic child process IPC endpoint and bootstrap the legacy
// IPC channel driven by this process.
class FakeChildProcessImpl
    : public content::mojom::ChildProcessInterceptorForTesting {
 public:
  explicit FakeChildProcessImpl(
      mojo::PendingRemote<IPC::mojom::ChannelBootstrap> legacy_ipc_bootstrap)
      : legacy_ipc_bootstrap_(std::move(legacy_ipc_bootstrap)) {
    ignore_result(disconnected_process_.BindNewPipeAndPassReceiver());
  }

  // content::mojom::ChildProcessInterceptorForTesting overrides:
  content::mojom::ChildProcess* GetForwardingInterface() override {
    return disconnected_process_.get();
  }

  void Initialize(mojo::PendingRemote<content::mojom::ChildProcessHostBootstrap>
                      bootstrap) override {
    bootstrap_.Bind(std::move(bootstrap));
  }

  void BootstrapLegacyIpc(
      mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> receiver) override {
    mojo::FusePipes(std::move(receiver), std::move(legacy_ipc_bootstrap_));
  }

 private:
  mojo::PendingRemote<IPC::mojom::ChannelBootstrap> legacy_ipc_bootstrap_;
  mojo::Remote<content::mojom::ChildProcessHostBootstrap> bootstrap_;
  mojo::Remote<content::mojom::ChildProcess> disconnected_process_;
};

}  // namespace

void InitializeMojo() {
  mojo::core::Configuration config;
  config.max_message_num_bytes = 64 * 1024 * 1024;
  mojo::core::Init(config);
}

mojo::IncomingInvitation InitializeMojoIPCChannel() {
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_WIN)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif defined(OS_POSIX)
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif
  CHECK(endpoint.is_valid());
  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

ReplayProcess::ReplayProcess()
    : io_thread_("Chrome_ChildIOThread"),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      message_index_(0) {}

ReplayProcess::~ReplayProcess() {
  channel_.reset();

  // Signal this event before shutting down the service process. That way all
  // background threads can cleanup.
  shutdown_event_.Signal();
  io_thread_.Stop();
}

bool ReplayProcess::Initialize(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIpcFuzzerTestcase)) {
    LOG(ERROR) << "This binary shouldn't be executed directly, "
               << "please use tools/ipc_fuzzer/scripts/play_testcase.py";
    return false;
  }

  // Log to both stderr and file destinations.
  logging::SetMinLogLevel(logging::LOG_ERROR);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = FILE_PATH_LITERAL("ipc_replay.log");
  logging::InitLogging(settings);

  // Make sure to initialize Mojo before starting the IO thread.
  InitializeMojo();

  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

#if defined(OS_POSIX)
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(service_manager::kMojoIPCChannel,
             service_manager::kMojoIPCChannel +
                 base::GlobalDescriptors::kBaseDescriptor);
#endif

  mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
      io_thread_.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));
  mojo_invitation_ =
      std::make_unique<mojo::IncomingInvitation>(InitializeMojoIPCChannel());

  return true;
}

void ReplayProcess::OpenChannel() {
  DCHECK(mojo_invitation_);
  mojo::PendingRemote<IPC::mojom::ChannelBootstrap> bootstrap;
  auto bootstrap_receiver = bootstrap.InitWithNewPipeAndPassReceiver();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeChildProcessImpl>(std::move(bootstrap)),
      mojo::PendingReceiver<content::mojom::ChildProcess>(
          mojo_invitation_->ExtractMessagePipe(0)));
  channel_ = IPC::ChannelProxy::Create(
      IPC::ChannelMojo::CreateClientFactory(
          bootstrap_receiver.PassPipe(), io_thread_.task_runner(),
          base::ThreadTaskRunnerHandle::Get()),
      this, io_thread_.task_runner(), base::ThreadTaskRunnerHandle::Get());
}

bool ReplayProcess::OpenTestcase() {
  base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kIpcFuzzerTestcase);
  return MessageFile::Read(path, &messages_);
}

void ReplayProcess::SendNextMessage() {
  if (message_index_ >= messages_.size()) {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    return;
  }

  std::unique_ptr<IPC::Message> message =
      std::move(messages_[message_index_++]);

  if (!channel_->Send(message.release())) {
    LOG(ERROR) << "ChannelProxy::Send() failed after "
               << message_index_ << " messages";
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

void ReplayProcess::Run() {
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
              base::BindRepeating(&ReplayProcess::SendNextMessage,
                                  base::Unretained(this)));
  base::RunLoop().Run();
}

bool ReplayProcess::OnMessageReceived(const IPC::Message& msg) {
  return true;
}

void ReplayProcess::OnChannelError() {
  LOG(ERROR) << "Channel error, quitting after "
             << message_index_ << " messages";
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace ipc_fuzzer
