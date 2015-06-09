// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_NS_GL_SURFACE_H_
#define UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_NS_GL_SURFACE_H_

#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>

#include "base/mac/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "ui/accelerated_widget_mac/display_link_mac.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/accelerated_widget_mac/io_surface_texture.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class IOSurfaceNSGLSurfaceClient {
 public:
  virtual void IOSurfaceNSGLSurfaceDidDrawFrame() = 0;
};

class IOSurfaceNSGLSurface {
 public:
  static IOSurfaceNSGLSurface* Create(
      IOSurfaceNSGLSurfaceClient* client, NSView* view);
  virtual ~IOSurfaceNSGLSurface();

  // Called on the UI thread.
  bool GotFrame(IOSurfaceID io_surface_id,
                gfx::Size pixel_size,
                float scale_factor,
                gfx::Rect pixel_damage_rect);

 private:
  explicit IOSurfaceNSGLSurface(
      IOSurfaceNSGLSurfaceClient* client,
      NSView* view,
      base::scoped_nsobject<NSOpenGLContext> ns_gl_context,
      scoped_refptr<ui::IOSurfaceTexture> iosurface);

  void DoPendingDrawIfNeeded();

  IOSurfaceNSGLSurfaceClient* client_;
  NSView* view_;
  scoped_refptr<IOSurfaceTexture> iosurface_;
  base::scoped_nsobject<NSOpenGLContext> ns_gl_context_;

  // The size and scale factor of the backbuffer.
  gfx::Size contents_pixel_size_;
  float contents_scale_factor_;

  // The state of a draw that has been sent to the surface, but has not been
  // flushed yet.
  bool pending_draw_exists_;
  gfx::Rect pending_draw_damage_rect_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_NS_GL_SURFACE_H_
