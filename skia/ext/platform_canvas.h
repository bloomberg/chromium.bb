// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_CANVAS_H_
#define SKIA_EXT_PLATFORM_CANVAS_H_
#pragma once

// The platform-specific device will include the necessary platform headers
// to get the surface type.
#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace skia {

// This class is a specialization of the regular SkCanvas that is designed to
// work with a PlatformDevice to manage platform-specific drawing. It allows
// using both Skia operations and platform-specific operations.
class SK_API PlatformCanvas : public SkCanvas {
 public:
  // If you use the version with no arguments, you MUST call initialize()
  PlatformCanvas();
  // Set is_opaque if you are going to erase the bitmap and not use
  // transparency: this will enable some optimizations.
  PlatformCanvas(int width, int height, bool is_opaque);

#if defined(WIN32)
  // The shared_section parameter is passed to gfx::PlatformDevice::create.
  // See it for details.
  PlatformCanvas(int width, int height, bool is_opaque, HANDLE shared_section);
#elif defined(__APPLE__)
  PlatformCanvas(int width, int height, bool is_opaque,
                 CGContextRef context);
  PlatformCanvas(int width, int height, bool is_opaque, uint8_t* context);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__sun) || defined(ANDROID)
  // Linux ---------------------------------------------------------------------

  // Construct a canvas from the given memory region. The memory is not cleared
  // first. @data must be, at least, @height * StrideForWidth(@width) bytes.
  PlatformCanvas(int width, int height, bool is_opaque, uint8_t* data);
#endif

  virtual ~PlatformCanvas();

#if defined(WIN32)
  // For two-part init, call if you use the no-argument constructor above. Note
  // that we want this to optionally match the Linux initialize if you only
  // pass 3 arguments, hence the evil default argument.
  bool initialize(int width, int height, bool is_opaque,
                  HANDLE shared_section = NULL);
#elif defined(__APPLE__)
  // For two-part init, call if you use the no-argument constructor above
  bool initialize(CGContextRef context, int width, int height, bool is_opaque);
  bool initialize(int width, int height, bool is_opaque, uint8_t* data = NULL);

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__sun) || defined(ANDROID)
  // For two-part init, call if you use the no-argument constructor above
  bool initialize(int width, int height, bool is_opaque, uint8_t* data = NULL);
#endif

  // Shared --------------------------------------------------------------------

  // Return the stride (length of a line in bytes) for the given width. Because
  // we use 32-bits per pixel, this will be roughly 4*width. However, for
  // alignment reasons we may wish to increase that.
  static size_t StrideForWidth(unsigned width);

  // Allow callers to see the non-virtual function even though we have an
  // override of a virtual one.
  // FIXME(brettw) is this necessary?
  using SkCanvas::clipRect;

 private:
  // Helper method used internally by the initialize() methods.
  bool initializeWithDevice(SkDevice* device);

  // Unimplemented. This is to try to prevent people from calling this function
  // on SkCanvas. SkCanvas' version is not virtual, so we can't prevent this
  // 100%, but hopefully this will make people notice and not use the function.
  // Calling SkCanvas' version will create a new device which is not compatible
  // with us and we will crash if somebody tries to draw into it with
  // CoreGraphics.
  virtual SkDevice* setBitmapDevice(const SkBitmap& bitmap);

  // Disallow copy and assign
  PlatformCanvas(const PlatformCanvas&);
  PlatformCanvas& operator=(const PlatformCanvas&);
};

// Returns the SkDevice pointer of the topmost rect with a non-empty
// clip. In practice, this is usually either the top layer or nothing, since
// we usually set the clip to new layers when we make them.
//
// If there is no layer that is not all clipped out, this will return a
// dummy device so callers do not have to check. If you are concerned about
// performance, check the clip before doing any painting.
//
// This is different than SkCanvas' getDevice, because that returns the
// bottommost device.
//
// Danger: the resulting device should not be saved. It will be invalidated
// by the next call to save() or restore().
SK_API SkDevice* GetTopDevice(const SkCanvas& canvas);

// Creates a canvas with raster bitmap backing.
// Set is_opaque if you are going to erase the bitmap and not use
// transparency: this will enable some optimizations.
SK_API SkCanvas* CreateBitmapCanvas(int width, int height, bool is_opaque);

// Non-crashing version of CreateBitmapCanvas
// returns NULL if allocation fails for any reason.
// Use this instead of CreateBitmapCanvas in places that are likely to
// attempt to allocate very large canvases (therefore likely to fail),
// and where it is possible to recover gracefully from the failed allocation.
SK_API SkCanvas* TryCreateBitmapCanvas(int width, int height, bool is_opaque);

// Returns true if native platform routines can be used to draw on the
// given canvas. If this function returns false, BeginPlatformPaint will
// return NULL PlatformSurface.
SK_API bool SupportsPlatformPaint(const SkCanvas* canvas);

// Draws into the a native platform surface, |context|.  Forwards to
// DrawToNativeContext on a PlatformDevice instance bound to the top device.
// If no PlatformDevice instance is bound, is a no-operation.
SK_API void DrawToNativeContext(SkCanvas* canvas, PlatformSurface context,
                                int x, int y, const PlatformRect* src_rect);

// Sets the opacity of each pixel in the specified region to be opaque.
SK_API void MakeOpaque(SkCanvas* canvas, int x, int y, int width, int height);

// These calls should surround calls to platform drawing routines, the
// surface returned here can be used with the native platform routines.
//
// Call EndPlatformPaint when you are done and want to use skia operations
// after calling the platform-specific BeginPlatformPaint; this will
// synchronize the bitmap to OS if necessary.
SK_API PlatformSurface BeginPlatformPaint(SkCanvas* canvas);
SK_API void EndPlatformPaint(SkCanvas* canvas);

// Helper class for pairing calls to BeginPlatformPaint and EndPlatformPaint.
// Upon construction invokes BeginPlatformPaint, and upon destruction invokes
// EndPlatformPaint.
class SK_API ScopedPlatformPaint {
 public:
  explicit ScopedPlatformPaint(SkCanvas* canvas) : canvas_(canvas) {
    platform_surface_ = BeginPlatformPaint(canvas);
  }
  ~ScopedPlatformPaint() { EndPlatformPaint(canvas_); }

  // Returns the PlatformSurface to use for native platform drawing calls.
  PlatformSurface GetPlatformSurface() { return platform_surface_; }
 private:
  SkCanvas* canvas_;
  PlatformSurface platform_surface_;

  // Disallow copy and assign
  ScopedPlatformPaint(const ScopedPlatformPaint&);
  ScopedPlatformPaint& operator=(const ScopedPlatformPaint&);
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_H_
