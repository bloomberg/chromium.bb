// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_mac.h"

#include <time.h>

#include "base/mac_util.h"
#include "base/ref_counted.h"
#include "skia/ext/bitmap_platform_device_data.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkUtils.h"

namespace skia {

namespace {

// Constrains position and size to fit within available_size. If |size| is -1,
// all the |available_size| is used. Returns false if the position is out of
// |available_size|.
bool Constrain(int available_size, int* position, int *size) {
  if (*size < -2)
    return false;

  if (*position < 0) {
    if (*size != -1)
      *size += *position;
    *position = 0;
  }
  if (*size == 0 || *position >= available_size)
    return false;

  if (*size > 0) {
    int overflow = (*position + *size) - available_size;
    if (overflow > 0) {
      *size -= overflow;
    }
  } else {
    // Fill up available size.
    *size = available_size - *position;
  }
  return true;
}

static CGContextRef CGContextForData(void* data, int width, int height) {
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  // Allocate a bitmap context with 4 components per pixel (BGRA).  Apple
  // recommends these flags for improved CG performance.
  CGContextRef context =
      CGBitmapContextCreate(data, width, height, 8, width * 4,
                            mac_util::GetSystemColorSpace(),
                            kCGImageAlphaPremultipliedFirst |
                                kCGBitmapByteOrder32Host);
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS

  if (!context)
    return NULL;

  // Change the coordinate system to match WebCore's
  CGContextTranslateCTM(context, 0, height);
  CGContextScaleCTM(context, 1.0, -1.0);

  return context;
}

}  // namespace

BitmapPlatformDevice::BitmapPlatformDeviceData::BitmapPlatformDeviceData(
    CGContextRef bitmap)
    : bitmap_context_(bitmap),
      config_dirty_(true) {  // Want to load the config next time.
  SkASSERT(bitmap_context_);
  // Initialize the clip region to the entire bitmap.

  SkIRect rect;
  rect.set(0, 0,
           CGBitmapContextGetWidth(bitmap_context_),
           CGBitmapContextGetHeight(bitmap_context_));
  clip_region_ = SkRegion(rect);
  transform_.reset();
  CGContextRetain(bitmap_context_);
  // We must save the state once so that we can use the restore/save trick
  // in LoadConfig().
  CGContextSaveGState(bitmap_context_);
}

BitmapPlatformDevice::BitmapPlatformDeviceData::~BitmapPlatformDeviceData() {
  if (bitmap_context_)
    CGContextRelease(bitmap_context_);
}

void BitmapPlatformDevice::BitmapPlatformDeviceData::SetMatrixClip(
    const SkMatrix& transform,
    const SkRegion& region) {
  transform_ = transform;
  clip_region_ = region;
  config_dirty_ = true;
}

void BitmapPlatformDevice::BitmapPlatformDeviceData::LoadConfig() {
  if (!config_dirty_ || !bitmap_context_)
    return;  // Nothing to do.
  config_dirty_ = false;

  // We must restore and then save the state of the graphics context since the
  // calls to Load the clipping region to the context are strictly cummulative,
  // i.e., you can't replace a clip rect, other than with a save/restore.
  // But this implies that no other changes to the state are done elsewhere.
  // If we ever get to need to change this, then we must replace the clip rect
  // calls in LoadClippingRegionToCGContext() with an image mask instead.
  CGContextRestoreGState(bitmap_context_);
  CGContextSaveGState(bitmap_context_);
  LoadTransformToCGContext(bitmap_context_, transform_);
  LoadClippingRegionToCGContext(bitmap_context_, clip_region_, transform_);
}


// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(CGContextRef context,
                                                   int width,
                                                   int height,
                                                   bool is_opaque) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  if (bitmap.allocPixels() != true)
    return NULL;

  void* data = NULL;
  if (context) {
    data = CGBitmapContextGetData(context);
    bitmap.setPixels(data);
  } else {
    data = bitmap.getPixels();

    // Note: The Windows implementation clears the Bitmap later on.
    // This bears mentioning since removal of this line makes the
    // unit tests only fail periodically (or when MallocPreScribble is set).
    bitmap.eraseARGB(0, 0, 0, 0);
  }

  bitmap.setIsOpaque(is_opaque);

  // If we were given data, then don't clobber it!
#ifndef NDEBUG
  if (!context && is_opaque) {
    // To aid in finding bugs, we set the background color to something
    // obviously wrong so it will be noticable when it is not cleared
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
  }
