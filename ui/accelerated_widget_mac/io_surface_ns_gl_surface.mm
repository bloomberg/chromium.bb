// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/io_surface_ns_gl_surface.h"

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/GL.h>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gpu_switching_manager.h"

namespace ui {

IOSurfaceNSGLSurface* IOSurfaceNSGLSurface::Create(
    IOSurfaceNSGLSurfaceClient* client,
    NSView* view,
    bool needs_gl_finish_workaround) {
  scoped_refptr<IOSurfaceTexture> iosurface =
      IOSurfaceTexture::Create(needs_gl_finish_workaround, true);
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

  return new IOSurfaceNSGLSurface(
      client, view, needs_gl_finish_workaround, ns_gl_context, iosurface);
}

IOSurfaceNSGLSurface::IOSurfaceNSGLSurface(
    IOSurfaceNSGLSurfaceClient* client,
    NSView* view,
    bool needs_gl_finish_workaround,
    base::scoped_nsobject<NSOpenGLContext> ns_gl_context,
    scoped_refptr<ui::IOSurfaceTexture> iosurface)
    : client_(client), view_(view), iosurface_(iosurface),
      ns_gl_context_(ns_gl_context), contents_scale_factor_(1),
      pending_draw_exists_(false), needs_to_be_recreated_(false) {
  [[view_ layer] setContentsGravity:kCAGravityTopLeft];
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

IOSurfaceNSGLSurface::~IOSurfaceNSGLSurface() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  DoPendingDrawIfNeeded();
  [ns_gl_context_ makeCurrentContext];
  iosurface_ = NULL;
  [NSOpenGLContext clearCurrentContext];
  [ns_gl_context_ clearDrawable];

  ScopedCAActionDisabler disabler;
  [[view_ layer] setContents:nil];
}

bool IOSurfaceNSGLSurface::GotFrame(IOSurfaceID io_surface_id,
                                    gfx::Size frame_pixel_size,
                                    float frame_scale_factor,
                                    gfx::Rect pixel_damage_rect) {
  TRACE_EVENT0("ui", "IOSurfaceNSGLSurface::GotFrame");
  pending_draw_exists_ = true;
  pending_draw_damage_rect_.Union(pixel_damage_rect);

  // If the OpenGL framebuffer does not match the frame in scale factor or
  // pixel size, then re-latch them. Note that they will latch to the layer's
  // bounds, which will not necessarily match the frame's pixel size.
  if (!iosurface_->IsUpToDate(io_surface_id, frame_pixel_size) ||
      [ns_gl_context_ view] != view_ ||
      frame_pixel_size != contents_pixel_size_ ||
      frame_scale_factor != contents_scale_factor_) {
    ScopedCAActionDisabler disabler;
    [ns_gl_context_ clearDrawable];
    [[view_ layer] setContentsScale:frame_scale_factor];

    // The OpenGL framebuffer's scale factor and pixel size are updated to match
    // the CALayer's contentsScale and bounds at setView. The pixel size is the
    // stored in the GL_VIEWPORT state of the context.
    [ns_gl_context_ setView:view_];
    [ns_gl_context_ makeCurrentContext];

    // Use a clear instead of explicitly drawing background-color quads, since
    // quads do not always cover the screen.
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Retrieve the backbuffer's size from the GL viewport state.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    contents_pixel_size_ = gfx::Size(viewport[2], viewport[3]);
    contents_scale_factor_ = frame_scale_factor;

    bool result = iosurface_->SetIOSurface(io_surface_id, frame_pixel_size);
    [NSOpenGLContext clearCurrentContext];
    if (!result) {
      LOG(ERROR) << "Failed to set IOSurface, poisoning NSOpenGLContext.";
      needs_to_be_recreated_ = true;
      return false;
    }

    pending_draw_damage_rect_ = gfx::Rect(frame_pixel_size);
  }

  DoPendingDrawIfNeeded();
  return true;
}

void IOSurfaceNSGLSurface::DoPendingDrawIfNeeded() {
  if (!pending_draw_exists_)
    return;

  {
    TRACE_EVENT0("ui", "IOSurfaceNSGLSurface::Draw");
    [ns_gl_context_ makeCurrentContext];
    bool result = iosurface_->DrawIOSurfaceWithDamageRect(
        pending_draw_damage_rect_);
    if (!result) {
      LOG(ERROR) << "Failed to draw IOSurface, poisoning NSOpenGLContext.";
      needs_to_be_recreated_ = true;
    }
    glFlush();
    [NSOpenGLContext clearCurrentContext];
  }

  pending_draw_exists_ = false;
  pending_draw_damage_rect_ = gfx::Rect();

  client_->IOSurfaceNSGLSurfaceDidDrawFrame();
}

int IOSurfaceNSGLSurface::GetRendererID() {
  GLint current_renderer_id = -1;
  CGLContextObj cgl_context = static_cast<CGLContextObj>(
      [ns_gl_context_ CGLContextObj]);
  if (CGLGetParameter(cgl_context,
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) == kCGLNoError) {
    return current_renderer_id & kCGLRendererIDMatchingMask;
  }
  return -1;
}

void IOSurfaceNSGLSurface::OnGpuSwitched() {
  needs_to_be_recreated_ = true;
}

// static
bool IOSurfaceNSGLSurface::CanUseNSGLSurfaceForView(NSView* view) {
  // This must be explicitly enabled at the command line.
  static bool use_ns_gl_surfaces =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNSGLSurfaces);
  if (!use_ns_gl_surfaces)
    return false;

  // Do not attempt this before 10.9. The power savings are not worth the
  // stability risk and testing burden.
  if (!base::mac::IsOSMavericksOrLater())
    return false;

  // If the NSView being attached to the NSOpenGLContext is not on the main
  // monitor, then, due to an OS X bug, the IOSurface will be thrashed,
  // resulting in hangs of 50 msec or more.
  CGDirectDisplayID main_display = CGMainDisplayID();
  NSScreen* screen = [[view window] screen];
  NSDictionary* screen_description = [screen deviceDescription];
  NSNumber* screen_number = [screen_description objectForKey:@"NSScreenNumber"];
  CGDirectDisplayID display_id = [screen_number unsignedIntValue];
  if (display_id != main_display)
    return false;

  return true;
}

}  // namespace ui
