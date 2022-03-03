// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/desktop_process.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/crash_process.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_agent.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

namespace remoting {

DesktopProcess::DesktopProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle daemon_channel_handle)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      io_task_runner_(io_task_runner),
      daemon_channel_handle_(std::move(daemon_channel_handle)) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(base::CurrentUIThread::IsSet());
}

DesktopProcess::~DesktopProcess() {
  DCHECK(!daemon_channel_);
  DCHECK(!desktop_agent_.get());
}

DesktopEnvironmentFactory& DesktopProcess::desktop_environment_factory() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return *desktop_environment_factory_;
}

void DesktopProcess::OnNetworkProcessDisconnected() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  OnChannelError();
}

void DesktopProcess::CrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(ERROR) << "Asking the daemon process to crash the network process. "
             << "Request originated from: " << location.ToString();
  desktop_session_request_handler_->CrashNetworkProcess();
}

void DesktopProcess::InjectSas() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  desktop_session_request_handler_->InjectSecureAttentionSequence();
}

void DesktopProcess::LockWorkstation() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
#if defined(OS_WIN)
  if (base::win::OSInfo::GetInstance()->version_type() ==
      base::win::VersionType::SUITE_HOME) {
    return;
  }

  if (!::LockWorkStation()) {
    LOG(ERROR) << "LockWorkStation() failed: " << ::GetLastError();
  }
#else
  NOTREACHED();
#endif  // defined(OS_WIN)
}

bool DesktopProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingDaemonMsg_Crash, OnCrash)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void DesktopProcess::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- daemon (" << peer_pid << ")";
}

void DesktopProcess::OnChannelError() {
  // Shutdown the desktop process.
  daemon_channel_.reset();
  if (desktop_agent_.get()) {
    desktop_agent_->Stop();
    desktop_agent_ = nullptr;
  }
  desktop_session_request_handler_.reset();

  caller_task_runner_ = nullptr;
  input_task_runner_ = nullptr;
  io_task_runner_ = nullptr;
  desktop_environment_factory_.reset();
}

bool DesktopProcess::Start(
    std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_environment_factory_);
  DCHECK(desktop_environment_factory);

  desktop_environment_factory_ = std::move(desktop_environment_factory);

  // Launch the audio capturing thread.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner;
#if defined(OS_WIN)
  // On Windows the AudioCapturer requires COM, so we run a single-threaded
  // apartment, which requires a UI thread.
  audio_task_runner = AutoThread::CreateWithLoopAndComInitTypes(
      "ChromotingAudioThread", caller_task_runner_, base::MessagePumpType::UI,
      AutoThread::COM_INIT_STA);
#else // !defined(OS_WIN)
  audio_task_runner = AutoThread::CreateWithType(
      "ChromotingAudioThread", caller_task_runner_, base::MessagePumpType::IO);
#endif  // !defined(OS_WIN)

  // Create a desktop agent.
  desktop_agent_ =
      new DesktopSessionAgent(audio_task_runner, caller_task_runner_,
                              input_task_runner_, io_task_runner_);

  // Start the agent and create an IPC channel to talk to it.
  mojo::ScopedMessagePipeHandle desktop_pipe =
      desktop_agent_->Start(weak_factory_.GetWeakPtr());

  // Connect to the daemon.
  daemon_channel_ = IPC::ChannelProxy::Create(
      daemon_channel_handle_.release(), IPC::Channel::MODE_CLIENT, this,
      io_task_runner_, base::ThreadTaskRunnerHandle::Get());

  daemon_channel_->GetRemoteAssociatedInterface(
      &desktop_session_request_handler_);

  // Pass |desktop_pipe| to the daemon.
  desktop_session_request_handler_->ConnectDesktopChannel(
      std::move(desktop_pipe));

  return true;
}

void DesktopProcess::OnCrash(const std::string& function_name,
                             const std::string& file_name,
                             const int& line_number) {
  // The daemon requested us to crash the process.
  CrashProcess(function_name, file_name, line_number);
}

} // namespace remoting
