// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include "base/logging.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"

namespace remoting {

DesktopSessionAgent::~DesktopSessionAgent() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

bool DesktopSessionAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return false;
}

void DesktopSessionAgent::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- network (" << peer_pid << ")";

  NOTIMPLEMENTED();
}

void DesktopSessionAgent::OnChannelError() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Make sure the channel is closed.
  network_channel_.reset();

  // Notify the caller that |this| can be destroyed now.
  done_task_.Run();
}

bool DesktopSessionAgent::Start(const base::Closure& done_task,
                                IPC::PlatformFileForTransit* desktop_pipe_out) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  done_task_ = done_task;
  return DoCreateNetworkChannel(desktop_pipe_out, &network_channel_);
}

DesktopSessionAgent::DesktopSessionAgent(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

}  // namespace remoting
