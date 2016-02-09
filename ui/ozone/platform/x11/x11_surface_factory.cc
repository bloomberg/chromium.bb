// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_surface_factory.h"

#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/ozone/common/egl_util.h"

namespace ui {

X11SurfaceFactory::X11SurfaceFactory() {}

X11SurfaceFactory::~X11SurfaceFactory() {}

bool X11SurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

intptr_t X11SurfaceFactory::GetNativeDisplay() {
  return reinterpret_cast<intptr_t>(gfx::GetXDisplay());
}

}  // namespace ui
