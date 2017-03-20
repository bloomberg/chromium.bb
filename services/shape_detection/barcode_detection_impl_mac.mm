// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "media/capture/video/scoped_result_callback.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/barcode_detection_impl.h"
#include "services/shape_detection/detection_utils_mac.h"

namespace shape_detection {

namespace {

void RunCallbackWithBarcodes(
    const shape_detection::mojom::BarcodeDetection::DetectCallback& callback,
    std::vector<shape_detection::mojom::BarcodeDetectionResultPtr> results) {
  callback.Run(std::move(results));
}

void RunCallbackWithNoBarcodes(
    const shape_detection::mojom::BarcodeDetection::DetectCallback& callback) {
  callback.Run(
      std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>());
}

}  // anonymous namespace

// static
void BarcodeDetectionImpl::Create(
    shape_detection::mojom::BarcodeDetectionRequest request) {
  // Barcode detection needs at least MAC OS X 10.10.
  if (!base::mac::IsAtLeastOS10_10())
    return;
  mojo::MakeStrongBinding(base::MakeUnique<BarcodeDetectionImplMac>(),
                          std::move(request));
}

BarcodeDetectionImplMac::BarcodeDetectionImplMac() {
  NSDictionary* const options = @{CIDetectorAccuracy : CIDetectorAccuracyHigh};
  detector_.reset([[CIDetector detectorOfType:CIDetectorTypeQRCode
                                      context:nil
                                      options:options] retain]);
}

BarcodeDetectionImplMac::~BarcodeDetectionImplMac() {}

void BarcodeDetectionImplMac::Detect(mojo::ScopedSharedBufferHandle frame_data,
                                     uint32_t width,
                                     uint32_t height,
                                     const DetectCallback& callback) {
  media::ScopedResultCallback<DetectCallback> scoped_callback(
      base::Bind(&RunCallbackWithBarcodes, callback),
      base::Bind(&RunCallbackWithNoBarcodes));

  base::scoped_nsobject<CIImage> ci_image =
      CreateCIImageFromSharedMemory(std::move(frame_data), width, height);
  if (!ci_image)
    return;

  NSArray* const features = [detector_ featuresInImage:ci_image];

  std::vector<mojom::BarcodeDetectionResultPtr> results;
  for (CIQRCodeFeature* const f in features) {
    shape_detection::mojom::BarcodeDetectionResultPtr result =
        shape_detection::mojom::BarcodeDetectionResult::New();
    // In the default Core Graphics coordinate space, the origin is located
    // in the lower-left corner, and thus |ci_image| is flipped vertically.
    // We need to adjust |y| coordinate of bounding box before sending it.
    gfx::RectF boundingbox(f.bounds.origin.x,
                           height - f.bounds.origin.y - f.bounds.size.height,
                           f.bounds.size.width, f.bounds.size.height);
    result->bounding_box = std::move(boundingbox);

    // Enumerate corner points starting from top-left in clockwise fashion:
    // https://wicg.github.io/shape-detection-api/#dom-detectedbarcode-cornerpoints
    result->corner_points.emplace_back(f.topLeft.x, height - f.topLeft.y);
    result->corner_points.emplace_back(f.topRight.x, height - f.topRight.y);
    result->corner_points.emplace_back(f.bottomRight.x,
                                       height - f.bottomRight.y);
    result->corner_points.emplace_back(f.bottomLeft.x, height - f.bottomLeft.y);

    result->raw_value = base::SysNSStringToUTF8(f.messageString);
    results.push_back(std::move(result));
  }
  scoped_callback.Run(std::move(results));
}

}  // namespace shape_detection
