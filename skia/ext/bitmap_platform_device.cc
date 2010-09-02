// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device.h"

#include "skia/ext/bitmap_platform_device_data.h"
#include "third_party/skia/include/core/SkUtils.h"

namespace {

// Constrains position and size to fit within available_size. If |size| is -1,
// all the available_size is used. Returns false if the position is out of
// available_size.
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

}  // namespace

namespace skia {

void BitmapPlatformDevice::makeOpaque(int x, int y, int width, int height) {
  const SkBitmap& bitmap = accessBitmap(true);
  SkASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);

  // FIXME(brettw): This is kind of lame, we shouldn't be dealing with
  // transforms at this level. Probably there should be a PlatformCanvas
  // function that does the transform (using the actual transform not just the
  // translation) and calls us with the transformed rect.
  const SkMatrix& matrix = data_->transform();
  int bitmap_start_x = SkScalarRound(matrix.getTranslateX()) + x;
  int bitmap_start_y = SkScalarRound(matrix.getTranslateY()) + y;

  if (Constrain(bitmap.width(), &bitmap_start_x, &width) &&
      Constrain(bitmap.height(), &bitmap_start_y, &height)) {
    SkAutoLockPixels lock(bitmap);
    SkASSERT(bitmap.rowBytes() % sizeof(uint32_t) == 0u);
    size_t row_words = bitmap.rowBytes() / sizeof(uint32_t);
    // Set data to the first pixel to be modified.
    uint32_t* data = bitmap.getAddr32(0, 0) + (bitmap_start_y * row_words) +
                     bitmap_start_x;
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++)
        data[j] |= (0xFF << SK_A32_SHIFT);
      data += row_words;
    }
  }
}

}
