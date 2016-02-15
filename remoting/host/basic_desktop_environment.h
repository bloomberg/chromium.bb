// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/desktop_environment.h"

namespace webrtc {

class DesktopCaptureOptions;

}  // namespace webrtc

namespace remoting {

class GnubbyAuthHandler;

// Used to create audio/video capturers and event executor that work with
// the local console.
class BasicDesktopEnvironment : public DesktopEnvironment {
 public:
  ~BasicDesktopEnvironment() override;

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

 protected:
  friend class BasicDesktopEnvironmentFactory;

  BasicDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      bool supports_touch_events);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner()
      const {
    return video_capture_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

  webrtc::DesktopCaptureOptions* desktop_capture_options() {
    return desktop_capture_options_.get();
  }

 private:
  // Task runner on which methods of DesktopEnvironment interface should be
  // called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run video capturer.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Options shared between |DesktopCapturer| and |MouseCursorMonitor|. It
  // might contain expensive resources, thus justifying the sharing.
  // Also: it's dynamically allocated to avoid having to bring in
  // desktop_capture_options.h which brings in X11 headers which causes hard to
  // find build errors.
  scoped_ptr<webrtc::DesktopCaptureOptions> desktop_capture_options_;

  // True if the touch events capability should be offered.
  const bool supports_touch_events_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironment);
};

// Used to create |BasicDesktopEnvironment| instances.
class BasicDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  BasicDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~BasicDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  bool SupportsAudioCapture() const override;

  void set_supports_touch_events(bool enable) {
    supports_touch_events_ = enable;
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner()
      const {
    return video_capture_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

  bool supports_touch_events() const { return supports_touch_events_; }

 private:
  // Task runner on which methods of DesktopEnvironmentFactory interface should
  // be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run video capture tasks.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // True if the touch events capability should be offered by the
  // DesktopEnvironment instances.
  bool supports_touch_events_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
