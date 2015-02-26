// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_

#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_desktop_capturer.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"

namespace remoting {

class FakeInputInjector : public InputInjector {
 public:
  FakeInputInjector();
  ~FakeInputInjector() override;

  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;
};

class FakeScreenControls : public ScreenControls {
 public:
  FakeScreenControls();
  ~FakeScreenControls() override;

  // ScreenControls implementation.
  void SetScreenResolution(const ScreenResolution& resolution) override;
};

class FakeDesktopEnvironment : public DesktopEnvironment {
 public:
  FakeDesktopEnvironment();
  ~FakeDesktopEnvironment() override;

  // Sets frame generator to be used for FakeDesktopCapturer created by
  // FakeDesktopEnvironment.
  void set_frame_generator(
      FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironment implementation.
  scoped_ptr<AudioCapturer> CreateAudioCapturer() override;
  scoped_ptr<InputInjector> CreateInputInjector() override;
  scoped_ptr<ScreenControls> CreateScreenControls() override;
  scoped_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  scoped_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor() override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) override;

 private:
  FakeDesktopCapturer::FrameGenerator frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironment);
};

class FakeDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  FakeDesktopEnvironmentFactory();
  ~FakeDesktopEnvironmentFactory() override;

  // Sets frame generator to be used for FakeDesktopCapturer created by
  // FakeDesktopEnvironment.
  void set_frame_generator(
      FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironmentFactory implementation.
  scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) override;
  void SetEnableCurtaining(bool enable) override;
  bool SupportsAudioCapture() const override;
  void SetEnableGnubbyAuth(bool enable) override;

 private:
  FakeDesktopCapturer::FrameGenerator frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
