// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "media/capture/video/scoped_result_callback.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/text_detection_impl.h"

namespace shape_detection {

namespace {

void RunCallbackWithResults(
    const mojom::TextDetection::DetectCallback& callback,
    std::vector<mojom::TextDetectionResultPtr> results) {
  callback.Run(std::move(results));
}

void RunCallbackWithNoResults(mojom::TextDetection::DetectCallback callback) {
  callback.Run(std::vector<mojom::TextDetectionResultPtr>());
}

}  // anonymous namespace

// static
void TextDetectionImpl::Create(
    const service_manager::BindSourceInfo& source_info,
    mojom::TextDetectionRequest request) {
  // Text detection needs at least MAC OS X 10.11.
  if (!base::mac::IsAtLeastOS10_11())
    return;
  mojo::MakeStrongBinding(base::MakeUnique<TextDetectionImplMac>(),
                          std::move(request));
}

TextDetectionImplMac::TextDetectionImplMac() {
  NSDictionary* const opts = @{CIDetectorAccuracy : CIDetectorAccuracyHigh};
  detector_.reset([[CIDetector detectorOfType:CIDetectorTypeText
                                      context:nil
                                      options:opts] retain]);
}

TextDetectionImplMac::~TextDetectionImplMac() {}

void TextDetectionImplMac::Detect(const SkBitmap& bitmap,
                                  const DetectCallback& callback) {
  DCHECK(base::mac::IsAtLeastOS10_11());
  media::ScopedResultCallback<DetectCallback> scoped_callback(
      base::Bind(&RunCallbackWithResults, callback),
      base::Bind(&RunCallbackWithNoResults));

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
    results.push_back(std::move(result));
  }
  scoped_callback.Run(std::move(results));
}

}  // namespace shape_detection
