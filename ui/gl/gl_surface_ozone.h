// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_OZONE_H_
#define UI_GL_GL_SURFACE_OZONE_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

namespace gl {

GL_EXPORT scoped_refptr<GLSurface> CreateViewGLSurfaceOzone(
    gfx::AcceleratedWidget window);

GL_EXPORT scoped_refptr<GLSurface> CreateViewGLSurfaceOzoneSurfaceless(
    gfx::AcceleratedWidget window);

GL_EXPORT scoped_refptr<GLSurface>
CreateViewGLSurfaceOzoneSurfacelessSurfaceImpl(gfx::AcceleratedWidget window);

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_OZONE_H_
