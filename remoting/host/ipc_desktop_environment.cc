// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/video_frame_capturer.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace remoting {

IpcDesktopEnvironment::IpcDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DesktopSessionConnector* desktop_session_connector,
    ClientSession* client)
    : DesktopEnvironment(
          AudioCapturer::Create(),
          EventExecutor::Create(input_task_runner, ui_task_runner),
          VideoFrameCapturer::Create()),
      network_task_runner_(network_task_runner),
      desktop_session_connector_(desktop_session_connector),
      client_(client),
      connected_(false) {
}

bool IpcDesktopEnvironment::OnMessageReceived(const IPC::Message& message) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return false;
}

void IpcDesktopEnvironment::OnChannelConnected(int32 peer_pid) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: network <- desktop (" << peer_pid << ")";
}

void IpcDesktopEnvironment::OnChannelError() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  desktop_channel_.reset();
}

void IpcDesktopEnvironment::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(!connected_);

  connected_ = true;
  desktop_session_connector_->ConnectTerminal(this);

  DesktopEnvironment::Start(client_clipboard.Pass());
}

void IpcDesktopEnvironment::DisconnectClient() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  client_->Disconnect();
}

void IpcDesktopEnvironment::OnDesktopSessionAgentAttached(
    IPC::PlatformFileForTransit desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

#if defined(OS_WIN)
  // On Windows: |desktop_process| is a valid handle, but |desktop_pipe| needs
  // to be duplicated from the desktop process.
  base::win::ScopedHandle pipe;
  if (!DuplicateHandle(desktop_process, desktop_pipe, GetCurrentProcess(),
                       pipe.Receive(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate the desktop-to-network"
                               " pipe handle";
    return;
  }

  base::ClosePlatformFile(desktop_process);

  // Connect to the desktop process.
  desktop_channel_.reset(new IPC::ChannelProxy(IPC::ChannelHandle(pipe),
                                               IPC::Channel::MODE_CLIENT,
                                               this,
                                               network_task_runner_));
#elif defined(OS_POSIX)
  // On posix: both |desktop_process| and |desktop_pipe| are valid file
  // descriptors.
  DCHECK(desktop_process.auto_close);
  DCHECK(desktop_pipe.auto_close);

  base::ClosePlatformFile(desktop_process.fd);

  // Connect to the desktop process.
  desktop_channel_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle("", desktop_pipe),
      IPC::Channel::MODE_CLIENT,
      this,
      network_task_runner_));
#else
#error Unsupported platform.
#endif
}

IpcDesktopEnvironment::~IpcDesktopEnvironment() {
  if (connected_) {
    connected_ = false;
    desktop_session_connector_->DisconnectTerminal(this);
  }
}

}  // namespace remoting
