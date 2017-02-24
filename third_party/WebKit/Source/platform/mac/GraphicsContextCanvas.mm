// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mac/GraphicsContextCanvas.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "skia/ext/skia_utils_mac.h"
#include "wtf/RetainPtr.h"

namespace blink {

GraphicsContextCanvas::GraphicsContextCanvas(PaintCanvas* canvas,
                                             const SkIRect& userClipRect,
                                             SkScalar bitmapScaleFactor)
    : m_canvas(canvas),
      m_cgContext(0),
      m_bitmapScaleFactor(bitmapScaleFactor),
      m_useDeviceBits(false),
      m_bitmapIsDummy(false) {
  m_canvas->save();
  m_canvas->clipRect(SkRect::MakeFromIRect(userClipRect));
}

GraphicsContextCanvas::~GraphicsContextCanvas() {
  releaseIfNeeded();
  m_canvas->restore();
}

SkIRect GraphicsContextCanvas::computeDirtyRect() {
  // If the user specified a clip region, assume that it was tight and that the
  // dirty rect is approximately the whole bitmap.
  return SkIRect::MakeWH(m_offscreen.width(), m_offscreen.height());
}

// This must be called to balance calls to cgContext
void GraphicsContextCanvas::releaseIfNeeded() {
  if (!m_cgContext)
    return;
  if (!m_useDeviceBits && !m_bitmapIsDummy) {
    // Find the bits that were drawn to.
    SkIRect bounds = computeDirtyRect();
    SkBitmap subset;
    if (!m_offscreen.extractSubset(&subset, bounds)) {
      return;
    }
    subset.setImmutable();  // Prevents a defensive copy inside Skia.
    m_canvas->save();
    m_canvas->setMatrix(SkMatrix::I());  // Reset back to device space.
    m_canvas->translate(bounds.x() + m_bitmapOffset.x(),
                        bounds.y() + m_bitmapOffset.y());
    m_canvas->scale(1.f / m_bitmapScaleFactor, 1.f / m_bitmapScaleFactor);
    m_canvas->drawBitmap(subset, 0, 0);
    m_canvas->restore();
  }
  CGContextRelease(m_cgContext);
  m_cgContext = 0;
  m_useDeviceBits = false;
  m_bitmapIsDummy = false;
}

CGContextRef GraphicsContextCanvas::cgContext() {
  releaseIfNeeded();  // This flushes any prior bitmap use

  SkIRect clip_bounds;
  if (!m_canvas->getDeviceClipBounds(&clip_bounds)) {
    // If the clip is empty, then there is nothing to draw. The caller may
    // attempt to draw (to-be-clipped) results, so ensure there is a dummy
    // non-NULL CGContext to use.
    m_bitmapIsDummy = true;
    clip_bounds = SkIRect::MakeXYWH(0, 0, 1, 1);
  }

  // remember the top/left, in case we need to compose this later
  m_bitmapOffset.set(clip_bounds.x(), clip_bounds.y());

  // Now make clip_bounds be relative to the current layer/device
  if (!m_bitmapIsDummy) {
    m_canvas->temporary_internal_describeTopLayer(nullptr, &clip_bounds);
  }

  SkPixmap devicePixels;
  ToPixmap(m_canvas, &devicePixels);

  // Only draw directly if we have pixels, and we're only rect-clipped.
  // If not, we allocate an offscreen and draw into that, relying on the
  // compositing step to apply skia's clip.
  m_useDeviceBits =
      devicePixels.addr() && m_canvas->isClipRect() && !m_bitmapIsDummy;
  WTF::RetainPtr<CGColorSpace> colorSpace(CGColorSpaceCreateDeviceRGB());

  int displayHeight;
  if (m_useDeviceBits) {
    SkPixmap subset;
    bool result = devicePixels.extractSubset(&subset, clip_bounds);
    DCHECK(result);
    if (!result)
      return 0;
    displayHeight = subset.height();
    m_cgContext = CGBitmapContextCreate(
        subset.writable_addr(), subset.width(), subset.height(), 8,
        subset.rowBytes(), colorSpace.get(),
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  } else {
    bool result = m_offscreen.tryAllocN32Pixels(
        SkScalarCeilToInt(m_bitmapScaleFactor * clip_bounds.width()),
        SkScalarCeilToInt(m_bitmapScaleFactor * clip_bounds.height()));
    DCHECK(result);
    if (!result)
      return 0;
    m_offscreen.eraseColor(0);
    displayHeight = m_offscreen.height();
    m_cgContext = CGBitmapContextCreate(
        m_offscreen.getPixels(), m_offscreen.width(), m_offscreen.height(), 8,
        m_offscreen.rowBytes(), colorSpace.get(),
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  }
  DCHECK(m_cgContext);

  SkMatrix matrix = m_canvas->getTotalMatrix();
  matrix.postTranslate(-SkIntToScalar(m_bitmapOffset.x()),
                       -SkIntToScalar(m_bitmapOffset.y()));
  matrix.postScale(m_bitmapScaleFactor, -m_bitmapScaleFactor);
  matrix.postTranslate(0, SkIntToScalar(displayHeight));

  CGContextConcatCTM(m_cgContext, skia::SkMatrixToCGAffineTransform(matrix));

  return m_cgContext;
}

bool GraphicsContextCanvas::hasEmptyClipRegion() const {
  return m_canvas->isClipEmpty();
}

}  // namespace blink
