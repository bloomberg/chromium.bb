// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/io_surface_ns_gl_surface.h"

#include "base/callback_helpers.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gpu_switching_manager.h"

namespace ui {

IOSurfaceNSGLSurface* IOSurfaceNSGLSurface::Create(NSView* view) {
  scoped_refptr<IOSurfaceTexture> iosurface = IOSurfaceTexture::Create(false);
  if (!iosurface)
    return NULL;

  std::vector<NSOpenGLPixelFormatAttribute> attribs;
  attribs.push_back(NSOpenGLPFAColorSize);
  attribs.push_back(24);
  attribs.push_back(NSOpenGLPFAAlphaSize);
  attribs.push_back(8);
  attribs.push_back(NSOpenGLPFAAccelerated);
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus())
    attribs.push_back(NSOpenGLPFAAllowOfflineRenderers);
  attribs.push_back(0);
  base::scoped_nsobject<NSOpenGLPixelFormat> pixel_format(
      [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs.front()]);
  if (!pixel_format) {
    LOG(ERROR) << "Failed to create pixel format object.";
    return NULL;
  }

  base::scoped_nsobject<NSOpenGLContext> ns_gl_context(
      [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:nil]);
  if (!ns_gl_context) {
    LOG(ERROR) << "Failed to create context object.";
    return NULL;
  }

  return new IOSurfaceNSGLSurface(view, ns_gl_context, iosurface);
}

IOSurfaceNSGLSurface::IOSurfaceNSGLSurface(
    NSView* view,
    base::scoped_nsobject<NSOpenGLContext> ns_gl_context,
    scoped_refptr<ui::IOSurfaceTexture> iosurface)
    : view_(view), iosurface_(iosurface), ns_gl_context_(ns_gl_context) {
  [[view_ layer] setContentsGravity:kCAGravityTopLeft];
  [ns_gl_context_ setView:view_];
}

IOSurfaceNSGLSurface::~IOSurfaceNSGLSurface() {
  [ns_gl_context_ makeCurrentContext];
  iosurface_ = NULL;
  [NSOpenGLContext clearCurrentContext];
  [ns_gl_context_ clearDrawable];
}

bool IOSurfaceNSGLSurface::GotFrame(IOSurfaceID io_surface_id,
                                    gfx::Size frame_pixel_size,
                                    float frame_scale_factor) {
  // The OpenGL framebuffer's scale factor and pixel size are updated to match
  // the CALayer's contentsScale and bounds at setView. The pixel size is the
  // stored in the GL_VIEWPORT state of the context.
  gfx::Size contents_pixel_size;
  float contents_scale_factor = [[view_ layer] contentsScale];
  {
    [ns_gl_context_ makeCurrentContext];
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    [NSOpenGLContext clearCurrentContext];
    contents_pixel_size = gfx::Size(viewport[2], viewport[3]);
  }

  // If the OpenGL framebuffer does not match the frame in scale factor or
  // pixel size, then re-latch them. Note that they will latch to the layer's
  // bounds, which will not necessarily match the frame's pixel size.
  if (frame_pixel_size != contents_pixel_size ||
      frame_scale_factor != contents_scale_factor) {
    ScopedCAActionDisabler disabler;
    [ns_gl_context_ clearDrawable];
    [[view_ layer] setContentsScale:frame_scale_factor];
    [ns_gl_context_ setView:view_];
  }

  bool result = true;
  [ns_gl_context_ makeCurrentContext];
  result &= iosurface_->SetIOSurface(io_surface_id, frame_pixel_size);
  result &= iosurface_->DrawIOSurface();
  glFlush();
  [NSOpenGLContext clearCurrentContext];
  return result;
}

};
