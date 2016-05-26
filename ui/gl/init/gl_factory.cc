// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gl {
namespace init {

// TODO(kylechar): This file should be replaced with a platform specific
// version for X11, Ozone, Windows, Mac and Android. The implementation of each
// factory function should be moved into that file and the original static
// methods should be removed from GLSurface and GLContext. This file can then
// be deleted.

bool InitializeGLOneOff() {
  return GLSurface::InitializeOneOff();
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         GpuPreference gpu_preference) {
  return GLContext::CreateGLContext(share_group, compatible_surface,
                                    gpu_preference);
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  return GLSurface::CreateViewGLSurface(window);
}

#if defined(USE_OZONE)
scoped_refptr<GLSurface> CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  return GLSurface::CreateSurfacelessViewGLSurface(window);
}
#endif  // defined(USE_OZONE)

scoped_refptr<GLSurface> CreateOffscreenGLSurface(const gfx::Size& size) {
  return GLSurface::CreateOffscreenGLSurface(size);
}

}  // namespace init
}  // namespace gl
