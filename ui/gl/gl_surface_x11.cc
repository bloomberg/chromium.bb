// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return gfx::GetXDisplay();
}

}  // namespace gl
