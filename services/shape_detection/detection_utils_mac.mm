// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/detection_utils_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/numerics/checked_math.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace shape_detection {

base::scoped_nsobject<CIImage> CreateCIImageFromSkBitmap(
    const SkBitmap& bitmap) {
  base::CheckedNumeric<uint32_t> num_pixels =
      base::CheckedNumeric<uint32_t>(bitmap.width()) * bitmap.height();
  base::CheckedNumeric<uint32_t> num_bytes = num_pixels * 4;
  if (!num_bytes.IsValid()) {
    DLOG(ERROR) << "Data overflow";
    return base::scoped_nsobject<CIImage>();
  }

  // First convert SkBitmap to CGImageRef.
  base::ScopedCFTypeRef<CGImageRef> cg_image(
      SkCreateCGImageRefWithColorspace(bitmap, NULL));
  if (!cg_image) {
    DLOG(ERROR) << "Failed to create CGImageRef";
    return base::scoped_nsobject<CIImage>();
  }

  base::scoped_nsobject<CIImage> ci_image(
      [[CIImage alloc] initWithCGImage:cg_image]);
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create CIImage";
    return base::scoped_nsobject<CIImage>();
  }
  return ci_image;
}

}  // namespace shape_detection
