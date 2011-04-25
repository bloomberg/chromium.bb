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
  explicit PlatformCanvas(SkDeviceFactory* factory);
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
      defined(__Solaris__)
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
      defined(__Solaris__)
  // For two-part init, call if you use the no-argument constructor above
  bool initialize(int width, int height, bool is_opaque, uint8_t* data = NULL);
#endif

  // Shared --------------------------------------------------------------------

  // These calls should surround calls to platform drawing routines, the
  // surface returned here can be used with the native platform routines
  //
  // Call endPlatformPaint when you are done and want to use Skia operations
  // after calling the platform-specific beginPlatformPaint; this will
  // synchronize the bitmap to OS if necessary.
  PlatformDevice::PlatformSurface beginPlatformPaint() const;
  void endPlatformPaint() const;

  // Returns the platform device pointer of the topmost rect with a non-empty
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
  PlatformDevice& getTopPlatformDevice() const;

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

  // Disallow copy and assign.
  PlatformCanvas(const PlatformCanvas&);
  PlatformCanvas& operator=(const PlatformCanvas&);
};

// Creates a canvas with raster bitmap backing.
// Set is_opaque if you are going to erase the bitmap and not use
// transparency: this will enable some optimizations.
SK_API SkCanvas* CreateBitmapCanvas(int width, int height, bool is_opaque);

// Returns true if native platform routines can be used to draw on the
// given canvas. If this function returns false, BeginPlatformPaint will
// return NULL PlatformSurface.
SK_API bool SupportsPlatformPaint(const SkCanvas* canvas);

// These calls should surround calls to platform drawing routines, the
// surface returned here can be used with the native platform routines.
//
// Call EndPlatformPaint when you are done and want to use skia operations
// after calling the platform-specific BeginPlatformPaint; this will
// synchronize the bitmap to OS if necessary.
//
// Note: These functions will eventually replace
// PlatformCanvas::beginPlatformPaint and PlatformCanvas::endPlatformPaint.
SK_API PlatformDevice::PlatformSurface BeginPlatformPaint(SkCanvas* canvas);
SK_API void EndPlatformPaint(SkCanvas* canvas);

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_H_
