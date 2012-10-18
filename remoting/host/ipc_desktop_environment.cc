// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/video_frame_capturer.h"

namespace remoting {

IpcDesktopEnvironment::IpcDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DesktopSessionConnector* desktop_session_connector,
    ClientSession* client)
    : DesktopEnvironment(
          AudioCapturer::Create(),
          EventExecutor::Create(input_task_runner, ui_task_runner),
          scoped_ptr<VideoFrameCapturer>(VideoFrameCapturer::Create())),
      desktop_session_connector_(desktop_session_connector),
      client_(client),
      connected_(false) {
}

void IpcDesktopEnvironment::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(!connected_);

  connected_ = true;
  desktop_session_connector_->ConnectTerminal(this);

  DesktopEnvironment::Start(client_clipboard.Pass());
}

void IpcDesktopEnvironment::DisconnectClient() {
  client_->Disconnect();
}

IpcDesktopEnvironment::~IpcDesktopEnvironment() {
  if (connected_) {
    connected_ = false;
    desktop_session_connector_->DisconnectTerminal(this);
  }
}

}  // namespace remoting
