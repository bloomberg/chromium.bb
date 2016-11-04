// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment_options.h"

#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace remoting {

DesktopEnvironmentOptions::DesktopEnvironmentOptions() = default;
DesktopEnvironmentOptions::DesktopEnvironmentOptions(
    DesktopEnvironmentOptions&& other) = default;
DesktopEnvironmentOptions::DesktopEnvironmentOptions(
    const DesktopEnvironmentOptions& other) = default;
DesktopEnvironmentOptions::~DesktopEnvironmentOptions() = default;
DesktopEnvironmentOptions&
DesktopEnvironmentOptions::operator=(
    DesktopEnvironmentOptions&& other) = default;
DesktopEnvironmentOptions&
DesktopEnvironmentOptions::operator=(
    const DesktopEnvironmentOptions& other) = default;

DesktopEnvironmentOptions::
DesktopCaptureOptionsPtr::DesktopCaptureOptionsPtr()
    : desktop_capture_options(new webrtc::DesktopCaptureOptions(
          webrtc::DesktopCaptureOptions::CreateDefault())) {
  desktop_capture_options->set_detect_updated_region(true);
}

DesktopEnvironmentOptions::
DesktopCaptureOptionsPtr::DesktopCaptureOptionsPtr(
    DesktopCaptureOptionsPtr&& other) = default;

DesktopEnvironmentOptions::
DesktopCaptureOptionsPtr::DesktopCaptureOptionsPtr(
    const DesktopCaptureOptionsPtr& other) {
  desktop_capture_options.reset(new webrtc::DesktopCaptureOptions(
        *other.desktop_capture_options));
}

DesktopEnvironmentOptions::
DesktopCaptureOptionsPtr::~DesktopCaptureOptionsPtr() = default;

DesktopEnvironmentOptions::DesktopCaptureOptionsPtr&
DesktopEnvironmentOptions::DesktopCaptureOptionsPtr::operator=(
    DesktopEnvironmentOptions::DesktopCaptureOptionsPtr&& other) = default;

DesktopEnvironmentOptions::DesktopCaptureOptionsPtr&
DesktopEnvironmentOptions::DesktopCaptureOptionsPtr::operator=(
    const DesktopEnvironmentOptions::DesktopCaptureOptionsPtr& other) {
  desktop_capture_options.reset(new webrtc::DesktopCaptureOptions(
        *other.desktop_capture_options));
  return *this;
}

webrtc::DesktopCaptureOptions*
DesktopEnvironmentOptions::desktop_capture_options() {
  return desktop_capture_options_.desktop_capture_options.get();
}

bool DesktopEnvironmentOptions::enable_curtaining() const {
  return enable_curtaining_;
}

void DesktopEnvironmentOptions::set_enable_curtaining(bool enabled) {
  enable_curtaining_ = enabled;
}

bool DesktopEnvironmentOptions::enable_user_interface() const {
  return enable_user_interface_;
}

void DesktopEnvironmentOptions::set_enable_user_interface(bool enabled) {
  enable_user_interface_ = enabled;
}

}  // namespace remoting
