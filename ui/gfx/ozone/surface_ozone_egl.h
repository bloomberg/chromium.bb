// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_SURFACE_OZONE_EGL_H_
#define UI_GFX_OZONE_SURFACE_OZONE_EGL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Size;
class VSyncProvider;

// The platform-specific part of an EGL surface.
//
// This class owns any bits that the ozone implementation needs freed when
// the EGL surface is destroyed.
class GFX_EXPORT SurfaceOzoneEGL {
 public:
  virtual ~SurfaceOzoneEGL() {}

  // Returns the EGL native window for rendering onto this surface.
  // This can be used to to create a GLSurface.
  virtual intptr_t /* EGLNativeWindowType */ GetNativeWindow() = 0;

  // Attempts to resize the EGL native window to match the viewport
  // size.
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) = 0;

  // Returns a gfx::VsyncProvider for this surface. Note that this may be
  // called after we have entered the sandbox so if there are operations (e.g.
  // opening a file descriptor providing vsync events) that must be done
  // outside of the sandbox, they must have been completed in
  // InitializeHardware. Returns an empty scoped_ptr on error.
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() = 0;
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_SURFACE_OZONE_EGL_H_
