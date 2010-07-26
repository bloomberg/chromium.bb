// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_CANVAS_H_
#define SKIA_EXT_VECTOR_CANVAS_H_
#pragma once

#include "skia/ext/platform_canvas.h"
#include "skia/ext/vector_platform_device.h"

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
typedef struct _cairo cairo_t;
#endif

namespace skia {

// This class is a specialization of the regular PlatformCanvas. It is designed
// to work with a VectorDevice to manage platform-specific drawing. It allows
// using both Skia operations and platform-specific operations. It *doesn't*
// support reading back from the bitmap backstore since it is not used.
class VectorCanvas : public PlatformCanvas {
 public:
  VectorCanvas();
#if defined(WIN32)
  VectorCanvas(HDC dc, int width, int height);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  // Caller owns |context|. Ownership is not transferred.
  VectorCanvas(cairo_t* context, int width, int height);
#endif
  virtual ~VectorCanvas();

  // For two-part init, call if you use the no-argument constructor above
#if defined(WIN32)
  bool initialize(HDC context, int width, int height);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  // Ownership of |context| is not transferred.
  bool initialize(cairo_t* context, int width, int height);
#endif

  virtual SkBounder* setBounder(SkBounder* bounder);
#if defined(WIN32) || defined(__linux__) || defined(__FreeBSD__) || \
  defined(__OpenBSD__)
  virtual SkDevice* createDevice(SkBitmap::Config config,
                                 int width, int height,
                                 bool is_opaque, bool isForLayer);
#endif
  virtual SkDrawFilter* setDrawFilter(SkDrawFilter* filter);

 private:
#if defined(WIN32)
  // |shared_section| is in fact the HDC used for output. |is_opaque| is unused.
  virtual SkDevice* createPlatformDevice(int width, int height, bool is_opaque,
                                         HANDLE shared_section);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  // Ownership of |context| is not transferred. |is_opaque| is unused.
  virtual SkDevice* createPlatformDevice(cairo_t* context,
                                         int width, int height,
                                         bool is_opaque);
#endif

  // Returns true if the top device is vector based and not bitmap based.
  bool IsTopDeviceVectorial() const;

  // Copy & assign are not supported.
  VectorCanvas(const VectorCanvas&);
  const VectorCanvas& operator=(const VectorCanvas&);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_CANVAS_H_

