// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_

#import <QuartzCore/QuartzCore.h>

#include "base/mac/scoped_nsobject.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom.h"

namespace shape_detection {

// Takes a ScopedSharedBufferHandle with dimensions and produces a new CIImage
// with the same contents, or a null scoped_nsobject is something goes wrong.
base::scoped_nsobject<CIImage> CreateCIImageFromSharedMemory(
    mojo::ScopedSharedBufferHandle frame_data,
    uint32_t width,
    uint32_t height);

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_
