// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_desktop_environment.h"

#include <utility>

#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/fake_desktop_capturer.h"

namespace remoting {

FakeInputInjector::FakeInputInjector() {}
FakeInputInjector::~FakeInputInjector() {}

void FakeInputInjector::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
}

void FakeInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (key_events_)
    key_events_->push_back(event);
}

void FakeInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  if (text_events_)
    text_events_->push_back(event);
}

void FakeInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  if (mouse_events_)
    mouse_events_->push_back(event);
}

void FakeInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
  if (touch_events_)
    touch_events_->push_back(event);
}

void FakeInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  if (clipboard_events_)
    clipboard_events_->push_back(event);
}

FakeScreenControls::FakeScreenControls() {}
FakeScreenControls::~FakeScreenControls() {}

void FakeScreenControls::SetScreenResolution(
    const ScreenResolution& resolution) {
}

FakeDesktopEnvironment::FakeDesktopEnvironment() {}

FakeDesktopEnvironment::~FakeDesktopEnvironment() {}

// DesktopEnvironment implementation.
scoped_ptr<AudioCapturer> FakeDesktopEnvironment::CreateAudioCapturer() {
  return nullptr;
}

scoped_ptr<InputInjector> FakeDesktopEnvironment::CreateInputInjector() {
  scoped_ptr<FakeInputInjector> result(new FakeInputInjector());
  last_input_injector_ = result->AsWeakPtr();
  return std::move(result);
}

scoped_ptr<ScreenControls> FakeDesktopEnvironment::CreateScreenControls() {
  return make_scoped_ptr(new FakeScreenControls());
}

scoped_ptr<webrtc::DesktopCapturer>
FakeDesktopEnvironment::CreateVideoCapturer() {
  scoped_ptr<protocol::FakeDesktopCapturer> result(
      new protocol::FakeDesktopCapturer());
  if (!frame_generator_.is_null())
    result->set_frame_generator(frame_generator_);
  return std::move(result);
}

scoped_ptr<webrtc::MouseCursorMonitor>
FakeDesktopEnvironment::CreateMouseCursorMonitor() {
  return make_scoped_ptr(new FakeMouseCursorMonitor());
}

std::string FakeDesktopEnvironment::GetCapabilities() const {
  return std::string();
}

void FakeDesktopEnvironment::SetCapabilities(const std::string& capabilities) {}

scoped_ptr<GnubbyAuthHandler> FakeDesktopEnvironment::CreateGnubbyAuthHandler(
    protocol::ClientStub* client_stub) {
  return nullptr;
}

FakeDesktopEnvironmentFactory::FakeDesktopEnvironmentFactory() {}
FakeDesktopEnvironmentFactory::~FakeDesktopEnvironmentFactory() {}

// DesktopEnvironmentFactory implementation.
scoped_ptr<DesktopEnvironment> FakeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  scoped_ptr<FakeDesktopEnvironment> result(new FakeDesktopEnvironment());
  result->set_frame_generator(frame_generator_);
  last_desktop_environment_ = result->AsWeakPtr();
  return std::move(result);
}

void FakeDesktopEnvironmentFactory::SetEnableCurtaining(bool enable) {}

bool FakeDesktopEnvironmentFactory::SupportsAudioCapture() const {
  return false;
}

void FakeDesktopEnvironmentFactory::SetEnableGnubbyAuth(bool enable) {}


}  // namespace remoting
