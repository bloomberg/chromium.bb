// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_NS_GL_SURFACE_H_
#define UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_NS_GL_SURFACE_H_

#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/accelerated_widget_mac/io_surface_texture.h"

namespace ui {

class IOSurfaceNSGLSurface {
 public:
  static IOSurfaceNSGLSurface* Create(NSView* view);
  ~IOSurfaceNSGLSurface();

  // Called on the UI thread.
  bool GotFrame(IOSurfaceID io_surface_id,
                gfx::Size pixel_size,
                float scale_factor,
                gfx::Rect pixel_damage_rect);

 private:
  explicit IOSurfaceNSGLSurface(
      NSView* view,
      base::scoped_nsobject<NSOpenGLContext> ns_gl_context,
      scoped_refptr<ui::IOSurfaceTexture> iosurface);
  NSView* view_;
  scoped_refptr<ui::IOSurfaceTexture> iosurface_;
  base::scoped_nsobject<NSOpenGLContext> ns_gl_context_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_NS_GL_SURFACE_H_
