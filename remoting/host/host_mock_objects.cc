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

scoped_ptr<AudioCapturer> MockDesktopEnvironment::CreateAudioCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) {
  return scoped_ptr<AudioCapturer>(CreateAudioCapturerPtr(audio_task_runner));
}

scoped_ptr<InputInjector> MockDesktopEnvironment::CreateInputInjector(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return scoped_ptr<InputInjector>(CreateInputInjectorPtr(input_task_runner,
                                                          ui_task_runner));
}

scoped_ptr<SessionController>
MockDesktopEnvironment::CreateSessionController() {
  return scoped_ptr<SessionController>(CreateSessionControllerPtr());
}

scoped_ptr<media::ScreenCapturer> MockDesktopEnvironment::CreateVideoCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) {
  return scoped_ptr<media::ScreenCapturer>(CreateVideoCapturerPtr(
      capture_task_runner, encode_task_runner));
}

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory() {}

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() {}

scoped_ptr<DesktopEnvironment> MockDesktopEnvironmentFactory::Create(
    const std::string& client_jid,
    const base::Closure& disconnect_callback) {
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

MockLocalInputMonitor::MockLocalInputMonitor() {}

MockLocalInputMonitor::~MockLocalInputMonitor() {}

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockHostStatusObserver::MockHostStatusObserver() {}

MockHostStatusObserver::~MockHostStatusObserver() {}

MockSessionController::MockSessionController() {}

MockSessionController::~MockSessionController() {}

}  // namespace remoting
