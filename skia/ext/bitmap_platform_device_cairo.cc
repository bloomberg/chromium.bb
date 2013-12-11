// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_cairo.h"
#include "skia/ext/platform_canvas.h"

#if defined(OS_OPENBSD)
#include <cairo.h>
#else
#include <cairo/cairo.h>
#endif

namespace skia {

namespace {

// CairoSurfacePixelRef is an SkPixelRef that is backed by a cairo surface.
class SK_API CairoSurfacePixelRef : public SkPixelRef {
 public:
  // The constructor takes ownership of the passed-in surface.
  explicit CairoSurfacePixelRef(const SkImageInfo& info,
                                cairo_surface_t* surface);
  virtual ~CairoSurfacePixelRef();

  SK_DECLARE_UNFLATTENABLE_OBJECT();

 protected:
  virtual void* onLockPixels(SkColorTable**) SK_OVERRIDE;
  virtual void onUnlockPixels() SK_OVERRIDE;

 private:
  cairo_surface_t* surface_;
};

CairoSurfacePixelRef::CairoSurfacePixelRef(const SkImageInfo& info,
                                           cairo_surface_t* surface)
    : SkPixelRef(info), surface_(surface) {
}

CairoSurfacePixelRef::~CairoSurfacePixelRef() {
  if (surface_)
    cairo_surface_destroy(surface_);
}

void* CairoSurfacePixelRef::onLockPixels(SkColorTable** color_table) {
  *color_table = NULL;
  return cairo_image_surface_get_data(surface_);
}

void CairoSurfacePixelRef::onUnlockPixels() {
  // Nothing to do.
  return;
}

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

void BitmapPlatformDevice::SetMatrixClip(
    const SkMatrix& transform,
    const SkRegion& region) {
  transform_ = transform;
  clip_region_ = region;
  config_dirty_ = true;
}

void BitmapPlatformDevice::LoadConfig() {
  if (!config_dirty_ || !cairo_)
    return;  // Nothing to do.
  config_dirty_ = false;

  // Load the identity matrix since this is what our clip is relative to.
  cairo_matrix_t cairo_matrix;
  cairo_matrix_init_identity(&cairo_matrix);
  cairo_set_matrix(cairo_, &cairo_matrix);

  LoadClipToContext(cairo_, clip_region_);
  LoadMatrixToContext(cairo_, transform_);
}

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   cairo_surface_t* surface) {
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface);
    return NULL;
  }

  SkImageInfo info = {
    width,
    height,
    kPMColor_SkColorType,
    is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType
  };

  SkBitmap bitmap;
  bitmap.setConfig(info, cairo_image_surface_get_stride(surface));
  RefPtr<SkPixelRef> pixel_ref = AdoptRef(new CairoSurfacePixelRef(info,
                                                                   surface));
  bitmap.setPixelRef(pixel_ref.get());

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDevice(bitmap, surface);
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque) {
  // This initializes the bitmap to all zeros.
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                        width, height);

  BitmapPlatformDevice* device = Create(width, height, is_opaque, surface);

#ifndef NDEBUG
  if (device && is_opaque)  // Fill with bright bluish green
    device->eraseColor(SkColorSetARGB(255, 0, 255, 128));
#endif

  return device;
}

BitmapPlatformDevice* BitmapPlatformDevice::CreateAndClear(int width,
                                                           int height,
                                                           bool is_opaque) {
  // The Linux port always constructs initialized bitmaps, so there is no extra
  // work to perform here.
  return Create(width, height, is_opaque);
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
// data. Therefore, we do not transfer ownership to the SkBitmapDevice's bitmap.
BitmapPlatformDevice::BitmapPlatformDevice(
    const SkBitmap& bitmap,
    cairo_surface_t* surface)
    : SkBitmapDevice(bitmap),
      cairo_(cairo_create(surface)),
      config_dirty_(true),
      transform_(SkMatrix::I()) {  // Want to load the config next time.
  SetPlatformDevice(this, this);
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
  cairo_destroy(cairo_);
}

SkBaseDevice* BitmapPlatformDevice::onCreateCompatibleDevice(
    SkBitmap::Config config, int width, int height, bool isOpaque,
    Usage /*usage*/) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return BitmapPlatformDevice::Create(width, height, isOpaque);
}

cairo_t* BitmapPlatformDevice::BeginPlatformPaint() {
  LoadConfig();
  cairo_surface_t* surface = cairo_get_target(cairo_);
  // Tell cairo to flush anything it has pending.
  cairo_surface_flush(surface);
  // Tell Cairo that we (probably) modified (actually, will modify) its pixel
  // buffer directly.
  cairo_surface_mark_dirty(surface);
  return cairo_;
}

void BitmapPlatformDevice::DrawToNativeContext(
    PlatformSurface surface, int x, int y, const PlatformRect* src_rect) {
  // Should never be called on Linux.
  SkASSERT(false);
}

void BitmapPlatformDevice::setMatrixClip(const SkMatrix& transform,
                                         const SkRegion& region,
                                         const SkClipStack&) {
  SetMatrixClip(transform, region);
}

// PlatformCanvas impl

SkCanvas* CreatePlatformCanvas(int width, int height, bool is_opaque,
                               uint8_t* data, OnFailureType failureType) {
  skia::RefPtr<SkBaseDevice> dev = skia::AdoptRef(
      BitmapPlatformDevice::Create(width, height, is_opaque, data));
  return CreateCanvas(dev, failureType);
}

// Port of PlatformBitmap to linux
PlatformBitmap::~PlatformBitmap() {
  cairo_destroy(surface_);
}

bool PlatformBitmap::Allocate(int width, int height, bool is_opaque) {
  SkImageInfo info = {
    width,
    height,
    kPMColor_SkColorType,
    is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType
  };

  // The SkBitmap allocates and owns the bitmap memory; PlatformBitmap owns the
  // cairo drawing context tied to the bitmap. The SkBitmap's pixelRef can
  // outlive the PlatformBitmap if additional copies are made.
  int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
  bitmap_.setConfig(info, stride);

  cairo_surface_t* surf = cairo_image_surface_create(
      CAIRO_FORMAT_ARGB32,
      width,
      height);
  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surf);
    return false;
  }
  RefPtr<SkPixelRef> pixel_ref = AdoptRef(new CairoSurfacePixelRef(info, surf));
  bitmap_.setPixelRef(pixel_ref.get());
  return true;
}

}  // namespace skia
