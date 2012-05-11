// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/android_native_window.h"

#include <android/native_window.h>

namespace gfx {

AndroidNativeWindow::AndroidNativeWindow(ANativeWindow* window)
    : window_(window) {
  if (window_)
    ANativeWindow_acquire(window_);
}

AndroidNativeWindow::~AndroidNativeWindow() {
  if (window_)
    ANativeWindow_release(window_);
}

ANativeWindow* AndroidNativeWindow::GetNativeHandle() const {
  return window_;
}

}  // namespace gfx
