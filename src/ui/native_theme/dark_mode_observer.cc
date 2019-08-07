// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/dark_mode_observer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ui {

DarkModeObserver::DarkModeObserver(
    NativeTheme* theme,
    const base::RepeatingCallback<void(bool)>& callback)
    : theme_(theme),
      callback_(callback),
      in_dark_mode_(theme_->SystemDarkModeEnabled()),
      theme_observer_(this) {}

DarkModeObserver::~DarkModeObserver() = default;

void DarkModeObserver::Start() {
  RunCallbackIfChanged();
  theme_observer_.Add(theme_);
}

void DarkModeObserver::Stop() {
  theme_observer_.RemoveAll();
}

void DarkModeObserver::OnNativeThemeUpdated(NativeTheme* observed_theme) {
  DCHECK_EQ(observed_theme, theme_);
  RunCallbackIfChanged();
}

void DarkModeObserver::RunCallbackIfChanged() {
  bool in_dark_mode = theme_->SystemDarkModeEnabled();
  if (in_dark_mode_ == in_dark_mode)
    return;
  in_dark_mode_ = in_dark_mode;
  callback_.Run(in_dark_mode_);
}

}  // namespace ui
