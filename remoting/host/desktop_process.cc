// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/desktop_process.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session_agent.h"

namespace remoting {

DesktopProcess::DesktopProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    const std::string& daemon_channel_name)
    : caller_task_runner_(caller_task_runner),
      daemon_channel_name_(daemon_channel_name) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
}

DesktopProcess::~DesktopProcess() {
  DCHECK(!daemon_channel_);
  DCHECK(!desktop_agent_);
}

void DesktopProcess::OnNetworkProcessDisconnected() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  OnChannelError();
}

void DesktopProcess::InjectSas() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  daemon_channel_->Send(new ChromotingDesktopDaemonMsg_InjectSas());
}

bool DesktopProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingDaemonDesktopMsg_Crash, OnCrash)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DesktopProcess::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- daemon (" << peer_pid << ")";
}

void DesktopProcess::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Shutdown the desktop process.
  daemon_channel_.reset();
  desktop_agent_->Stop();
  desktop_agent_ = NULL;
  caller_task_runner_ = NULL;
}

bool DesktopProcess::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Launch the audio capturing thread.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner;
#if defined(OS_WIN)
  // On Windows the AudioCapturer requires COM, so we run a single-threaded
  // apartment, which requires a UI thread.
  audio_task_runner = AutoThread::CreateWithLoopAndComInitTypes(
      "ChromotingAudioThread", caller_task_runner_, MessageLoop::TYPE_UI,
      AutoThread::COM_INIT_STA);
#else // !defined(OS_WIN)
  audio_task_runner = AutoThread::CreateWithType(
      "ChromotingAudioThread", caller_task_runner_, MessageLoop::TYPE_IO);
#endif // !defined(OS_WIN)

  // Launch the input thread.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner =
      AutoThread::CreateWithType("Input thread", caller_task_runner_,
                                 MessageLoop::TYPE_IO);

  // Launch the I/O thread.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner =
      AutoThread::CreateWithType("I/O thread", caller_task_runner_,
                                 MessageLoop::TYPE_IO);

  // Launch the video capture thread.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner =
      AutoThread::Create("Video capture thread", caller_task_runner_);

  // Create a desktop agent.
  desktop_agent_ = DesktopSessionAgent::Create(audio_task_runner,
                                               caller_task_runner_,
                                               input_task_runner,
                                               io_task_runner,
                                               video_capture_task_runner);

  // Start the agent and create an IPC channel to talk to it.
  IPC::PlatformFileForTransit desktop_pipe;
  if (!desktop_agent_->Start(AsWeakPtr(), &desktop_pipe)) {
    desktop_agent_ = NULL;
    caller_task_runner_ = NULL;
    return false;
  }

  // Connect to the daemon.
  daemon_channel_.reset(new IPC::ChannelProxy(daemon_channel_name_,
                                              IPC::Channel::MODE_CLIENT,
                                              this,
                                              io_task_runner));

  // Pass |desktop_pipe| to the daemon.
  daemon_channel_->Send(
      new ChromotingDesktopDaemonMsg_DesktopAttached(desktop_pipe));

  return true;
}

void DesktopProcess::OnCrash(const std::string& function_name,
                             const std::string& file_name,
                             const int& line_number) {
  // The daemon requested us to crash the process.
  CHECK(false);
}

} // namespace remoting
