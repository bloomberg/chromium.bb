// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac_vision.h"

#include <Foundation/Foundation.h>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shape_detection {

namespace {

mojom::BarcodeFormat ToBarcodeFormat(NSString* symbology) {
  if ([symbology isEqual:@"VNBarcodeSymbologyAztec"])
    return mojom::BarcodeFormat::AZTEC;
  if ([symbology isEqual:@"VNBarcodeSymbologyCode128"])
    return mojom::BarcodeFormat::CODE_128;
  if ([symbology isEqual:@"VNBarcodeSymbologyCode39"])
    return mojom::BarcodeFormat::CODE_39;
  if ([symbology isEqual:@"VNBarcodeSymbologyCode93"])
    return mojom::BarcodeFormat::CODE_93;
  if ([symbology isEqual:@"VNBarcodeSymbologyDataMatrix"])
    return mojom::BarcodeFormat::DATA_MATRIX;
  if ([symbology isEqual:@"VNBarcodeSymbologyEAN13"])
    return mojom::BarcodeFormat::EAN_13;
  if ([symbology isEqual:@"VNBarcodeSymbologyEAN8"])
    return mojom::BarcodeFormat::EAN_8;
  if ([symbology isEqual:@"VNBarcodeSymbologyITF14"])
    return mojom::BarcodeFormat::ITF;
  if ([symbology isEqual:@"VNBarcodeSymbologyPDF417"])
    return mojom::BarcodeFormat::PDF417;
  if ([symbology isEqual:@"VNBarcodeSymbologyQR"])
    return mojom::BarcodeFormat::QR_CODE;
  if ([symbology isEqual:@"VNBarcodeSymbologyUPCE"])
    return mojom::BarcodeFormat::UPC_E;
  return mojom::BarcodeFormat::UNKNOWN;
}

}  // unnamed namespace

BarcodeDetectionImplMacVision::BarcodeDetectionImplMacVision(
    mojom::BarcodeDetectorOptionsPtr options)
    : weak_factory_(this) {
  Class request_class = NSClassFromString(@"VNDetectBarcodesRequest");
  if (!request_class) {
    DLOG(ERROR) << "Failed to load VNDetectBarcodesRequest class";
    return;
  }

  // The repeating callback will not be run if BarcodeDetectionImplMacVision
  // object has already been destroyed.
  barcodes_async_request_ = VisionAPIAsyncRequestMac::Create(
      request_class,
      base::BindRepeating(&BarcodeDetectionImplMacVision::OnBarcodesDetected,
                          weak_factory_.GetWeakPtr()));
}

BarcodeDetectionImplMacVision::~BarcodeDetectionImplMacVision() = default;

void BarcodeDetectionImplMacVision::Detect(const SkBitmap& bitmap,
                                           DetectCallback callback) {
  DCHECK(barcodes_async_request_);

  if (!barcodes_async_request_->PerformRequest(bitmap)) {
    std::move(callback).Run({});
    return;
  }

  image_size_ = CGSizeMake(bitmap.width(), bitmap.height());
  // Hold on the callback until async request completes.
  detected_callback_ = std::move(callback);
  // This prevents the Detect function from being called before the
  // VisionAPIAsyncRequestMac completes.
  if (binding_)  // Can be unbound in unit testing.
    binding_->PauseIncomingMethodCallProcessing();
}

void BarcodeDetectionImplMacVision::OnBarcodesDetected(VNRequest* request,
                                                       NSError* error) {
  if (binding_)  // Can be unbound in unit testing.
    binding_->ResumeIncomingMethodCallProcessing();

  if ([request.results count] == 0 || error) {
    std::move(detected_callback_).Run({});
    return;
  }

  std::vector<mojom::BarcodeDetectionResultPtr> results;
  for (VNBarcodeObservation* const observation in request.results) {
    auto barcode = mojom::BarcodeDetectionResult::New();
    // The coordinates are normalized to the dimensions of the processed image.
    barcode->bounding_box = ConvertCGToGfxCoordinates(
        CGRectMake(observation.boundingBox.origin.x * image_size_.width,
                   observation.boundingBox.origin.y * image_size_.height,
                   observation.boundingBox.size.width * image_size_.width,
                   observation.boundingBox.size.height * image_size_.height),
        image_size_.height);

    // Enumerate corner points starting from top-left in clockwise fashion:
    // https://wicg.github.io/shape-detection-api/#dom-detectedbarcode-cornerpoints
    barcode->corner_points.emplace_back(
        observation.topLeft.x * image_size_.width,
        (1 - observation.topLeft.y) * image_size_.height);
    barcode->corner_points.emplace_back(
        observation.topRight.x * image_size_.width,
        (1 - observation.topRight.y) * image_size_.height);
    barcode->corner_points.emplace_back(
        observation.bottomRight.x * image_size_.width,
        (1 - observation.bottomRight.y) * image_size_.height);
    barcode->corner_points.emplace_back(
        observation.bottomLeft.x * image_size_.width,
        (1 - observation.bottomLeft.y) * image_size_.height);

    barcode->raw_value =
        base::SysNSStringToUTF8(observation.payloadStringValue);

    barcode->format = ToBarcodeFormat(observation.symbology);

    results.push_back(std::move(barcode));
  }
  std::move(detected_callback_).Run(std::move(results));
}

}  // namespace shape_detection
