// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mac/GraphicsContextCanvas.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "platform/wtf/RetainPtr.h"
#include "skia/ext/skia_utils_mac.h"

namespace blink {

GraphicsContextCanvas::GraphicsContextCanvas(PaintCanvas* canvas,
                                             const SkIRect& user_clip_rect,
                                             SkScalar bitmap_scale_factor)
    : canvas_(canvas),
      cg_context_(0),
      bitmap_scale_factor_(bitmap_scale_factor),
      use_device_bits_(false),
      bitmap_is_dummy_(false) {
  canvas_->save();
  canvas_->clipRect(SkRect::MakeFromIRect(user_clip_rect));
}

GraphicsContextCanvas::~GraphicsContextCanvas() {
  ReleaseIfNeeded();
  canvas_->restore();
}

SkIRect GraphicsContextCanvas::ComputeDirtyRect() {
  // If the user specified a clip region, assume that it was tight and that the
  // dirty rect is approximately the whole bitmap.
  return SkIRect::MakeWH(offscreen_.width(), offscreen_.height());
}

// This must be called to balance calls to cgContext
void GraphicsContextCanvas::ReleaseIfNeeded() {
  if (!cg_context_)
    return;
  if (!use_device_bits_ && !bitmap_is_dummy_) {
    // Find the bits that were drawn to.
    SkIRect bounds = ComputeDirtyRect();
    SkBitmap subset;
    if (!offscreen_.extractSubset(&subset, bounds)) {
      return;
    }
    subset.setImmutable();  // Prevents a defensive copy inside Skia.
    canvas_->save();
    canvas_->setMatrix(SkMatrix::I());  // Reset back to device space.
    canvas_->translate(bounds.x() + bitmap_offset_.x(),
                       bounds.y() + bitmap_offset_.y());
    canvas_->scale(1.f / bitmap_scale_factor_, 1.f / bitmap_scale_factor_);
    canvas_->drawBitmap(subset, 0, 0);
    canvas_->restore();
  }
  CGContextRelease(cg_context_);
  cg_context_ = 0;
  use_device_bits_ = false;
  bitmap_is_dummy_ = false;
}

CGContextRef GraphicsContextCanvas::CgContext() {
  ReleaseIfNeeded();  // This flushes any prior bitmap use

  SkIRect clip_bounds;
  if (!canvas_->getDeviceClipBounds(&clip_bounds)) {
    // If the clip is empty, then there is nothing to draw. The caller may
    // attempt to draw (to-be-clipped) results, so ensure there is a dummy
    // non-NULL CGContext to use.
    bitmap_is_dummy_ = true;
    clip_bounds = SkIRect::MakeXYWH(0, 0, 1, 1);
  }

  // remember the top/left, in case we need to compose this later
  bitmap_offset_.set(clip_bounds.x(), clip_bounds.y());

  SkPixmap device_pixels;
  ToPixmap(canvas_, &device_pixels);

  // Only draw directly if we have pixels, and we're only rect-clipped.
  // If not, we allocate an offscreen and draw into that, relying on the
  // compositing step to apply skia's clip.
  use_device_bits_ =
      device_pixels.addr() && canvas_->isClipRect() && !bitmap_is_dummy_;
  WTF::RetainPtr<CGColorSpace> color_space(CGColorSpaceCreateDeviceRGB());

  int display_height;
  if (use_device_bits_) {
    SkPixmap subset;
    bool result = device_pixels.extractSubset(&subset, clip_bounds);
    DCHECK(result);
    if (!result)
      return 0;
    display_height = subset.height();
    cg_context_ = CGBitmapContextCreate(
        subset.writable_addr(), subset.width(), subset.height(), 8,
        subset.rowBytes(), color_space.Get(),
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  } else {
    bool result = offscreen_.tryAllocN32Pixels(
        SkScalarCeilToInt(bitmap_scale_factor_ * clip_bounds.width()),
        SkScalarCeilToInt(bitmap_scale_factor_ * clip_bounds.height()));
    DCHECK(result);
    if (!result)
      return 0;
    offscreen_.eraseColor(0);
    display_height = offscreen_.height();
    cg_context_ = CGBitmapContextCreate(
        offscreen_.getPixels(), offscreen_.width(), offscreen_.height(), 8,
        offscreen_.rowBytes(), color_space.Get(),
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  }
  DCHECK(cg_context_);

  SkMatrix matrix = canvas_->getTotalMatrix();
  matrix.postTranslate(-SkIntToScalar(bitmap_offset_.x()),
                       -SkIntToScalar(bitmap_offset_.y()));
  matrix.postScale(bitmap_scale_factor_, -bitmap_scale_factor_);
  matrix.postTranslate(0, SkIntToScalar(display_height));

  CGContextConcatCTM(cg_context_, skia::SkMatrixToCGAffineTransform(matrix));

  return cg_context_;
}

bool GraphicsContextCanvas::HasEmptyClipRegion() const {
  return canvas_->isClipEmpty();
}

}  // namespace blink
