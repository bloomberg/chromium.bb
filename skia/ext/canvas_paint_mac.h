
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_MAC_H_
#define SKIA_EXT_CANVAS_PAINT_MAC_H_
#pragma once

#include "skia/ext/platform_canvas.h"

#import <Cocoa/Cocoa.h>

namespace skia {

// A class designed to translate skia painting into a region to the current
// graphics context.  On construction, it will set up a context for painting
// into, and on destruction, it will commit it to the current context.
template <class T>
class CanvasPaintT : public T {
 public:
  // This constructor assumes the result is opaque.
  explicit CanvasPaintT(NSRect dirtyRect)
      : context_(NULL),
        rectangle_(dirtyRect),
        composite_alpha_(false) {
    init(true);
  }

  CanvasPaintT(NSRect dirtyRect, bool opaque)
      : context_(NULL),
        rectangle_(dirtyRect),
        composite_alpha_(false) {
    init(opaque);
  }

  virtual ~CanvasPaintT() {
    if (!is_empty()) {
      T::restoreToCount(1);

      // Blit the dirty rect to the current context.
      CGImageRef image = CGBitmapContextCreateImage(context_);
      CGRect destRect = NSRectToCGRect(rectangle_);

      CGContextRef destinationContext =
          (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
      CGContextSaveGState(destinationContext);
      CGContextSetBlendMode(
          destinationContext,
          composite_alpha_ ? kCGBlendModeNormal : kCGBlendModeCopy);

      if ([[NSGraphicsContext currentContext] isFlipped]) {
        // Mirror context on the target's rect middle scanline.
        CGContextTranslateCTM(destinationContext, 0.0, NSMidY(rectangle_));
        CGContextScaleCTM(destinationContext, 1.0, -1.0);
        CGContextTranslateCTM(destinationContext, 0.0, -NSMidY(rectangle_));
      }

      CGContextDrawImage(destinationContext, destRect, image);
      CGContextRestoreGState(destinationContext);

      CFRelease(image);
    }
  }

  // If true, the data painted into the CanvasPaintT is blended onto the current
  // context, else it is copied.
  void set_composite_alpha(bool composite_alpha) {
    composite_alpha_ = composite_alpha;
  }

  // Returns true if the invalid region is empty. The caller should call this
  // function to determine if anything needs painting.
  bool is_empty() const {
    return rectangle_.size.width == 0 || rectangle_.size.height == 0;
  }

  const NSRect& rectangle() const {
    return rectangle_;
  }

 private:
  void init(bool opaque) {
    if (!T::initialize(rectangle_.size.width, rectangle_.size.height,
                       opaque, NULL)) {
      // Cause a deliberate crash;
      *(volatile char*) 0 = 0;
    }

    // Need to translate so that the dirty region appears at the origin of the
    // surface.
    T::translate(-SkDoubleToScalar(rectangle_.origin.x),
                 -SkDoubleToScalar(rectangle_.origin.y));

    context_ = T::getTopPlatformDevice().GetBitmapContext();
  }

  CGContext* context_;
  NSRect rectangle_;
  // See description above setter.
  bool composite_alpha_;

  // Disallow copy and assign.
  CanvasPaintT(const CanvasPaintT&);
  CanvasPaintT& operator=(const CanvasPaintT&);
};

typedef CanvasPaintT<PlatformCanvas> PlatformCanvasPaint;

}  // namespace skia


#endif  // SKIA_EXT_CANVAS_PAINT_MAC_H_
