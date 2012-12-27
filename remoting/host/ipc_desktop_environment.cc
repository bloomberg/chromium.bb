// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/capturer/capture_data.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/ipc_audio_capturer.h"
#include "remoting/host/ipc_event_executor.h"
#include "remoting/host/ipc_video_frame_capturer.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace remoting {

IpcDesktopEnvironment::IpcDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DesktopSessionConnector* desktop_session_connector,
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : DesktopEnvironment(
          scoped_ptr<AudioCapturer>(
              new IpcAudioCapturer(desktop_session_proxy)),
          scoped_ptr<EventExecutor>(
              new IpcEventExecutor(desktop_session_proxy)),
          scoped_ptr<VideoFrameCapturer>(
              new IpcVideoFrameCapturer(desktop_session_proxy))),
      network_task_runner_(network_task_runner),
      desktop_session_connector_(desktop_session_connector),
      desktop_session_proxy_(desktop_session_proxy),
      connected_(false) {
}

IpcDesktopEnvironment::~IpcDesktopEnvironment() {
  if (connected_) {
    connected_ = false;
    desktop_session_connector_->DisconnectTerminal(desktop_session_proxy_);
  }
}

void IpcDesktopEnvironment::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard,
    const std::string& client_jid,
    const base::Closure& disconnect_callback) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(!connected_);

  desktop_session_proxy_->Initialize(client_jid, disconnect_callback);

  // Register the proxy to receive AttachToDesktop() and DetachFromDesktop()
  // notifications.
  connected_ = true;
  desktop_session_connector_->ConnectTerminal(desktop_session_proxy_);

  DesktopEnvironment::Start(client_clipboard.Pass(), client_jid,
                            disconnect_callback);
}

}  // namespace remoting
