// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_linux.h"

#include <cairo/cairo.h>

namespace skia {

namespace {

void LoadMatrixToContext(cairo_t* context, const SkMatrix& matrix) {
  cairo_matrix_t cairo_matrix;
  cairo_matrix_init(&cairo_matrix,
                    SkScalarToFloat(matrix.getScaleX()),
                    SkScalarToFloat(matrix.getSkewY()),
                    SkScalarToFloat(matrix.getSkewX()),
                    SkScalarToFloat(matrix.getScaleY()),
                    SkScalarToFloat(matrix.getTranslateX()),
                    SkScalarToFloat(matrix.getTranslateY()));
  cairo_set_matrix(context, &cairo_matrix);
}

void LoadClipToContext(cairo_t* context, const SkRegion& clip) {
  cairo_reset_clip(context);

  // TODO(brettw) support non-rect clips.
  SkIRect bounding = clip.getBounds();
  cairo_rectangle(context, bounding.fLeft, bounding.fTop,
                  bounding.fRight - bounding.fLeft,
                  bounding.fBottom - bounding.fTop);
  cairo_clip(context);
}

}  // namespace

// -----------------------------------------------------------------------------
// These objects are reference counted and own a Cairo surface. The surface is
// the backing store for a Skia bitmap and we reference count it so that we can
// copy BitmapPlatformDevice objects without having to copy all the image
// data.
// -----------------------------------------------------------------------------
class BitmapPlatformDevice::BitmapPlatformDeviceData
    : public base::RefCounted<BitmapPlatformDeviceData> {
 public:
  explicit BitmapPlatformDeviceData(cairo_surface_t* surface);

  cairo_t* GetContext();
  cairo_surface_t* GetSurface();

  // Sets the transform and clip operations. This will not update the Cairo
  // surface, but will mark the config as dirty. The next call of LoadConfig
  // will pick up these changes.
  void SetMatrixClip(const SkMatrix& transform, const SkRegion& region);

 protected:
  void LoadConfig();

  // The Cairo surface inside this DC.
  cairo_t* context_;
  cairo_surface_t *const surface_;

  // True when there is a transform or clip that has not been set to the
  // surface.  The surface is retrieved for every text operation, and the
  // transform and clip do not change as much. We can save time by not loading
  // the clip and transform for every one.
  bool config_dirty_;

  // Translation assigned to the DC: we need to keep track of this separately
  // so it can be updated even if the DC isn't created yet.
  SkMatrix transform_;

  // The current clipping
  SkRegion clip_region_;

  // Disallow copy & assign.
  BitmapPlatformDeviceData(const BitmapPlatformDeviceData&);
  BitmapPlatformDeviceData& operator=(
      const BitmapPlatformDeviceData&);

 private:
  friend class base::RefCounted<BitmapPlatformDeviceData>;

  ~BitmapPlatformDeviceData();
};

BitmapPlatformDevice::BitmapPlatformDeviceData::BitmapPlatformDeviceData(
    cairo_surface_t* surface)
    : surface_(surface),
      config_dirty_(true) {  // Want to load the config next time.
  context_ = cairo_create(surface);
}

BitmapPlatformDevice::BitmapPlatformDeviceData::~BitmapPlatformDeviceData() {
  cairo_destroy(context_);
  cairo_surface_destroy(surface_);
}

cairo_t* BitmapPlatformDevice::BitmapPlatformDeviceData::GetContext() {
  LoadConfig();
  return context_;
}

void BitmapPlatformDevice::BitmapPlatformDeviceData::SetMatrixClip(
    const SkMatrix& transform,
    const SkRegion& region) {
  transform_ = transform;
  clip_region_ = region;
  config_dirty_ = true;
}

cairo_surface_t*
BitmapPlatformDevice::BitmapPlatformDeviceData::GetSurface() {
  // TODO(brettw) this function should be removed.
  LoadConfig();
  return surface_;
}

void BitmapPlatformDevice::BitmapPlatformDeviceData::LoadConfig() {
  if (!config_dirty_ || !context_)
    return;  // Nothing to do.
  config_dirty_ = false;

  // Load the identity matrix since this is what our clip is relative to.
  cairo_matrix_t cairo_matrix;
  cairo_matrix_init_identity(&cairo_matrix);
  cairo_set_matrix(context_, &cairo_matrix);

  LoadClipToContext(context_, clip_region_);
  LoadMatrixToContext(context_, transform_);
}

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   cairo_surface_t* surface) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height,
                   cairo_image_surface_get_stride(surface));
  bitmap.setPixels(cairo_image_surface_get_data(surface));
  bitmap.setIsOpaque(is_opaque);

#ifndef NDEBUG
  if (is_opaque) {
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
  }
#endif

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDevice
      (bitmap, new BitmapPlatformDeviceData(surface));
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque) {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                 width, height);

  return Create(width, height, is_opaque, surface);
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   uint8_t* data) {
  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      data, CAIRO_FORMAT_ARGB32, width, height,
      cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));

  return Create(width, height, is_opaque, surface);
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDevice::BitmapPlatformDevice(
    const SkBitmap& bitmap,
    BitmapPlatformDeviceData* data)
    : PlatformDevice(bitmap),
      data_(data) {
}

BitmapPlatformDevice::BitmapPlatformDevice(
    const BitmapPlatformDevice& other)
    : PlatformDevice(const_cast<BitmapPlatformDevice&>(
                          other).accessBitmap(true)),
      data_(other.data_) {
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
}

cairo_t* BitmapPlatformDevice::beginPlatformPaint() {
  return data_->GetContext();
}

void BitmapPlatformDevice::setMatrixClip(const SkMatrix& transform,
                                         const SkRegion& region) {
  data_->SetMatrixClip(transform, region);
}

BitmapPlatformDevice& BitmapPlatformDevice::operator=(
    const BitmapPlatformDevice& other) {
  data_ = other.data_;
  return *this;
}

}  // namespace skia
