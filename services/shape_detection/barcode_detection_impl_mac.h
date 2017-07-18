// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class CIDetector;

namespace shape_detection {

// The __attribute__ visibility annotation is necessary to work around a clang
// bug: https://bugs.llvm.org/show_bug.cgi?id=33796.
class API_AVAILABLE(macosx(10.10))
    __attribute__((visibility("hidden"))) BarcodeDetectionImplMac
    : public shape_detection::mojom::BarcodeDetection {
 public:
  BarcodeDetectionImplMac();
  ~BarcodeDetectionImplMac() override;

  void Detect(const SkBitmap& bitmap,
              shape_detection::mojom::BarcodeDetection::DetectCallback callback)
      override;

 private:
  base::scoped_nsobject<CIDetector> detector_;

  DISALLOW_COPY_AND_ASSIGN(BarcodeDetectionImplMac);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
