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
  virtual ~FakeInputInjector();

  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) override;
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) override;
  virtual void InjectTextEvent(const protocol::TextEvent& event) override;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) override;
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) override;
};

class FakeScreenControls : public ScreenControls {
 public:
  FakeScreenControls();
  virtual ~FakeScreenControls();

  // ScreenControls implementation.
  virtual void SetScreenResolution(const ScreenResolution& resolution) override;
};

class FakeDesktopEnvironment : public DesktopEnvironment {
 public:
  FakeDesktopEnvironment();
  virtual ~FakeDesktopEnvironment();

  // Sets frame generator to be used for FakeDesktopCapturer created by
  // FakeDesktopEnvironment.
  void set_frame_generator(
      FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer() override;
  virtual scoped_ptr<InputInjector> CreateInputInjector() override;
  virtual scoped_ptr<ScreenControls> CreateScreenControls() override;
  virtual scoped_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  virtual scoped_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  virtual std::string GetCapabilities() const override;
  virtual void SetCapabilities(const std::string& capabilities) override;
  virtual scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) override;

 private:
  FakeDesktopCapturer::FrameGenerator frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironment);
};

class FakeDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  FakeDesktopEnvironmentFactory();
  virtual ~FakeDesktopEnvironmentFactory();

  // Sets frame generator to be used for FakeDesktopCapturer created by
  // FakeDesktopEnvironment.
  void set_frame_generator(
      FakeDesktopCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironmentFactory implementation.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) override;
  virtual void SetEnableCurtaining(bool enable) override;
  virtual bool SupportsAudioCapture() const override;
  virtual void SetEnableGnubbyAuth(bool enable) override;

 private:
  FakeDesktopCapturer::FrameGenerator frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
