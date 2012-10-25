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
#include "base/run_loop.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session_agent.h"
#include "remoting/host/host_exit_codes.h"

const char kIoThreadName[] = "I/O thread";

namespace remoting {

DesktopProcess::DesktopProcess(const std::string& daemon_channel_name)
    : daemon_channel_name_(daemon_channel_name) {
}

DesktopProcess::~DesktopProcess() {
}

bool DesktopProcess::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingDaemonDesktopMsg_Crash, OnCrash)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DesktopProcess::OnChannelConnected(int32 peer_pid) {
  VLOG(1) << "IPC: desktop <- daemon (" << peer_pid << ")";

  NOTIMPLEMENTED();
}

void DesktopProcess::OnChannelError() {
  // Shutdown the desktop process.
  daemon_channel_.reset();
  desktop_agent_.reset();
}

int DesktopProcess::Run() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  {
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
        new remoting::AutoThreadTaskRunner(
            MessageLoop::current()->message_loop_proxy(),
            MessageLoop::QuitClosure());

    // Launch the I/O thread.
    scoped_refptr<AutoThreadTaskRunner> io_task_runner =
        AutoThread::CreateWithType(kIoThreadName, ui_task_runner,
                                   MessageLoop::TYPE_IO);

    // Create a desktop agent.
    desktop_agent_ = DesktopSessionAgent::Create(ui_task_runner,
                                                 io_task_runner);

    // Start the agent and create an IPC channel to talk to it. It is safe to
    // use base::Unretained(this) here because the message loop below will run
    // until |desktop_agent_| is completely destroyed.
    IPC::PlatformFileForTransit desktop_pipe;
    if (!desktop_agent_->Start(base::Bind(&DesktopProcess::OnChannelError,
                                          base::Unretained(this)),
                               &desktop_pipe)) {
      desktop_agent_.reset();
      return kInitializationFailed;
    }

    // Connect to the daemon.
    daemon_channel_.reset(new IPC::ChannelProxy(daemon_channel_name_,
                                                IPC::Channel::MODE_CLIENT,
                                                this,
                                                io_task_runner));

    // Pass |desktop_pipe| to the daemon.
    daemon_channel_->Send(
        new ChromotingDesktopDaemonMsg_DesktopAttached(desktop_pipe));
  }

  // Run the UI message loop.
  base::RunLoop run_loop;
  run_loop.Run();

  DCHECK(!daemon_channel_);
  DCHECK(!desktop_agent_);
  return kSuccessExitCode;
}

void DesktopProcess::OnCrash(const std::string& function_name,
                             const std::string& file_name,
                             const int& line_number) {
  // The daemon requested us to crash the process.
  CHECK(false);
}

} // namespace remoting
