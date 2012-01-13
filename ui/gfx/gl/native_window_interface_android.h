// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_NATIVE_WINDOW_INTERFACE_ANDROID_H_
#define UI_GFX_GL_NATIVE_WINDOW_INTERFACE_ANDROID_H_
#pragma once

struct ANativeWindow;

namespace gfx {

// This is a wrapper object to pass ANativeWindow handles around when running
// in SurfaceTexture more.
class NativeWindowInterface {
 public:
  virtual ~NativeWindowInterface() {}

  virtual ANativeWindow* GetNativeHandle() const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GL_NATIVE_WINDOW_INTERFACE_ANDROID_H_
