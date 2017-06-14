// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "core/dom/DOMException.h"
#include "core/geometry/DOMRect.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "modules/imagecapture/Point2D.h"
#include "modules/shapedetection/DetectedFace.h"
#include "modules/shapedetection/FaceDetectorOptions.h"
#include "modules/shapedetection/Landmark.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom-blink.h"

namespace blink {

FaceDetector* FaceDetector::Create(const FaceDetectorOptions& options) {
  return new FaceDetector(options);
}

FaceDetector::FaceDetector(const FaceDetectorOptions& options)
    : ShapeDetector() {
  shape_detection::mojom::blink::FaceDetectorOptionsPtr face_detector_options =
      shape_detection::mojom::blink::FaceDetectorOptions::New();
  face_detector_options->max_detected_faces = options.maxDetectedFaces();
  face_detector_options->fast_mode = options.fastMode();
  shape_detection::mojom::blink::FaceDetectionProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));
  provider->CreateFaceDetection(mojo::MakeRequest(&face_service_),
                                std::move(face_detector_options));

  face_service_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &FaceDetector::OnFaceServiceConnectionError, WrapWeakPersistent(this))));
}

ScriptPromise FaceDetector::DoDetect(ScriptPromiseResolver* resolver,
                                     skia::mojom::blink::BitmapPtr bitmap) {
  ScriptPromise promise = resolver->Promise();
  if (!face_service_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Face detection service unavailable."));
    return promise;
  }
  face_service_requests_.insert(resolver);
  face_service_->Detect(std::move(bitmap),
                        ConvertToBaseCallback(WTF::Bind(
                            &FaceDetector::OnDetectFaces, WrapPersistent(this),
                            WrapPersistent(resolver))));
  return promise;
}

void FaceDetector::OnDetectFaces(
    ScriptPromiseResolver* resolver,
    Vector<shape_detection::mojom::blink::FaceDetectionResultPtr>
        face_detection_results) {
  DCHECK(face_service_requests_.Contains(resolver));
  face_service_requests_.erase(resolver);

  HeapVector<Member<DetectedFace>> detected_faces;
  for (const auto& face : face_detection_results) {
    HeapVector<Landmark> landmarks;
    for (const auto& landmark : face->landmarks) {
      Point2D location;
      location.setX(landmark->location.x);
      location.setY(landmark->location.y);
      Landmark web_landmark;
      web_landmark.setLocation(location);
      if (landmark->type == shape_detection::mojom::blink::LandmarkType::EYE) {
        web_landmark.setType("eye");
      } else if (landmark->type ==
                 shape_detection::mojom::blink::LandmarkType::MOUTH) {
        web_landmark.setType("mouth");
      }
      landmarks.push_back(web_landmark);
    }

    detected_faces.push_back(DetectedFace::Create(
        DOMRect::Create(face->bounding_box.x, face->bounding_box.y,
                        face->bounding_box.width, face->bounding_box.height),
        landmarks));
  }

  resolver->Resolve(detected_faces);
}

void FaceDetector::OnFaceServiceConnectionError() {
  for (const auto& request : face_service_requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Face Detection not implemented."));
  }
  face_service_requests_.clear();
  face_service_.reset();
}

DEFINE_TRACE(FaceDetector) {
  ShapeDetector::Trace(visitor);
  visitor->Trace(face_service_requests_);
}

}  // namespace blink
