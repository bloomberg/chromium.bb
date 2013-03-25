// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_sender.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"

namespace remoting {

IpcDesktopEnvironment::IpcDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    bool virtual_terminal) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());

  desktop_session_proxy_ = new DesktopSessionProxy(audio_task_runner,
                                                   caller_task_runner,
                                                   io_task_runner,
                                                   capture_task_runner,
                                                   client_session_control);

  desktop_session_proxy_->ConnectToDesktopSession(desktop_session_connector,
                                                  virtual_terminal);
}

IpcDesktopEnvironment::~IpcDesktopEnvironment() {
}

scoped_ptr<AudioCapturer> IpcDesktopEnvironment::CreateAudioCapturer() {
  return desktop_session_proxy_->CreateAudioCapturer();
}

scoped_ptr<InputInjector> IpcDesktopEnvironment::CreateInputInjector() {
  return desktop_session_proxy_->CreateInputInjector();
}

scoped_ptr<ScreenControls> IpcDesktopEnvironment::CreateScreenControls() {
  return desktop_session_proxy_->CreateScreenControls();
}

scoped_ptr<media::ScreenCapturer> IpcDesktopEnvironment::CreateVideoCapturer() {
  return desktop_session_proxy_->CreateVideoCapturer();
}

IpcDesktopEnvironmentFactory::IpcDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Sender* daemon_channel)
    : audio_task_runner_(audio_task_runner),
      caller_task_runner_(caller_task_runner),
      capture_task_runner_(capture_task_runner),
      io_task_runner_(io_task_runner),
      curtain_activated_(false),
      daemon_channel_(daemon_channel),
      connector_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      next_id_(0) {
}

IpcDesktopEnvironmentFactory::~IpcDesktopEnvironmentFactory() {
}

void IpcDesktopEnvironmentFactory::SetActivated(bool activated) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  curtain_activated_ = activated;
}

scoped_ptr<DesktopEnvironment> IpcDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<DesktopEnvironment>(
      new IpcDesktopEnvironment(audio_task_runner_,
                                caller_task_runner_,
                                capture_task_runner_,
                                io_task_runner_,
                                client_session_control,
                                connector_factory_.GetWeakPtr(),
                                curtain_activated_));
}

bool IpcDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

void IpcDesktopEnvironmentFactory::ConnectTerminal(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  int id = next_id_++;
  bool inserted = active_connections_.insert(
      std::make_pair(id, desktop_session_proxy)).second;
  CHECK(inserted);

  VLOG(1) << "Network: registered desktop environment " << id;

  daemon_channel_->Send(new ChromotingNetworkHostMsg_ConnectTerminal(
      id, resolution, virtual_terminal));
}

void IpcDesktopEnvironmentFactory::DisconnectTerminal(
    DesktopSessionProxy* desktop_session_proxy) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second == desktop_session_proxy)
      break;
  }

  if (i != active_connections_.end()) {
    int id = i->first;
    active_connections_.erase(i);

    VLOG(1) << "Network: unregistered desktop environment " << id;
    daemon_channel_->Send(new ChromotingNetworkHostMsg_DisconnectTerminal(id));
  }
}

void IpcDesktopEnvironmentFactory::SetScreenResolution(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second == desktop_session_proxy)
      break;
  }

  if (i != active_connections_.end()) {
    daemon_channel_->Send(new ChromotingNetworkDaemonMsg_SetScreenResolution(
        i->first, resolution));
  }
}

void IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached(
    int terminal_id,
    base::ProcessHandle desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(FROM_HERE, base::Bind(
        &IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached,
        base::Unretained(this), terminal_id, desktop_process, desktop_pipe));
    return;
  }

  ActiveConnectionsList::iterator i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    i->second->DetachFromDesktop();
    i->second->AttachToDesktop(desktop_process, desktop_pipe);
  } else {
    base::CloseProcessHandle(desktop_process);

#if defined(OS_POSIX)
    DCHECK(desktop_pipe.auto_close);

    base::ClosePlatformFile(desktop_pipe.fd);
#endif  // defined(OS_POSIX)
  }
}

void IpcDesktopEnvironmentFactory::OnTerminalDisconnected(int terminal_id) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(FROM_HERE, base::Bind(
        &IpcDesktopEnvironmentFactory::OnTerminalDisconnected,
        base::Unretained(this), terminal_id));
    return;
  }

  ActiveConnectionsList::iterator i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    DesktopSessionProxy* desktop_session_proxy = i->second;
    active_connections_.erase(i);

    // Disconnect the client session.
    desktop_session_proxy->DisconnectSession();
  }
}

}  // namespace remoting
