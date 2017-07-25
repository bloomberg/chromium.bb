// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "media/base/scoped_callback_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/face_detection_provider_impl.h"

namespace shape_detection {

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

void FaceDetectionImplMac::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  DetectCallback scoped_callback = media::ScopedCallbackRunner(
      std::move(callback), std::vector<mojom::FaceDetectionResultPtr>());

  base::scoped_nsobject<CIImage> ci_image = CreateCIImageFromSkBitmap(bitmap);
  if (!ci_image)
    return;

  NSArray* const features = [detector_ featuresInImage:ci_image];
  const int height = bitmap.height();

  std::vector<mojom::FaceDetectionResultPtr> results;
  for (CIFaceFeature* const f in features) {
    // In the default Core Graphics coordinate space, the origin is located
    // in the lower-left corner, and thus |ci_image| is flipped vertically.
    // We need to adjust |y| coordinate of bounding box before sending it.
    gfx::RectF boundingbox(f.bounds.origin.x,
                           height - f.bounds.origin.y - f.bounds.size.height,
                           f.bounds.size.width, f.bounds.size.height);

    auto face = shape_detection::mojom::FaceDetectionResult::New();
    face->bounding_box = std::move(boundingbox);

    if (f.hasLeftEyePosition) {
      auto landmark = shape_detection::mojom::Landmark::New();
      landmark->type = shape_detection::mojom::LandmarkType::EYE;
      landmark->location =
          gfx::PointF(f.leftEyePosition.x, height - f.leftEyePosition.y);
      face->landmarks.push_back(std::move(landmark));
    }
    if (f.hasRightEyePosition) {
      auto landmark = shape_detection::mojom::Landmark::New();
      landmark->type = shape_detection::mojom::LandmarkType::EYE;
      landmark->location =
          gfx::PointF(f.rightEyePosition.x, height - f.rightEyePosition.y);
      face->landmarks.push_back(std::move(landmark));
    }
    if (f.hasMouthPosition) {
      auto landmark = shape_detection::mojom::Landmark::New();
      landmark->type = shape_detection::mojom::LandmarkType::MOUTH;
      landmark->location =
          gfx::PointF(f.mouthPosition.x, height - f.mouthPosition.y);
      face->landmarks.push_back(std::move(landmark));
    }

    results.push_back(std::move(face));
  }
  std::move(scoped_callback).Run(std::move(results));
}

}  // namespace shape_detection
