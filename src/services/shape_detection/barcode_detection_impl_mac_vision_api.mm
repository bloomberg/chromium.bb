// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac_vision_api.h"

#include "base/logging.h"

namespace shape_detection {

namespace {

class VisionAPI : public VisionAPIInterface {
 public:
  VisionAPI() = default;

  ~VisionAPI() override = default;

  NSArray<VNBarcodeSymbology>* GetSupportedSymbologies() const override {
    if (@available(macOS 10.13, *)) {
      return [VNDetectBarcodesRequest supportedSymbologies];
    }

    return @[];
  }
};

}  // namespace

// static
std::unique_ptr<VisionAPIInterface> VisionAPIInterface::Create() {
  return std::make_unique<VisionAPI>();
}

}  // namespace shape_detection
