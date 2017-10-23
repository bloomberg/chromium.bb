// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/scoped_callback_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/text_detection_impl.h"

namespace shape_detection {

// static
void TextDetectionImpl::Create(mojom::TextDetectionRequest request) {
  // Text detection needs at least MAC OS X 10.11.
  if (@available(macOS 10.11, *)) {
    mojo::MakeStrongBinding(base::MakeUnique<TextDetectionImplMac>(),
                            std::move(request));
  }
}

TextDetectionImplMac::TextDetectionImplMac() {
  NSDictionary* const opts = @{CIDetectorAccuracy : CIDetectorAccuracyHigh};
  detector_.reset(
      [[CIDetector detectorOfType:CIDetectorTypeText context:nil options:opts]
          retain]);
}

TextDetectionImplMac::~TextDetectionImplMac() {}

void TextDetectionImplMac::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  DCHECK(base::mac::IsAtLeastOS10_11());
  DetectCallback scoped_callback = media::ScopedCallbackRunner(
      std::move(callback), std::vector<mojom::TextDetectionResultPtr>());

  base::scoped_nsobject<CIImage> ci_image = CreateCIImageFromSkBitmap(bitmap);
  if (!ci_image)
    return;

  NSArray* const features = [detector_ featuresInImage:ci_image];

  const int height = bitmap.height();
  std::vector<mojom::TextDetectionResultPtr> results;
  for (CIRectangleFeature* const f in features) {
    // CIRectangleFeature only has bounding box information.
    auto result = mojom::TextDetectionResult::New();
    // In the default Core Graphics coordinate space, the origin is located
    // in the lower-left corner, and thus |ci_image| is flipped vertically.
    // We need to adjust |y| coordinate of bounding box before sending it.
    gfx::RectF boundingbox(f.bounds.origin.x,
                           height - f.bounds.origin.y - f.bounds.size.height,
                           f.bounds.size.width, f.bounds.size.height);
    result->bounding_box = std::move(boundingbox);

    // Enumerate corner points starting from top-left in clockwise fashion:
    // https://wicg.github.io/shape-detection-api/text.html#dom-detectedtext-cornerpoints
    result->corner_points.emplace_back(f.topLeft.x, height - f.topLeft.y);
    result->corner_points.emplace_back(f.topRight.x, height - f.topRight.y);
    result->corner_points.emplace_back(f.bottomRight.x,
                                       height - f.bottomRight.y);
    result->corner_points.emplace_back(f.bottomLeft.x, height - f.bottomLeft.y);

    results.push_back(std::move(result));
  }
  std::move(scoped_callback).Run(std::move(results));
}

}  // namespace shape_detection
