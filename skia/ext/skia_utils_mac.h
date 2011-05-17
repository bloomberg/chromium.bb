// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_MAC_H_
#define SKIA_EXT_SKIA_UTILS_MAC_H_
#pragma once

#include <CoreGraphics/CGColor.h>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"

struct SkIRect;
struct SkPoint;
struct SkRect;
class SkBitmap;
class SkCanvas;
class SkMatrix;
#ifdef __LP64__
typedef CGSize NSSize;
#else
typedef struct _NSSize NSSize;
#endif

#ifdef __OBJC__
@class NSImage;
@class NSImageRep;
#else
class NSImage;
class NSImageRep;
#endif

namespace gfx {

// Converts a Skia point to a CoreGraphics CGPoint.
// Both use same in-memory format.
inline const CGPoint& SkPointToCGPoint(const SkPoint& point) {
  return reinterpret_cast<const CGPoint&>(point);
}

// Converts a CoreGraphics point to a Skia CGPoint.
// Both use same in-memory format.
inline const SkPoint& CGPointToSkPoint(const CGPoint& point) {
  return reinterpret_cast<const SkPoint&>(point);
}

// Matrix converters.
CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix);

// Rectangle converters.
SkRect CGRectToSkRect(const CGRect& rect);
SkIRect CGRectToSkIRect(const CGRect& rect);

// Converts a Skia rect to a CoreGraphics CGRect.
CGRect SkIRectToCGRect(const SkIRect& rect);
CGRect SkRectToCGRect(const SkRect& rect);

// Converts CGColorRef to the ARGB layout Skia expects.
SkColor CGColorRefToSkColor(CGColorRef color);

// Converts ARGB to CGColorRef.
CGColorRef SkColorToCGColorRef(SkColor color);

// Converts a CGImage to a SkBitmap.
SkBitmap CGImageToSkBitmap(CGImageRef image);

// Draws an NSImage with a given size into a SkBitmap.
SkBitmap NSImageToSkBitmap(NSImage* image, NSSize size, bool is_opaque);

// Draws an NSImageRep with a given size into a SkBitmap.
SkBitmap NSImageRepToSkBitmap(NSImageRep* image, NSSize size, bool is_opaque);

// Given an SkBitmap and a color space, return an autoreleased NSImage.
NSImage* SkBitmapToNSImageWithColorSpace(const SkBitmap& icon,
                                         CGColorSpaceRef colorSpace);

// Given an SkBitmap, return an autoreleased NSImage in the generic color space.
// DEPRECATED, use SkBitmapToNSImageWithColorSpace() instead.
// TODO(thakis): Remove this -- http://crbug.com/69432
NSImage* SkBitmapToNSImage(const SkBitmap& icon);

// Given a vector of SkBitmaps, return an NSImage with each bitmap added
// as a representation.
NSImage* SkBitmapsToNSImage(const std::vector<const SkBitmap*>& bitmaps);

// Returns |[NSImage imageNamed:@"NSApplicationIcon"]| as SkBitmap.
SkBitmap AppplicationIconAtSize(int size);

// Converts a SkCanvas temporarily to a CGContext
class SkiaBitLocker {
 public:
  explicit SkiaBitLocker(SkCanvas* canvas);
  ~SkiaBitLocker();
  CGContextRef cgContext();

 private:
  void releaseIfNeeded();
  SkCanvas* canvas_;
  CGContextRef cgContext_;
};


}  // namespace gfx

#endif  // SKIA_EXT_SKIA_UTILS_MAC_H_
