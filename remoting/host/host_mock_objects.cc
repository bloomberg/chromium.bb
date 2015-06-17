// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

MockDesktopEnvironment::MockDesktopEnvironment() {}

MockDesktopEnvironment::~MockDesktopEnvironment() {}

scoped_ptr<AudioCapturer> MockDesktopEnvironment::CreateAudioCapturer() {
  return make_scoped_ptr(CreateAudioCapturerPtr());
}

scoped_ptr<InputInjector> MockDesktopEnvironment::CreateInputInjector() {
  return make_scoped_ptr(CreateInputInjectorPtr());
}

scoped_ptr<ScreenControls> MockDesktopEnvironment::CreateScreenControls() {
  return make_scoped_ptr(CreateScreenControlsPtr());
}

scoped_ptr<webrtc::DesktopCapturer>
MockDesktopEnvironment::CreateVideoCapturer() {
  return make_scoped_ptr(CreateVideoCapturerPtr());
}

scoped_ptr<GnubbyAuthHandler>
MockDesktopEnvironment::CreateGnubbyAuthHandler(
    protocol::ClientStub* client_stub) {
  return make_scoped_ptr(CreateGnubbyAuthHandlerPtr(client_stub));
}

scoped_ptr<webrtc::MouseCursorMonitor>
MockDesktopEnvironment::CreateMouseCursorMonitor() {
  return make_scoped_ptr(CreateMouseCursorMonitorPtr());
}

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory() {}

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() {}

scoped_ptr<DesktopEnvironment> MockDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return make_scoped_ptr(CreatePtr());
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

MockVideoEncoder::MockVideoEncoder() {
}

MockVideoEncoder::~MockVideoEncoder() {
}

scoped_ptr<VideoPacket> MockVideoEncoder::Encode(
    const webrtc::DesktopFrame& frame) {
  return make_scoped_ptr(EncodePtr(frame));
}

}  // namespace remoting
