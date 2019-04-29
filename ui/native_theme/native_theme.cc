// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ui {

NativeTheme::ExtraParams::ExtraParams() {
  memset(this, 0, sizeof(*this));
}

NativeTheme::ExtraParams::ExtraParams(const ExtraParams& other) {
  memcpy(this, &other, sizeof(*this));
}

void NativeTheme::SetScrollbarColors(unsigned inactive_color,
                                     unsigned active_color,
                                     unsigned track_color) {
  thumb_inactive_color_ = inactive_color;
  thumb_active_color_ = active_color;
  track_color_ = track_color;
}

void NativeTheme::AddObserver(NativeThemeObserver* observer) {
  native_theme_observers_.AddObserver(observer);
}

void NativeTheme::RemoveObserver(NativeThemeObserver* observer) {
  native_theme_observers_.RemoveObserver(observer);
}

void NativeTheme::NotifyObservers() {
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnNativeThemeUpdated(this);
}

NativeTheme::NativeTheme()
    : thumb_inactive_color_(0xeaeaea),
      thumb_active_color_(0xf4f4f4),
      track_color_(0xd3d3d3),
      is_dark_mode_(IsForcedDarkMode()),
      is_high_contrast_(IsForcedHighContrast()) {}

NativeTheme::~NativeTheme() {}

bool NativeTheme::SystemDarkModeEnabled() const {
  return is_dark_mode_;
}

bool NativeTheme::UsesHighContrastColors() const {
  return is_high_contrast_;
}

bool NativeTheme::IsForcedDarkMode() const {
  static bool kIsForcedDarkMode =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode);
  return kIsForcedDarkMode;
}

bool NativeTheme::IsForcedHighContrast() const {
  static bool kIsForcedHighContrast =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast);
  return kIsForcedHighContrast;
}

CaptionStyle NativeTheme::GetSystemCaptionStyle() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceCaptionStyle)) {
    return CaptionStyle::FromSpec(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceCaptionStyle));
  }

  return CaptionStyle::FromSystemSettings();
}

}  // namespace ui
