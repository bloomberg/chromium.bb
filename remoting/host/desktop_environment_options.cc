// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment_options.h"

#include <string>
#include <utility>

#include "base/optional.h"

namespace remoting {

using DesktopCaptureOptions = webrtc::DesktopCaptureOptions;

// static
DesktopEnvironmentOptions DesktopEnvironmentOptions::CreateDefault() {
  DesktopEnvironmentOptions options;
  options.desktop_capture_options_ = DesktopCaptureOptions::CreateDefault();
  options.Initialize();
  return options;
}

DesktopEnvironmentOptions::DesktopEnvironmentOptions() {
  Initialize();
}

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

void DesktopEnvironmentOptions::Initialize() {
  desktop_capture_options_.set_detect_updated_region(true);
#if defined (OS_WIN)
  // TODO(joedow): Enable the DirectX capturer once the blocking bugs are fixed.
  // desktop_capture_options_.set_allow_directx_capturer(true);
#endif
}

const DesktopCaptureOptions*
DesktopEnvironmentOptions::desktop_capture_options() const {
  return &desktop_capture_options_;
}

DesktopCaptureOptions*
DesktopEnvironmentOptions::desktop_capture_options() {
  return &desktop_capture_options_;
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

void DesktopEnvironmentOptions::ApplyHostSessionOptions(
    const HostSessionOptions& options) {
#if defined(OS_WIN)
  base::Optional<std::string> directx_capturer =
      options.Get("DirectX-Capturer");
  if (directx_capturer) {
    desktop_capture_options_.set_allow_directx_capturer(
        *directx_capturer == "true");
  }
#endif
  // This field is for test purpose. Usually it should not be set to false.
  base::Optional<std::string> detect_updated_region =
      options.Get("Detect-Updated-Region");
  if (detect_updated_region) {
    desktop_capture_options_.set_detect_updated_region(
        *detect_updated_region == "true");
  }
}

}  // namespace remoting
