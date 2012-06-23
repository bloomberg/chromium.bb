
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_MAC_H_
#define SKIA_EXT_CANVAS_PAINT_MAC_H_
#pragma once

#include "skia/ext/canvas_paint_common.h"
#include "skia/ext/platform_canvas.h"

#import <Cocoa/Cocoa.h>

namespace skia {

// A class designed to translate skia painting into a region to the current
// graphics context.  On construction, it will set up a context for painting
// into, and on destruction, it will commit it to the current context.
// Note: The created context is always inialized to (0, 0, 0, 0).
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
      GetPlatformCanvas(this)->restoreToCount(1);

      // Blit the dirty rect to the current context.
      CGImageRef image = CGBitmapContextCreateImage(context_);
      CGRect dest_rect = NSRectToCGRect(rectangle_);

      CGContextRef destination_context =
          (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
      CGContextSaveGState(destination_context);
      CGContextSetBlendMode(
          destination_context,
          composite_alpha_ ? kCGBlendModeNormal : kCGBlendModeCopy);

      if ([[NSGraphicsContext currentContext] isFlipped]) {
        // Mirror context on the target's rect middle scanline.
        CGContextTranslateCTM(destination_context, 0.0, NSMidY(rectangle_));
        CGContextScaleCTM(destination_context, 1.0, -1.0);
        CGContextTranslateCTM(destination_context, 0.0, -NSMidY(rectangle_));
      }

      CGContextDrawImage(destination_context, dest_rect, image);
      CGContextRestoreGState(destination_context);

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
    return NSIsEmptyRect(rectangle_);
  }

  const NSRect& rectangle() const {
    return rectangle_;
  }

 private:
  void init(bool opaque) {
    CGContextRef destination_context =
        (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGRect scaled_unit_rect = CGContextConvertRectToDeviceSpace(
        destination_context, CGRectMake(0, 0, 1, 1));
    CGFloat x_scale = scaled_unit_rect.size.width;
    CGFloat y_scale = scaled_unit_rect.size.height;

    PlatformCanvas* canvas = GetPlatformCanvas(this);
    if (!canvas->initialize(NSWidth(rectangle_) * x_scale,
                            NSHeight(rectangle_) * y_scale,
                            opaque, NULL)) {
      // Cause a deliberate crash.
      *(volatile char*) 0 = 0;
    }
    canvas->clear(SkColorSetARGB(0, 0, 0, 0));

    // Need to translate so that the dirty region appears at the origin of the
    // surface.
    canvas->translate(-SkDoubleToScalar(NSMinX(rectangle_) * x_scale),
                      -SkDoubleToScalar(NSMinY(rectangle_) * y_scale));
    // Scale appropriately.
    canvas->scale(SkFloatToScalar(x_scale), SkFloatToScalar(y_scale));

    context_ = GetBitmapContext(GetTopDevice(*canvas));
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
