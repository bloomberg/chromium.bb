// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_provider_mac.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/barcode_detection_impl_mac.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision.h"

namespace shape_detection {

BarcodeDetectionProviderMac::BarcodeDetectionProviderMac() = default;
BarcodeDetectionProviderMac::BarcodeDetectionProviderMac(
    std::unique_ptr<VisionAPIInterface> vision_api)
    : vision_api_(std::move(vision_api)) {}

BarcodeDetectionProviderMac::~BarcodeDetectionProviderMac() = default;

// static
void BarcodeDetectionProviderMac::Create(
    mojom::BarcodeDetectionProviderRequest request) {
  mojo::MakeStrongBinding(std::make_unique<BarcodeDetectionProviderMac>(),
                          std::move(request));
}

void BarcodeDetectionProviderMac::CreateBarcodeDetection(
    mojom::BarcodeDetectionRequest request,
    mojom::BarcodeDetectorOptionsPtr options) {
  if (!vision_api_)
    vision_api_ = VisionAPIInterface::Create();

  // Vision Framework needs at least MAC OS X 10.13.
  if (@available(macOS 10.13, *)) {
    if (!BarcodeDetectionImplMacVision::IsBlockedMacOSVersion()) {
      auto impl =
          std::make_unique<BarcodeDetectionImplMacVision>(std::move(options));
      auto* impl_ptr = impl.get();
      impl_ptr->SetBinding(
          mojo::MakeStrongBinding(std::move(impl), std::move(request)));
      return;
    }
  }

  mojo::MakeStrongBinding(std::make_unique<BarcodeDetectionImplMac>(),
                          std::move(request));
}

void BarcodeDetectionProviderMac::EnumerateSupportedFormats(
    EnumerateSupportedFormatsCallback callback) {
  // If we have supported formats already cached, return them.
  if (supported_formats_) {
    DLOG_IF(WARNING, supported_formats_->empty())
        << "Supported formats requested previously but error or none "
        << "recognized.";

    std::move(callback).Run(supported_formats_.value());
    return;
  }

  if (!vision_api_)
    vision_api_ = VisionAPIInterface::Create();

  // Vision Framework needs at least MAC OS X 10.13.
  if (@available(macOS 10.13, *)) {
    // Vision recognizes more barcode symbologies than Core Image Framework.
    supported_formats_ = BarcodeDetectionImplMacVision::GetSupportedSymbologies(
        vision_api_.get());
    std::move(callback).Run(supported_formats_.value());
    return;
  }

  supported_formats_ = std::vector<mojom::BarcodeFormat>(
      BarcodeDetectionImplMac::GetSupportedSymbologies());
  std::move(callback).Run(supported_formats_.value());
}

}  // namespace shape_detection
