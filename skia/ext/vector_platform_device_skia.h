// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
#define SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
#pragma once

#include "base/basictypes.h"
#include "base/logging.h"
#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTScopedPtr.h"
#include "third_party/skia/include/pdf/SkPDFDevice.h"

class SkClipStack;
class SkMatrix;
struct SkIRect;
struct SkRect;

namespace skia {

class BitmapPlatformDevice;

class VectorPlatformDeviceSkiaFactory : public SkDeviceFactory {
 public:
  virtual SkDevice* newDevice(SkCanvas* notUsed, SkBitmap::Config config,
                              int width, int height, bool isOpaque,
                              bool isForLayer);
 private:
  SkPDFDeviceFactory factory_;
};

class VectorPlatformDeviceSkia : public PlatformDevice {
 public:
  SK_API VectorPlatformDeviceSkia(SkPDFDevice* pdf_device);
  ~VectorPlatformDeviceSkia();

  SkPDFDevice* PdfDevice() { return pdf_device_.get(); }

  // PlatformDevice methods.
  virtual bool IsVectorial();
  virtual bool IsNativeFontRenderingAllowed();

  virtual PlatformSurface BeginPlatformPaint();
  virtual void EndPlatformPaint();

  // SkDevice methods.
  virtual uint32_t getDeviceCapabilities();

  virtual int width() const;
  virtual int height() const;
  virtual void setMatrixClip(const SkMatrix& matrix, const SkRegion& region,
                             const SkClipStack& stack);
  virtual bool readPixels(const SkIRect& srcRect, SkBitmap* bitmap);

  virtual void drawPaint(const SkDraw& draw, const SkPaint& paint);
  virtual void drawPoints(const SkDraw& draw, SkCanvas::PointMode mode,
                          size_t count, const SkPoint[], const SkPaint& paint);
  virtual void drawRect(const SkDraw& draw, const SkRect& rect,
                        const SkPaint& paint);
  virtual void drawPath(const SkDraw& draw, const SkPath& path,
                        const SkPaint& paint, const SkMatrix* prePathMatrix,
                        bool pathIsMutable);
  virtual void drawBitmap(const SkDraw& draw, const SkBitmap& bitmap,
                          const SkIRect* srcRectOrNull, const SkMatrix& matrix,
                          const SkPaint& paint);
  virtual void drawSprite(const SkDraw& draw, const SkBitmap& bitmap,
                          int x, int y, const SkPaint& paint);
  virtual void drawText(const SkDraw& draw, const void* text, size_t len,
                        SkScalar x, SkScalar y, const SkPaint& paint);
  virtual void drawPosText(const SkDraw& draw, const void* text, size_t len,
                           const SkScalar pos[], SkScalar constY,
                           int scalarsPerPos, const SkPaint& paint);
  virtual void drawTextOnPath(const SkDraw& draw, const void* text, size_t len,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint);
  virtual void drawVertices(const SkDraw& draw, SkCanvas::VertexMode,
                            int vertexCount, const SkPoint verts[],
                            const SkPoint texs[], const SkColor colors[],
                            SkXfermode* xmode, const uint16_t indices[],
                            int indexCount, const SkPaint& paint);
  virtual void drawDevice(const SkDraw& draw, SkDevice*, int x, int y,
                          const SkPaint&);

#if defined(OS_WIN)
  virtual void drawToHDC(HDC dc, int x, int y, const RECT* src_rect);
#endif

 protected:
  // Override from SkDevice (through PlatformDevice).
  virtual SkDeviceFactory* onNewDeviceFactory();

 private:
  SkRefPtr<SkPDFDevice> pdf_device_;
  SkRefPtr<BitmapPlatformDevice> raster_surface_;

  DISALLOW_COPY_AND_ASSIGN(VectorPlatformDeviceSkia);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
