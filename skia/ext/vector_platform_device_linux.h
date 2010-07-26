// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_PLATFORM_DEVICE_LINUX_H_
#define SKIA_EXT_VECTOR_PLATFORM_DEVICE_LINUX_H_
#pragma once

#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace skia {

// This device is basically a wrapper that provides a surface for SkCanvas
// to draw into. It is basically an adaptor which converts skia APIs into
// cooresponding Cairo APIs and outputs to a Cairo surface. Please NOTE that
// since it is completely vectorial, the bitmap content in it is thus
// meaningless.
class VectorPlatformDevice : public PlatformDevice {
 public:
  // Factory function. Ownership of |context| is not transferred.
  static VectorPlatformDevice* create(PlatformSurface context,
                                      int width, int height);
  virtual ~VectorPlatformDevice();

  virtual bool IsVectorial() { return true; }
  virtual PlatformSurface beginPlatformPaint() { return context_; }

  // We translate following skia APIs into corresponding Cairo APIs.
  virtual void drawBitmap(const SkDraw& draw, const SkBitmap& bitmap,
                          const SkMatrix& matrix, const SkPaint& paint);
  virtual void drawDevice(const SkDraw& draw, SkDevice*, int x, int y,
                          const SkPaint&);
  virtual void drawPaint(const SkDraw& draw, const SkPaint& paint);
  virtual void drawPath(const SkDraw& draw, const SkPath& path,
                        const SkPaint& paint);
  virtual void drawPoints(const SkDraw& draw, SkCanvas::PointMode mode,
                          size_t count, const SkPoint[], const SkPaint& paint);
  virtual void drawPosText(const SkDraw& draw, const void* text, size_t len,
                           const SkScalar pos[], SkScalar constY,
                           int scalarsPerPos, const SkPaint& paint);
  virtual void drawRect(const SkDraw& draw, const SkRect& r,
                        const SkPaint& paint);
  virtual void drawSprite(const SkDraw& draw, const SkBitmap& bitmap,
                          int x, int y, const SkPaint& paint);
  virtual void drawText(const SkDraw& draw, const void* text, size_t len,
                        SkScalar x, SkScalar y, const SkPaint& paint);
  virtual void drawTextOnPath(const SkDraw& draw, const void* text, size_t len,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint);
  virtual void drawVertices(const SkDraw& draw, SkCanvas::VertexMode,
                            int vertexCount,
                            const SkPoint verts[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint);
  virtual void setMatrixClip(const SkMatrix& transform,
                             const SkRegion& region);

  // Clean up cached fonts. It is an error to call this while some
  // VectorPlatformDevice callee is still using fonts created for it by this
  // class.
  static void ClearFontCache();

 protected:
  explicit VectorPlatformDevice(PlatformSurface context,
                                const SkBitmap& bitmap);

 private:
  // Apply paint's color in the context.
  void ApplyPaintColor(const SkPaint& paint);

  // Apply path's fill style in the context.
  void ApplyFillStyle(const SkPath& path);

  // Apply paint's stroke style in the context.
  void ApplyStrokeStyle(const SkPaint& paint);

  // Perform painting.
  void DoPaintStyle(const SkPaint& paint);

  // Draws a bitmap in the the device, using the currently loaded matrix.
  void InternalDrawBitmap(const SkBitmap& bitmap, int x, int y,
                          const SkPaint& paint);

  // Set up the clipping region for the context. Please note that now we only
  // use the bounding box of the region for clipping.
  // TODO(myhuang): Support non-rectangular clipping.
  void LoadClipRegion(const SkRegion& clip);

  // Use identity matrix to set up context's transformation.
  void LoadIdentityTransformToContext();

  // Use matrix to set up context's transformation.
  void LoadTransformToContext(const SkMatrix& matrix);

  // Selects the font associated with |font_id| in |context|.
  // Return true on success.
  bool SelectFontById(uint32_t font_id);

  // Transformation assigned to the context.
  SkMatrix transform_;

  // The current clipping region.
  SkRegion clip_region_;

  // Device context.
  PlatformSurface context_;

  // Copy & assign are not supported.
  VectorPlatformDevice(const VectorPlatformDevice&);
  const VectorPlatformDevice& operator=(const VectorPlatformDevice&);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_PLATFORM_DEVICE_LINUX_H_
