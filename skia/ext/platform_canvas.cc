// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "skia/ext/bitmap_platform_device.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

PlatformCanvas::PlatformCanvas() {}

SkDevice* PlatformCanvas::setBitmapDevice(const SkBitmap&) {
  SkASSERT(false);  // Should not be called.
  return NULL;
}

// static
size_t PlatformCanvas::StrideForWidth(unsigned width) {
  return 4 * width;
}

bool PlatformCanvas::initializeWithDevice(SkDevice* device) {
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // Was created with refcount 1, and setDevice also refs.
  return true;
}

SkCanvas* CreateBitmapCanvas(int width, int height, bool is_opaque) {
  return new PlatformCanvas(width, height, is_opaque);
}

SkCanvas* TryCreateBitmapCanvas(int width, int height, bool is_opaque) {
  PlatformCanvas* canvas = new PlatformCanvas();
  if (!canvas->initialize(width, height, is_opaque)) {
    delete canvas;
    canvas = NULL;
  }
  return canvas;
}

SkDevice* GetTopDevice(const SkCanvas& canvas) {
  SkCanvas::LayerIter iter(const_cast<SkCanvas*>(&canvas), false);
  return iter.device();
}

bool SupportsPlatformPaint(const SkCanvas* canvas) {
  // TODO(alokp): Rename IsNativeFontRenderingAllowed after removing these
  // calls from WebKit.
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  return platform_device && platform_device->IsNativeFontRenderingAllowed();
}

PlatformSurface BeginPlatformPaint(SkCanvas* canvas) {
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  if (platform_device)
    return platform_device->BeginPlatformPaint();

  return 0;
}

void EndPlatformPaint(SkCanvas* canvas) {
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  if (platform_device)
    platform_device->EndPlatformPaint();
}

void DrawToNativeContext(SkCanvas* canvas, PlatformSurface context, int x,
                         int y, const PlatformRect* src_rect) {
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  if (platform_device)
    platform_device->DrawToNativeContext(context, x, y, src_rect);
}

static SkPMColor MakeOpaqueXfermodeProc(SkPMColor src, SkPMColor dst) {
    return dst | (0xFF << SK_A32_SHIFT);
}

void MakeOpaque(SkCanvas* canvas, int x, int y, int width, int height) {
  if (width <= 0 || height <= 0)
    return;

  SkRect rect;
  rect.setXYWH(SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(width), SkIntToScalar(height));
  SkPaint paint;
  // so we don't draw anything on a device that ignores xfermodes
  paint.setColor(0);
  // install our custom mode
  paint.setXfermode(new SkProcXfermode(MakeOpaqueXfermodeProc))->unref();
  canvas->drawRect(rect, paint);
}

}  // namespace skia
