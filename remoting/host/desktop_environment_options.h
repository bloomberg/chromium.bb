// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_OPTIONS_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_OPTIONS_H_

#include <memory>

namespace webrtc {
class DesktopCaptureOptions;
}  // namespace webrtc

namespace remoting {

// A container of options a DesktopEnvironment or its derived classes need to
// control the behavior.
class DesktopEnvironmentOptions final {
 public:
  DesktopEnvironmentOptions();
  DesktopEnvironmentOptions(DesktopEnvironmentOptions&& other);
  DesktopEnvironmentOptions(const DesktopEnvironmentOptions& other);
  ~DesktopEnvironmentOptions();

  DesktopEnvironmentOptions& operator=(DesktopEnvironmentOptions&& other);
  DesktopEnvironmentOptions& operator=(const DesktopEnvironmentOptions& other);

  bool enable_curtaining() const;
  void set_enable_curtaining(bool enabled);

  bool enable_user_interface() const;
  void set_enable_user_interface(bool enabled);

  webrtc::DesktopCaptureOptions* desktop_capture_options();

 private:
  // A copyable DesktopCaptureOptions without needing to include the definition
  // of DesktopCaptureOptions. Uses std::unique_ptr to avoid including
  // desktop_capture_options.h, which contains a set of evil macros from Xlib to
  // break build.
  struct DesktopCaptureOptionsPtr final {
    DesktopCaptureOptionsPtr();
    DesktopCaptureOptionsPtr(DesktopCaptureOptionsPtr&& other);
    DesktopCaptureOptionsPtr(const DesktopCaptureOptionsPtr& other);
    ~DesktopCaptureOptionsPtr();

    DesktopCaptureOptionsPtr& operator=(DesktopCaptureOptionsPtr&& other);
    DesktopCaptureOptionsPtr& operator=(const DesktopCaptureOptionsPtr& other);

    std::unique_ptr<webrtc::DesktopCaptureOptions> desktop_capture_options;
  };

  // True if the curtain mode should be enabled by the DesktopEnvironment
  // instances. Note, not all DesktopEnvironments support curtain mode.
  bool enable_curtaining_ = false;

  // True if a user-interactive window is showing up in it2me scenario.
  bool enable_user_interface_ = true;

  // The DesktopCaptureOptions to initialize DesktopCapturer.
  // |desktop_capture_options_| needs to be copyable and copy-assignable, but
  // std::unique_ptr is not copyable. So work around the issue by using
  // DesktopCaptureOptionsPtr.
  DesktopCaptureOptionsPtr desktop_capture_options_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_OPTIONS_H_
