// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment_factory.h"

#include <utility>

#include "ipc/ipc_channel_proxy.h"
#include "base/platform_file.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/ipc_desktop_environment.h"

namespace remoting {

IpcDesktopEnvironmentFactory::IpcDesktopEnvironmentFactory(
    IPC::ChannelProxy* daemon_channel,
    scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner)
    : DesktopEnvironmentFactory(input_task_runner, ui_task_runner),
      daemon_channel_(daemon_channel),
      audio_capture_task_runner_(audio_capture_task_runner),
      network_task_runner_(network_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      next_id_(0) {
}

IpcDesktopEnvironmentFactory::~IpcDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> IpcDesktopEnvironmentFactory::Create() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  scoped_refptr<DesktopSessionProxy> desktop_session_proxy(
      new DesktopSessionProxy(audio_capture_task_runner_,
                              network_task_runner_,
                              video_capture_task_runner_));

  return scoped_ptr<DesktopEnvironment>(new IpcDesktopEnvironment(
      input_task_runner_, network_task_runner_, ui_task_runner_,
      this, desktop_session_proxy));
}

void IpcDesktopEnvironmentFactory::ConnectTerminal(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  int id = next_id_++;
  bool inserted = active_connections_.insert(
      std::make_pair(id, desktop_session_proxy)).second;
  CHECK(inserted);

  VLOG(1) << "Network: registered desktop environment " << id;
  daemon_channel_->Send(new ChromotingNetworkHostMsg_ConnectTerminal(id));
}

void IpcDesktopEnvironmentFactory::DisconnectTerminal(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second.get() == desktop_session_proxy.get())
      break;
  }

  if (i != active_connections_.end()) {
    int id = i->first;
    active_connections_.erase(i);

    VLOG(1) << "Network: unregistered desktop environment " << id;
    daemon_channel_->Send(new ChromotingNetworkHostMsg_DisconnectTerminal(id));
  }
}

void IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached(
    int terminal_id,
    IPC::PlatformFileForTransit desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(FROM_HERE, base::Bind(
        &IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached,
        base::Unretained(this), terminal_id, desktop_process, desktop_pipe));
    return;
  }

  ActiveConnectionsList::iterator i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    i->second->DetachFromDesktop();
    i->second->AttachToDesktop(desktop_process, desktop_pipe);
  } else {
#if defined(OS_POSIX)
    DCHECK(desktop_process.auto_close);
    DCHECK(desktop_pipe.auto_close);

    base::ClosePlatformFile(desktop_process.fd);
    base::ClosePlatformFile(desktop_pipe.fd);
#elif defined(OS_WIN)
    base::ClosePlatformFile(desktop_process);
#endif  // defined(OS_WIN)
  }
}

void IpcDesktopEnvironmentFactory::OnTerminalDisconnected(int terminal_id) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(FROM_HERE, base::Bind(
        &IpcDesktopEnvironmentFactory::OnTerminalDisconnected,
        base::Unretained(this), terminal_id));
    return;
  }

  ActiveConnectionsList::iterator i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy = i->second;
    active_connections_.erase(i);

    // Disconnect the client session.
    desktop_session_proxy->DisconnectSession();
  }
}

}  // namespace remoting
