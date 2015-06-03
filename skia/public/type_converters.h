// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_TYPE_CONVERTERS_H_
#define SKIA_PUBLIC_TYPE_CONVERTERS_H_

#include "skia/public/interfaces/bitmap.mojom.h"

class SkBitmap;

namespace mojo {

template <>
struct TypeConverter<SkBitmap, skia::BitmapPtr> {
  static SkBitmap Convert(const skia::BitmapPtr& image);
};

template <>
struct TypeConverter<skia::BitmapPtr, SkBitmap> {
  static skia::BitmapPtr Convert(const SkBitmap& bitmap);
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_TYPE_CONVERTERS_H_
