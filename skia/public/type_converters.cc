// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/public/type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

namespace {

SkColorType MojoColorTypeToSk(skia::ColorType type) {
  switch (type) {
    case skia::COLOR_TYPE_UNKNOWN:
      return kUnknown_SkColorType;
    case skia::COLOR_TYPE_ALPHA_8:
      return kAlpha_8_SkColorType;
    case skia::COLOR_TYPE_RGB_565:
      return kRGB_565_SkColorType;
    case skia::COLOR_TYPE_ARGB_4444:
      return kARGB_4444_SkColorType;
    case skia::COLOR_TYPE_RGBA_8888:
      return kRGBA_8888_SkColorType;
    case skia::COLOR_TYPE_BGRA_8888:
      return kBGRA_8888_SkColorType;
    case skia::COLOR_TYPE_INDEX_8:
      return kIndex_8_SkColorType;
    case skia::COLOR_TYPE_GRAY_8:
      return kGray_8_SkColorType;
    default:
      NOTREACHED();
  }
  return kUnknown_SkColorType;
}

SkAlphaType MojoAlphaTypeToSk(skia::AlphaType type) {
  switch (type) {
    case skia::ALPHA_TYPE_UNKNOWN:
      return kUnknown_SkAlphaType;
    case skia::ALPHA_TYPE_OPAQUE:
      return kOpaque_SkAlphaType;
    case skia::ALPHA_TYPE_PREMUL:
      return kPremul_SkAlphaType;
    case skia::ALPHA_TYPE_UNPREMUL:
      return kUnpremul_SkAlphaType;
    default:
      NOTREACHED();
  }
  return kUnknown_SkAlphaType;
}

skia::ColorType SkColorTypeToMojo(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
      return skia::COLOR_TYPE_UNKNOWN;
    case kAlpha_8_SkColorType:
      return skia::COLOR_TYPE_ALPHA_8;
    case kRGB_565_SkColorType:
      return skia::COLOR_TYPE_RGB_565;
    case kARGB_4444_SkColorType:
      return skia::COLOR_TYPE_ARGB_4444;
    case kRGBA_8888_SkColorType:
      return skia::COLOR_TYPE_RGBA_8888;
    case kBGRA_8888_SkColorType:
      return skia::COLOR_TYPE_BGRA_8888;
    case kIndex_8_SkColorType:
      return skia::COLOR_TYPE_INDEX_8;
    case kGray_8_SkColorType:
      return skia::COLOR_TYPE_GRAY_8;
    default:
      NOTREACHED();
  }
  return skia::COLOR_TYPE_UNKNOWN;
}

skia::AlphaType SkAlphaTypeToMojo(SkAlphaType type) {
  switch (type) {
    case kUnknown_SkAlphaType:
      return skia::ALPHA_TYPE_UNKNOWN;
    case kOpaque_SkAlphaType:
      return skia::ALPHA_TYPE_OPAQUE;
    case kPremul_SkAlphaType:
      return skia::ALPHA_TYPE_PREMUL;
    case kUnpremul_SkAlphaType:
      return skia::ALPHA_TYPE_UNPREMUL;
    default:
      NOTREACHED();
  }
  return skia::ALPHA_TYPE_UNKNOWN;
}

}  // namespace

SkBitmap TypeConverter<SkBitmap, skia::BitmapPtr>::Convert(
    const skia::BitmapPtr& image) {
  SkBitmap bitmap;
  if (image.is_null())
    return bitmap;
  if (!bitmap.tryAllocPixels(SkImageInfo::Make(
          image->width, image->height, MojoColorTypeToSk(image->color_type),
          MojoAlphaTypeToSk(image->alpha_type)))) {
    return SkBitmap();
  }
  if (bitmap.getSize() != image->pixel_data.size() || !bitmap.getPixels()) {
    return SkBitmap();
  }

  memcpy(bitmap.getPixels(), &image->pixel_data[0], bitmap.getSize());
  return bitmap;
}

skia::BitmapPtr TypeConverter<skia::BitmapPtr, SkBitmap>::Convert(
    const SkBitmap& bitmap) {
  if (bitmap.isNull())
    return nullptr;

  // NOTE: This code doesn't correctly serialize Index8 bitmaps.
  const SkImageInfo& info = bitmap.info();
  DCHECK_NE(info.colorType(), kIndex_8_SkColorType);
  if (info.colorType() == kIndex_8_SkColorType)
    return nullptr;
  skia::BitmapPtr result = skia::Bitmap::New();
  result->color_type = SkColorTypeToMojo(info.colorType());
  result->alpha_type = SkAlphaTypeToMojo(info.alphaType());
  result->width = info.width();
  result->height = info.height();
  size_t size = bitmap.getSize();
  size_t row_bytes = bitmap.rowBytes();
  result->pixel_data = mojo::Array<uint8_t>::New(size);
  if (!bitmap.readPixels(info, &result->pixel_data[0], row_bytes, 0, 0))
    return nullptr;
  return result;
}

}  // namespace mojo
