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
  // Vision Framework needs at least MAC OS X 10.13. Stay vigilant: past OS
  // upgrades (e.g. to 10.14, see crbug.com/921968) have broken it.
  if (@available(macOS 10.13, *)) {
    auto impl =
        std::make_unique<BarcodeDetectionImplMacVision>(std::move(options));
    auto* impl_ptr = impl.get();
    impl_ptr->SetBinding(
        mojo::MakeStrongBinding(std::move(impl), std::move(request)));
    return;
  }

  // CIDetector barcode detection needs at least MAC OS X 10.10.
  if (@available(macOS 10.10, *)) {
    mojo::MakeStrongBinding(std::make_unique<BarcodeDetectionImplMac>(),
                            std::move(request));
  }
}

void BarcodeDetectionProviderMac::EnumerateSupportedFormats(
    EnumerateSupportedFormatsCallback callback) {
  // Vision Framework needs at least MAC OS X 10.13.
  if (@available(macOS 10.13, *)) {
    // Vision recognizes more barcode symbologies than Core Image Framework.
    std::move(callback).Run(
        {mojom::BarcodeFormat::AZTEC, mojom::BarcodeFormat::CODE_128,
         mojom::BarcodeFormat::CODE_39, mojom::BarcodeFormat::CODE_93,
         mojom::BarcodeFormat::DATA_MATRIX, mojom::BarcodeFormat::EAN_13,
         mojom::BarcodeFormat::EAN_8, mojom::BarcodeFormat::ITF,
         mojom::BarcodeFormat::PDF417, mojom::BarcodeFormat::QR_CODE,
         mojom::BarcodeFormat::UPC_E});
    return;
  }

  // Barcode detection needs at least MAC OS X 10.10.
  if (@available(macOS 10.10, *)) {
    // Mac implementation supports only one BarcodeFormat.
    std::move(callback).Run({mojom::BarcodeFormat::QR_CODE});
    return;
  }

  DLOG(ERROR) << "Platform not supported for Barcode Detection.";
  std::move(callback).Run({});
}

}  // namespace shape_detection
