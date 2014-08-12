// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

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

scoped_ptr<webrtc::DesktopCapturer>
MockDesktopEnvironment::CreateVideoCapturer() {
  return scoped_ptr<webrtc::DesktopCapturer>(CreateVideoCapturerPtr());
}

scoped_ptr<GnubbyAuthHandler>
MockDesktopEnvironment::CreateGnubbyAuthHandler(
    protocol::ClientStub* client_stub) {
  return scoped_ptr<GnubbyAuthHandler>(CreateGnubbyAuthHandlerPtr(client_stub));
}

scoped_ptr<webrtc::MouseCursorMonitor>
MockDesktopEnvironment::CreateMouseCursorMonitor() {
  return scoped_ptr<webrtc::MouseCursorMonitor>(CreateMouseCursorMonitorPtr());
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

MockClientSessionControl::MockClientSessionControl() {}

MockClientSessionControl::~MockClientSessionControl() {}

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockHostStatusObserver::MockHostStatusObserver() {}

MockHostStatusObserver::~MockHostStatusObserver() {}

MockGnubbyAuthHandler::MockGnubbyAuthHandler() {}

MockGnubbyAuthHandler::~MockGnubbyAuthHandler() {}

MockMouseCursorMonitor::MockMouseCursorMonitor() {}

MockMouseCursorMonitor::~MockMouseCursorMonitor() {}

}  // namespace remoting
