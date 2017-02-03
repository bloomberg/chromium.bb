// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom.h"

@class CIDetector;

namespace shape_detection {

class BarcodeDetectionImplMac
    : public shape_detection::mojom::BarcodeDetection {
 public:
  BarcodeDetectionImplMac();
  ~BarcodeDetectionImplMac() override;

  void Detect(mojo::ScopedSharedBufferHandle frame_data,
              uint32_t width,
              uint32_t height,
              const shape_detection::mojom::BarcodeDetection::DetectCallback&
                  callback) override;

 private:
  base::scoped_nsobject<CIDetector> detector_;

  DISALLOW_COPY_AND_ASSIGN(BarcodeDetectionImplMac);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
