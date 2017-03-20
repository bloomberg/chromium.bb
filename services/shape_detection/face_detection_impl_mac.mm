// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "media/capture/video/scoped_result_callback.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/face_detection_provider_impl.h"

namespace shape_detection {

namespace {

void RunCallbackWithFaces(
    const shape_detection::mojom::FaceDetection::DetectCallback& callback,
    shape_detection::mojom::FaceDetectionResultPtr faces) {
  callback.Run(std::move(faces));
}

void RunCallbackWithNoFaces(
    const shape_detection::mojom::FaceDetection::DetectCallback& callback) {
  callback.Run(shape_detection::mojom::FaceDetectionResult::New());
}

}  // anonymous namespace

void FaceDetectionProviderImpl::CreateFaceDetection(
    shape_detection::mojom::FaceDetectionRequest request,
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  mojo::MakeStrongBinding(
      base::MakeUnique<FaceDetectionImplMac>(std::move(options)),
      std::move(request));
}

FaceDetectionImplMac::FaceDetectionImplMac(
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  NSString* const accuracy =
      options->fast_mode ? CIDetectorAccuracyHigh : CIDetectorAccuracyLow;
  NSDictionary* const detector_options = @{CIDetectorAccuracy : accuracy};
  detector_.reset([[CIDetector detectorOfType:CIDetectorTypeFace
                                      context:nil
                                      options:detector_options] retain]);
}

FaceDetectionImplMac::~FaceDetectionImplMac() {}

void FaceDetectionImplMac::Detect(mojo::ScopedSharedBufferHandle frame_data,
                                  uint32_t width,
                                  uint32_t height,
                                  const DetectCallback& callback) {
  media::ScopedResultCallback<DetectCallback> scoped_callback(
      base::Bind(&RunCallbackWithFaces, callback),
      base::Bind(&RunCallbackWithNoFaces));

  base::scoped_nsobject<CIImage> ci_image =
      CreateCIImageFromSharedMemory(std::move(frame_data), width, height);
  if (!ci_image)
    return;

  NSArray* const features = [detector_ featuresInImage:ci_image];

  shape_detection::mojom::FaceDetectionResultPtr faces =
      shape_detection::mojom::FaceDetectionResult::New();
  for (CIFaceFeature* const f in features) {
    // In the default Core Graphics coordinate space, the origin is located
    // in the lower-left corner, and thus |ci_image| is flipped vertically.
    // We need to adjust |y| coordinate of bounding box before sending it.
    gfx::RectF boundingbox(f.bounds.origin.x,
                           height - f.bounds.origin.y - f.bounds.size.height,
                           f.bounds.size.width, f.bounds.size.height);
    faces->bounding_boxes.push_back(boundingbox);
  }
  scoped_callback.Run(std::move(faces));
}

}  // namespace shape_detection
