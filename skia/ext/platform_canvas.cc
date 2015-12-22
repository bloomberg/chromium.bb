// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkMetaData.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace {

#if defined(OS_MACOSX)
const char kIsPreviewMetafileKey[] = "CrIsPreviewMetafile";

void SetBoolMetaData(const SkCanvas& canvas, const char* key,  bool value) {
  SkMetaData& meta = skia::GetMetaData(canvas);
  meta.setBool(key, value);
}

bool GetBoolMetaData(const SkCanvas& canvas, const char* key) {
  bool value;
  SkMetaData& meta = skia::GetMetaData(canvas);
  if (!meta.findBool(key, &value))
    value = false;
  return value;
}
#endif

}  // namespace

namespace skia {

SkBaseDevice* GetTopDevice(const SkCanvas& canvas) {
  return canvas.getTopDevice(true);
}

SkBitmap ReadPixels(SkCanvas* canvas) {
  SkBitmap bitmap;
  bitmap.setInfo(canvas->imageInfo());
  canvas->readPixels(&bitmap, 0, 0);
  return bitmap;
}

bool GetWritablePixels(SkCanvas* canvas, SkPixmap* result) {
  if (!canvas || !result) {
    return false;
  }

  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas->accessTopLayerPixels(&info, &row_bytes);
  if (!pixels) {
    result->reset();
    return false;
  }

  result->reset(info, pixels, row_bytes);
  return true;
}

bool SupportsPlatformPaint(const SkCanvas* canvas) {
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  return platform_device && platform_device->SupportsPlatformPaint();
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

void MakeOpaque(SkCanvas* canvas, int x, int y, int width, int height) {
  if (width <= 0 || height <= 0)
    return;

  SkRect rect;
  rect.setXYWH(SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(width), SkIntToScalar(height));
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setXfermodeMode(SkXfermode::kDstATop_Mode);
  canvas->drawRect(rect, paint);
}

size_t PlatformCanvasStrideForWidth(unsigned width) {
  return 4 * width;
}

SkCanvas* CreateCanvas(const skia::RefPtr<SkBaseDevice>& device, OnFailureType failureType) {
  if (!device) {
    if (CRASH_ON_FAILURE == failureType)
      SK_CRASH();
    return nullptr;
  }
  return new SkCanvas(device.get());
}

SkMetaData& GetMetaData(const SkCanvas& canvas) {
  SkBaseDevice* device = canvas.getDevice();
  DCHECK(device != nullptr);
  return device->getMetaData();
}

#if defined(OS_MACOSX)
void SetIsPreviewMetafile(const SkCanvas& canvas, bool is_preview) {
  SetBoolMetaData(canvas, kIsPreviewMetafileKey, is_preview);
}

bool IsPreviewMetafile(const SkCanvas& canvas) {
  return GetBoolMetaData(canvas, kIsPreviewMetafileKey);
}

CGContextRef GetBitmapContext(const SkCanvas& canvas) {
  SkBaseDevice* device = GetTopDevice(canvas);
  PlatformDevice* platform_device = GetPlatformDevice(device);
  return platform_device ?  platform_device->GetBitmapContext() :
      nullptr;
}

#endif


}  // namespace skia
