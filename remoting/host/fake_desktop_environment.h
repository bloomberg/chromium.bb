// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_

#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_screen_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"

namespace remoting {

class FakeInputInjector : public InputInjector {
 public:
  FakeInputInjector();
  virtual ~FakeInputInjector();

  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectTextEvent(const protocol::TextEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
};

class FakeScreenControls : public ScreenControls {
 public:
  FakeScreenControls();
  virtual ~FakeScreenControls();

  // ScreenControls implementation.
  virtual void SetScreenResolution(const ScreenResolution& resolution) OVERRIDE;
};

class FakeDesktopEnvironment : public DesktopEnvironment {
 public:
  FakeDesktopEnvironment();
  virtual ~FakeDesktopEnvironment();

  // Sets frame generator to be used for FakeScreenCapturer created by
  // FakeDesktopEnvironment.
  void set_frame_generator(FakeScreenCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer() OVERRIDE;
  virtual scoped_ptr<InputInjector> CreateInputInjector() OVERRIDE;
  virtual scoped_ptr<ScreenControls> CreateScreenControls() OVERRIDE;
  virtual scoped_ptr<webrtc::ScreenCapturer> CreateVideoCapturer() OVERRIDE;
  virtual std::string GetCapabilities() const OVERRIDE;
  virtual void SetCapabilities(const std::string& capabilities) OVERRIDE;
  virtual scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) OVERRIDE;

 private:
  FakeScreenCapturer::FrameGenerator frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironment);
};

class FakeDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  FakeDesktopEnvironmentFactory();
  virtual ~FakeDesktopEnvironmentFactory();

  // Sets frame generator to be used for FakeScreenCapturer created by
  // FakeDesktopEnvironment.
  void set_frame_generator(FakeScreenCapturer::FrameGenerator frame_generator) {
    frame_generator_ = frame_generator;
  }

  // DesktopEnvironmentFactory implementation.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) OVERRIDE;
  virtual void SetEnableCurtaining(bool enable) OVERRIDE;
  virtual bool SupportsAudioCapture() const OVERRIDE;
  virtual void SetEnableGnubbyAuth(bool enable) OVERRIDE;

 private:
  FakeScreenCapturer::FrameGenerator frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_DESKTOP_ENVIRONMENT_H_
