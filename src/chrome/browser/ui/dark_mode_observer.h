// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DARK_MODE_OBSERVER_H_
#define CHROME_BROWSER_UI_DARK_MODE_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/native_theme/native_theme_observer.h"

class DarkModeObserver : public ui::NativeThemeObserver {
 public:
  // Observe |theme| for changes and run |callback| if detected.
  DarkModeObserver(ui::NativeTheme* theme,
                   const base::RepeatingCallback<void(bool)>& callback);

  ~DarkModeObserver() override;

  // Observe |theme| for changes in dark mode. |callback_| may be immediately
  // run if |theme_->SystemDarkModeEnabled()| has changed since construction or
  // last time |Stop()| was called.
  void Start();

  // Stop observing |theme|. |callback_| will never be called after stopping.
  void Stop();

  bool InDarkMode() const { return in_dark_mode_; }

 private:
  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Run |callback_| if |theme_->SystemDarkModeEnabled()| != |in_dark_mode_|.
  void RunCallbackIfChanged();

  // The theme to query/watch for changes.
  ui::NativeTheme* const theme_;

  base::RepeatingCallback<void(bool)> callback_;

  bool in_dark_mode_;

  ScopedObserver<ui::NativeTheme, DarkModeObserver> theme_observer_;

  DISALLOW_COPY_AND_ASSIGN(DarkModeObserver);
};

#endif  // CHROME_BROWSER_UI_DARK_MODE_OBSERVER_H_
