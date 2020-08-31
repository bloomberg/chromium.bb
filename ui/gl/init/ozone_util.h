// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_OZONE_UTIL_H_
#define UI_GL_INIT_OZONE_UTIL_H_

#include "base/macros.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace init {

inline ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() {
  return ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
}

// Returns true if there is an GLOzone for the specified GL implementation.
inline bool HasGLOzone(GLImplementation impl) {
  return GetSurfaceFactoryOzone() && GetSurfaceFactoryOzone()->GetGLOzone(impl);
}

// Returns true if there is an GLOzone for the set GL implementation.
inline bool HasGLOzone() {
  return HasGLOzone(GetGLImplementation());
}

// Returns the GLOzone for the specified GL implementation or null if none
// exists.
inline ui::GLOzone* GetGLOzone(GLImplementation impl) {
  return GetSurfaceFactoryOzone()->GetGLOzone(impl);
}

// Returns the GLOzone for the set GL implementation or null if none exists.
inline ui::GLOzone* GetGLOzone() {
  return GetGLOzone(GetGLImplementation());
}

}  // namespace init
}  // namespace gl

#endif  // UI_GL_INIT_OZONE_UTIL_H_
