// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/io_surface_texture.h"

#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_context.h"

namespace ui {

// static
scoped_refptr<IOSurfaceTexture> IOSurfaceTexture::Create(
    bool needs_gl_finish_workaround,
    bool use_ns_apis) {
  scoped_refptr<IOSurfaceContext> offscreen_context;
  if (!use_ns_apis) {
    offscreen_context = IOSurfaceContext::Get(
        IOSurfaceContext::kOffscreenContext);
    if (!offscreen_context.get()) {
      LOG(ERROR) << "Failed to create context for offscreen operations";
      return NULL;
    }
  }
  return new IOSurfaceTexture(
      offscreen_context, use_ns_apis, needs_gl_finish_workaround);
}

IOSurfaceTexture::IOSurfaceTexture(
    const scoped_refptr<IOSurfaceContext>& offscreen_context,
    bool use_ns_apis,
    bool needs_gl_finish_workaround)
    : offscreen_context_(offscreen_context),
      texture_(0),
      gl_error_(GL_NO_ERROR),
      eviction_queue_iterator_(eviction_queue_.Get().end()),
      eviction_has_been_drawn_since_updated_(false),
      needs_gl_finish_workaround_(needs_gl_finish_workaround),
      using_ns_apis_(use_ns_apis) {
}

IOSurfaceTexture::~IOSurfaceTexture() {
  ReleaseIOSurfaceAndTexture();
  offscreen_context_ = NULL;
  DCHECK(eviction_queue_iterator_ == eviction_queue_.Get().end());
}

bool IOSurfaceTexture::DrawIOSurface() {
  return DrawIOSurfaceWithDamageRect(gfx::Rect(pixel_size_));
}

bool IOSurfaceTexture::DrawIOSurfaceWithDamageRect(gfx::Rect damage_rect) {
  TRACE_EVENT0("browser", "IOSurfaceTexture::DrawIOSurfaceWithDamageRect");
  DCHECK(CGLGetCurrentContext());

  // If we have release the IOSurface, clear the screen to light grey and
  // early-out.
  if (!io_surface_) {
    glClearColor(0.9, 0.9, 0.9, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return false;
  }

  // The viewport is the size of the CALayer, which should always match the
  // IOSurface pixel size.
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  gfx::Rect viewport_rect(viewport[0], viewport[1], viewport[2], viewport[3]);

  // Set the projection matrix to match 1 unit to 1 pixel.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, viewport_rect.width(),
          pixel_size_.height() - viewport_rect.height(), pixel_size_.height(),
          -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Draw a quad the size of the IOSurface. This should cover the full viewport.
  glColor4f(1, 1, 1, 1);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);
  glBegin(GL_QUADS);
  glTexCoord2f(damage_rect.x(), damage_rect.y());
  glVertex2f(damage_rect.x(), damage_rect.y());
  glTexCoord2f(damage_rect.right(), damage_rect.y());
  glVertex2f(damage_rect.right(), damage_rect.y());
  glTexCoord2f(damage_rect.right(), damage_rect.bottom());
  glVertex2f(damage_rect.right(), damage_rect.bottom());
  glTexCoord2f(damage_rect.x(), damage_rect.bottom());
  glVertex2f(damage_rect.x(), damage_rect.bottom());
  glEnd();
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  // Workaround for issue 158469. Issue a dummy draw call with texture_ not
  // bound to a texture, in order to shake all references to the IOSurface out
  // of the driver.
  glBegin(GL_TRIANGLES);
  glEnd();

  if (needs_gl_finish_workaround_) {
    TRACE_EVENT0("gpu", "glFinish");
    glFinish();
  }

  // Check if any of the drawing calls result in an error.
  GetAndSaveGLError();
  bool result = true;
  if (gl_error_ != GL_NO_ERROR) {
    LOG(ERROR) << "GL error in DrawIOSurface: " << gl_error_;
    result = false;
    // If there was an error, clear the screen to a light grey to avoid
    // rendering artifacts.
    glClearColor(0.8, 0.8, 0.8, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  eviction_has_been_drawn_since_updated_ = true;
  return result;
}

bool IOSurfaceTexture::IsUpToDate(
    IOSurfaceID io_surface_id, const gfx::Size& pixel_size) const {
  return io_surface_ &&
         io_surface_id == IOSurfaceGetID(io_surface_) &&
         pixel_size == pixel_size_;
}

bool IOSurfaceTexture::SetIOSurface(
    IOSurfaceID io_surface_id, const gfx::Size& pixel_size) {
  TRACE_EVENT0("browser", "IOSurfaceTexture::MapIOSurfaceToTexture");

  // Destroy the old IOSurface and texture if it is no longer needed.
  bool needs_new_iosurface =
      !io_surface_ || io_surface_id != IOSurfaceGetID(io_surface_);
  if (needs_new_iosurface)
    ReleaseIOSurfaceAndTexture();

  // Note that because IOSurface sizes are rounded, the same IOSurface may have
  // two different sizes associated with it, so update the sizes before the
  // early-out.
  pixel_size_ = pixel_size;

  // Early-out if the IOSurface has not changed.
  if (!needs_new_iosurface)
    return true;

  // If we early-out at any point from now on, it's because of an error, and we
  // should destroy the texture and release the IOSurface.
  base::ScopedClosureRunner error_runner(base::BindBlock(^{
      ReleaseIOSurfaceAndTexture();
  }));

  // Open the IOSurface handle.
  io_surface_.reset(IOSurfaceLookup(io_surface_id));
  if (!io_surface_)
    return false;

  // Actual IOSurface size is rounded up to reduce reallocations during window
  // resize. Get the actual size to properly map the texture.
  gfx::Size rounded_size(IOSurfaceGetWidth(io_surface_),
                         IOSurfaceGetHeight(io_surface_));

  // Create the GL texture and set it to be backed by the IOSurface.
  CGLError cgl_error = kCGLNoError;
  {
    scoped_ptr<gfx::ScopedCGLSetCurrentContext> scoped_set_current_context;
    if (offscreen_context_) {
      scoped_set_current_context.reset(new gfx::ScopedCGLSetCurrentContext(
          offscreen_context_->cgl_context()));
    } else {
      DCHECK(CGLGetCurrentContext());
    }

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);
    glTexParameterf(
        GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(
        GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    cgl_error = CGLTexImageIOSurface2D(
        CGLGetCurrentContext(),
        GL_TEXTURE_RECTANGLE_ARB,
        GL_RGBA,
        rounded_size.width(),
        rounded_size.height(),
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        io_surface_.get(),
        0 /* plane */);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    GetAndSaveGLError();
  }

  // Return failure if an error was encountered by CGL or GL.
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "CGLTexImageIOSurface2D failed with CGL error: " << cgl_error;
    return false;
  }
  if (gl_error_ != GL_NO_ERROR) {
    LOG(ERROR) << "Hit GL error in SetIOSurface: " << gl_error_;
    return false;
  }

  ignore_result(error_runner.Release());
  return true;
}

void IOSurfaceTexture::ReleaseIOSurfaceAndTexture() {
  scoped_ptr<gfx::ScopedCGLSetCurrentContext> scoped_set_current_context;
  if (offscreen_context_) {
    scoped_set_current_context.reset(new gfx::ScopedCGLSetCurrentContext(
        offscreen_context_->cgl_context()));
  } else {
    DCHECK(CGLGetCurrentContext());
  }

  if (texture_) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  pixel_size_ = gfx::Size();
  io_surface_.reset();

  EvictionMarkEvicted();
}

bool IOSurfaceTexture::HasBeenPoisoned() const {
  if (offscreen_context_)
    return offscreen_context_->HasBeenPoisoned();
  return false;
}

GLenum IOSurfaceTexture::GetAndSaveGLError() {
  GLenum gl_error = glGetError();
  if (gl_error_ == GL_NO_ERROR)
    gl_error_ = gl_error;
  return gl_error;
}

void IOSurfaceTexture::EvictionMarkUpdated() {
  EvictionMarkEvicted();
  eviction_queue_.Get().push_back(this);
  eviction_queue_iterator_ = --eviction_queue_.Get().end();
  eviction_has_been_drawn_since_updated_ = false;
  EvictionScheduleDoEvict();
}

void IOSurfaceTexture::EvictionMarkEvicted() {
  if (eviction_queue_iterator_ == eviction_queue_.Get().end())
    return;
  eviction_queue_.Get().erase(eviction_queue_iterator_);
  eviction_queue_iterator_ = eviction_queue_.Get().end();
  eviction_has_been_drawn_since_updated_ = false;
}

// static
void IOSurfaceTexture::EvictionScheduleDoEvict() {
  if (eviction_scheduled_)
    return;
  if (eviction_queue_.Get().size() <= kMaximumUnevictedSurfaces)
    return;

  eviction_scheduled_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&IOSurfaceTexture::EvictionDoEvict));
}

// static
void IOSurfaceTexture::EvictionDoEvict() {
  eviction_scheduled_ = false;
  // Walk the list of allocated surfaces from least recently used to most
  // recently used.
  for (EvictionQueue::iterator it = eviction_queue_.Get().begin();
       it != eviction_queue_.Get().end();) {
    IOSurfaceTexture* surface = *it;
    ++it;

    // If the number of IOSurfaces allocated is less than the threshold,
    // stop walking the list of surfaces.
    if (eviction_queue_.Get().size() <= kMaximumUnevictedSurfaces)
      break;

    // Don't evict anything that has not yet been drawn.
    if (!surface->eviction_has_been_drawn_since_updated_)
      continue;

    // Don't evict anything that doesn't have an offscreen context (as we have
    // context in which to delete the texture).
    if (!surface->offscreen_context_)
      continue;

    // Evict the surface.
    surface->ReleaseIOSurfaceAndTexture();
  }
}

// static
base::LazyInstance<IOSurfaceTexture::EvictionQueue>
    IOSurfaceTexture::eviction_queue_;

// static
bool IOSurfaceTexture::eviction_scheduled_ = false;

}  // namespace ui
