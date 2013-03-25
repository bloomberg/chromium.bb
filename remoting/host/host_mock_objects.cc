// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/transport.h"

namespace remoting {

MockDesktopEnvironment::MockDesktopEnvironment() {}

MockDesktopEnvironment::~MockDesktopEnvironment() {}

scoped_ptr<AudioCapturer> MockDesktopEnvironment::CreateAudioCapturer() {
  return scoped_ptr<AudioCapturer>(CreateAudioCapturerPtr());
}

scoped_ptr<InputInjector> MockDesktopEnvironment::CreateInputInjector() {
  return scoped_ptr<InputInjector>(CreateInputInjectorPtr());
}

scoped_ptr<ScreenControls> MockDesktopEnvironment::CreateScreenControls() {
  return scoped_ptr<ScreenControls>(CreateScreenControlsPtr());
}

scoped_ptr<media::ScreenCapturer>
MockDesktopEnvironment::CreateVideoCapturer() {
  return scoped_ptr<media::ScreenCapturer>(CreateVideoCapturerPtr());
}

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory() {}

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() {}

scoped_ptr<DesktopEnvironment> MockDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return scoped_ptr<DesktopEnvironment>(CreatePtr());
}

MockInputInjector::MockInputInjector() {}

MockInputInjector::~MockInputInjector() {}

void MockInputInjector::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  StartPtr(client_clipboard.get());
}

MockDisconnectWindow::MockDisconnectWindow() {}

MockDisconnectWindow::~MockDisconnectWindow() {}

scoped_ptr<DisconnectWindow> DisconnectWindow::Create(
    const UiStrings* ui_strings) {
  return scoped_ptr<DisconnectWindow>(new MockDisconnectWindow());
}

MockContinueWindow::MockContinueWindow() {}

MockContinueWindow::~MockContinueWindow() {}

scoped_ptr<ContinueWindow> ContinueWindow::Create(const UiStrings* ui_strings) {
  return scoped_ptr<ContinueWindow>(new MockContinueWindow());
}

MockClientSessionControl::MockClientSessionControl() {}

MockClientSessionControl::~MockClientSessionControl() {}

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockHostStatusObserver::MockHostStatusObserver() {}

MockHostStatusObserver::~MockHostStatusObserver() {}

}  // namespace remoting
