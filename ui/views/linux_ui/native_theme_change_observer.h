// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_NATIVE_THEME_CHANGE_OBSERVER_H_
#define UI_VIEWS_LINUX_UI_NATIVE_THEME_CHANGE_OBSERVER_H_

#include "ui/views/views_export.h"

namespace views {

// Observers which are notified when the native theme changes.
class VIEWS_EXPORT NativeThemeChangeObserver {
 public:
  virtual ~NativeThemeChangeObserver() {}

  // Called whenever the underlying platform changes its native theme.
  virtual void OnNativeThemeChanged() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_LINUX_UI_NATIVE_THEME_CHANGE_OBSERVER_H_