#endif

  if (!context)
    context = CGContextForData(data, width, height);
  else
    CGContextRetain(context);

  BitmapPlatformDevice* rv = new BitmapPlatformDevice(
      new BitmapPlatformDeviceData(context), bitmap);

  // The device object took ownership of the graphics context with its own
  // CGContextRetain call.
  CGContextRelease(context);

  return rv;
}

BitmapPlatformDevice* BitmapPlatformDevice::CreateWithData(uint8_t* data,
                                                           int width,
                                                           int height,
                                                           bool is_opaque) {
  CGContextRef context = NULL;
  if (data)
    context = CGContextForData(data, width, height);

  BitmapPlatformDevice* rv = Create(context, width, height, is_opaque);

  // The device object took ownership of the graphics context with its own
  // CGContextRetain call.
  if (context)
    CGContextRelease(context);

  return rv;
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDevice::BitmapPlatformDevice(
    BitmapPlatformDeviceData* data, const SkBitmap& bitmap)
    : PlatformDevice(bitmap),
      data_(data) {
}

// The copy constructor just adds another reference to the underlying data.
// We use a const cast since the default Skia definitions don't define the
// proper constedness that we expect (accessBitmap should really be const).
BitmapPlatformDevice::BitmapPlatformDevice(
    const BitmapPlatformDevice& other)
    : PlatformDevice(
          const_cast<BitmapPlatformDevice&>(other).accessBitmap(true)),
      data_(other.data_) {
  data_->ref();
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
  data_->unref();
}

BitmapPlatformDevice& BitmapPlatformDevice::operator=(
    const BitmapPlatformDevice& other) {
  data_ = other.data_;
  data_->ref();
  return *this;
}

CGContextRef BitmapPlatformDevice::GetBitmapContext() {
  data_->LoadConfig();
  return data_->bitmap_context();
}

void BitmapPlatformDevice::setMatrixClip(const SkMatrix& transform,
                                         const SkRegion& region) {
  data_->SetMatrixClip(transform, region);
}

void BitmapPlatformDevice::DrawToContext(CGContextRef context, int x, int y,
                                         const CGRect* src_rect) {
  bool created_dc = false;
  if (!data_->bitmap_context()) {
    created_dc = true;
    GetBitmapContext();
  }

  // this should not make a copy of the bits, since we're not doing
  // anything to trigger copy on write
  CGImageRef image = CGBitmapContextCreateImage(data_->bitmap_context());
  CGRect bounds;
  bounds.origin.x = x;
  bounds.origin.y = y;
  if (src_rect) {
    bounds.size.width = src_rect->size.width;
    bounds.size.height = src_rect->size.height;
    CGImageRef sub_image = CGImageCreateWithImageInRect(image, *src_rect);
    CGContextDrawImage(context, bounds, sub_image);
    CGImageRelease(sub_image);
  } else {
    bounds.size.width = width();
    bounds.size.height = height();
    CGContextDrawImage(context, bounds, image);
  }
  CGImageRelease(image);

  if (created_dc)
    data_->ReleaseBitmapContext();
}

// Returns the color value at the specified location.
SkColor BitmapPlatformDevice::getColorAt(int x, int y) {
  const SkBitmap& bitmap = accessBitmap(true);
  SkAutoLockPixels lock(bitmap);
  uint32_t* data = bitmap.getAddr32(0, 0);
  return static_cast<SkColor>(data[x + y * width()]);
}

void BitmapPlatformDevice::onAccessBitmap(SkBitmap*) {
  // Not needed in CoreGraphics
}

void BitmapPlatformDevice::processPixels(int x, int y,
                                         int width, int height,
                                         adjustAlpha adjustor) {
  const SkBitmap& bitmap = accessBitmap(true);
  const SkMatrix& matrix = data_->transform();
  int bitmap_start_x = SkScalarRound(matrix.getTranslateX()) + x;
  int bitmap_start_y = SkScalarRound(matrix.getTranslateY()) + y;

  SkAutoLockPixels lock(bitmap);
  if (Constrain(bitmap.width(), &bitmap_start_x, &width) &&
      Constrain(bitmap.height(), &bitmap_start_y, &height)) {
    uint32_t* data = bitmap.getAddr32(0, 0);
    size_t row_words = bitmap.rowBytes() / 4;
    for (int i = 0; i < height; i++) {
      size_t offset = (i + bitmap_start_y) * row_words + bitmap_start_x;
      for (int j = 0; j < width; j++) {
        adjustor(data + offset + j);
      }
    }
  }
}

}  // namespace skia
