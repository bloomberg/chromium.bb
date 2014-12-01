// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_TEXTURE_H_
#define UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_TEXTURE_H_

#include <deque>
#include <list>
#include <vector>

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>
#include <QuartzCore/QuartzCore.h>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

class SkBitmap;

namespace gfx {
class Rect;
}

namespace ui {

class IOSurfaceContext;
class RenderWidgetHostViewFrameSubscriber;
class RenderWidgetHostViewMac;

// This class manages an OpenGL context and IOSurfaceTexture for the accelerated
// compositing code path. The GL context is attached to
// RenderWidgetHostViewCocoa for blitting the IOSurfaceTexture.
class IOSurfaceTexture
    : public base::RefCounted<IOSurfaceTexture> {
 public:
  // Returns NULL if IOSurfaceTexture or GL API calls fail.
  static scoped_refptr<IOSurfaceTexture> Create(
      bool needs_gl_finish_workaround);

  // Set IOSurfaceTexture that will be drawn on the next NSView drawRect.
  bool SetIOSurface(
      IOSurfaceID io_surface_id,
      const gfx::Size& pixel_size) WARN_UNUSED_RESULT;

  // Blit the IOSurface to the rectangle specified by |window_rect| in DIPs,
  // with the origin in the lower left corner. If the window rect's size is
  // larger than the IOSurface, the remaining right and bottom edges will be
  // white. |window_scale_factor| is 1 in normal views, 2 in HiDPI views.
  bool DrawIOSurface() WARN_UNUSED_RESULT;

  // Returns true if the offscreen context used by this surface has been
  // poisoned.
  bool HasBeenPoisoned() const;

 private:
  friend class base::RefCounted<IOSurfaceTexture>;

  IOSurfaceTexture(
      const scoped_refptr<IOSurfaceContext>& context,
      bool needs_gl_finish_workaround);
  ~IOSurfaceTexture();

  // Unref the IOSurfaceTexture and delete the associated GL texture. If the GPU
  // process is no longer referencing it, this will delete the IOSurface.
  void ReleaseIOSurfaceAndTexture();

  // Check for GL errors and store the result in error_. Only return new
  // errors
  GLenum GetAndSaveGLError();

  // Offscreen context used for all operations other than drawing to the
  // screen. This is in the same share group as the contexts used for
  // drawing, and is the same for all IOSurfaces in all windows.
  scoped_refptr<IOSurfaceContext> offscreen_context_;

  // The IOSurface and its non-rounded size.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  gfx::Size pixel_size_;

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  // Error saved by GetAndSaveGLError
  GLint gl_error_;

  // Aggressive IOSurface eviction logic. When using CoreAnimation, IOSurfaces
  // are used only transiently to transfer from the GPU process to the browser
  // process. Once the IOSurface has been drawn to its CALayer, the CALayer
  // will not need updating again until its view is hidden and re-shown.
  // Aggressively evict surfaces when more than 8 (the number allowed by the
  // memory manager for fast tab switching) are allocated.
  enum {
    kMaximumUnevictedSurfaces = 8,
  };
  typedef std::list<IOSurfaceTexture*> EvictionQueue;
  void EvictionMarkUpdated();
  void EvictionMarkEvicted();
  EvictionQueue::iterator eviction_queue_iterator_;
  bool eviction_has_been_drawn_since_updated_;
  const bool needs_gl_finish_workaround_;

  static void EvictionScheduleDoEvict();
  static void EvictionDoEvict();
  static base::LazyInstance<EvictionQueue> eviction_queue_;
  static bool eviction_scheduled_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_TEXTURE_H_
