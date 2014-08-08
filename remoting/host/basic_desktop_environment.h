// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
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
  virtual ~BasicDesktopEnvironment();

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer() OVERRIDE;
  virtual scoped_ptr<InputInjector> CreateInputInjector() OVERRIDE;
  virtual scoped_ptr<ScreenControls> CreateScreenControls() OVERRIDE;
  virtual scoped_ptr<webrtc::ScreenCapturer> CreateVideoCapturer() OVERRIDE;
  virtual scoped_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      OVERRIDE;
  virtual std::string GetCapabilities() const OVERRIDE;
  virtual void SetCapabilities(const std::string& capabilities) OVERRIDE;
  virtual scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) OVERRIDE;

 protected:
  friend class BasicDesktopEnvironmentFactory;

  BasicDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
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

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Options shared between |ScreenCapturer| and |MouseCursorMonitor|. It
  // might contain expensive resources, thus justifying the sharing.
  // Also: it's dynamically allocated to avoid having to bring in
  // desktop_capture_options.h which brings in X11 headers which causes hard to
  // find build errors.
  scoped_ptr<webrtc::DesktopCaptureOptions> desktop_capture_options_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironment);
};

// Used to create |BasicDesktopEnvironment| instances.
class BasicDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  BasicDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~BasicDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory implementation.
  virtual bool SupportsAudioCapture() const OVERRIDE;

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

 private:
  // Task runner on which methods of DesktopEnvironmentFactory interface should
  // be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
