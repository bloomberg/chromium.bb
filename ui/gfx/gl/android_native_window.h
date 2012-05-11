// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_ANDROID_NATIVE_WINDOW_H_
#define UI_GFX_GL_ANDROID_NATIVE_WINDOW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"

struct ANativeWindow;

namespace gfx {

// This class deals with the Android native window ref count.
class AndroidNativeWindow {
 public:
  explicit AndroidNativeWindow(ANativeWindow* window);
  ~AndroidNativeWindow();

  ANativeWindow* GetNativeHandle() const;

 private:
  ANativeWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(AndroidNativeWindow);
};

}  // namespace gfx

#endif  // UI_GFX_GL_ANDROID_NATIVE_WINDOW_H_
