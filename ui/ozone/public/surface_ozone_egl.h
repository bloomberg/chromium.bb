// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_OZONE_EGL_H_
#define UI_OZONE_PUBLIC_SURFACE_OZONE_EGL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/ozone_base_export.h"

namespace gfx {
class Size;
class VSyncProvider;
}

namespace ui {
class NativePixmap;

// The platform-specific part of an EGL surface.
//
// This class owns any bits that the ozone implementation needs freed when
// the EGL surface is destroyed.
class OZONE_BASE_EXPORT SurfaceOzoneEGL {
 public:
  virtual ~SurfaceOzoneEGL() {}

  // Returns the EGL native window for rendering onto this surface.
  // This can be used to to create a GLSurface.
  virtual intptr_t /* EGLNativeWindowType */ GetNativeWindow() = 0;

  // Attempts to resize the EGL native window to match the viewport
  // size.
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) = 0;

  // Called after we swap buffers. This is usually a no-op but can
  // be used to present the new front buffer if the platform requires this.
  virtual bool OnSwapBuffers() = 0;

  // Returns a gfx::VsyncProvider for this surface. Note that this may be
  // called after we have entered the sandbox so if there are operations (e.g.
  // opening a file descriptor providing vsync events) that must be done
  // outside of the sandbox, they must have been completed in
  // InitializeHardware. Returns an empty scoped_ptr on error.
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() = 0;

  // Sets the overlay plane to switch to at the next page flip.
  // |plane_z_order| specifies the stacking order of the plane relative to the
  // main framebuffer located at index 0.
  // |plane_transform| specifies how the buffer is to be transformed during.
  // composition.
  // |buffer| to be presented by the overlay.
  // |display_bounds| specify where it is supposed to be on the screen.
  // |crop_rect| specifies the region within the buffer to be placed
  // inside |display_bounds|. This is specified in texture coordinates, in the
  // range of [0,1].
  virtual bool ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    scoped_refptr<NativePixmap> buffer,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& crop_rect) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_OZONE_EGL_H_
