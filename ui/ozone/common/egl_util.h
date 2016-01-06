// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_EGL_UTIL_H_
#define UI_OZONE_COMMON_EGL_UTIL_H_

#include "ui/ozone/ozone_base_export.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

OZONE_BASE_EXPORT
bool LoadDefaultEGLGLES2Bindings(
    SurfaceFactoryOzone::AddGLLibraryCallback add_gl_library,
    SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
        set_gl_get_proc_address);

OZONE_BASE_EXPORT
bool LoadEGLGLES2Bindings(
    SurfaceFactoryOzone::AddGLLibraryCallback add_gl_library,
    SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
        set_gl_get_proc_address,
    const char* egl_library_name,
    const char* gles_library_name);

}  // namespace ui

#endif  // UI_OZONE_COMMON_EGL_UTIL_H_
