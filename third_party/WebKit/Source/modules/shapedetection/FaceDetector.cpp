// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "modules/shapedetection/DetectedFace.h"
#include "modules/shapedetection/FaceDetectorOptions.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom-blink.h"

namespace blink {

FaceDetector* FaceDetector::create(const FaceDetectorOptions& options) {
  return new FaceDetector(options);
}

FaceDetector::FaceDetector(const FaceDetectorOptions& options)
    : ShapeDetector() {
  shape_detection::mojom::blink::FaceDetectorOptionsPtr faceDetectorOptions =
      shape_detection::mojom::blink::FaceDetectorOptions::New();
  faceDetectorOptions->max_detected_faces = options.maxDetectedFaces();
  faceDetectorOptions->fast_mode = options.fastMode();
  shape_detection::mojom::blink::FaceDetectionProviderPtr provider;
  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&provider));
  provider->CreateFaceDetection(mojo::MakeRequest(&m_faceService),
                                std::move(faceDetectorOptions));

  m_faceService.set_connection_error_handler(convertToBaseCallback(WTF::bind(
      &FaceDetector::onFaceServiceConnectionError, wrapWeakPersistent(this))));
}

ScriptPromise FaceDetector::doDetect(
    ScriptPromiseResolver* resolver,
    mojo::ScopedSharedBufferHandle sharedBufferHandle,
    int imageWidth,
    int imageHeight) {
  ScriptPromise promise = resolver->promise();
  if (!m_faceService) {
    resolver->reject(DOMException::create(
        NotSupportedError, "Face detection service unavailable."));
    return promise;
  }
  m_faceServiceRequests.insert(resolver);
  m_faceService->Detect(std::move(sharedBufferHandle), imageWidth, imageHeight,
                        convertToBaseCallback(WTF::bind(
                            &FaceDetector::onDetectFaces, wrapPersistent(this),
                            wrapPersistent(resolver))));
  return promise;
}

void FaceDetector::onDetectFaces(
    ScriptPromiseResolver* resolver,
    shape_detection::mojom::blink::FaceDetectionResultPtr faceDetectionResult) {
  DCHECK(m_faceServiceRequests.contains(resolver));
  m_faceServiceRequests.erase(resolver);

  HeapVector<Member<DetectedFace>> detectedFaces;
  for (const auto& boundingBox : faceDetectionResult->bounding_boxes) {
    detectedFaces.push_back(DetectedFace::create(
        DOMRect::create(boundingBox->x, boundingBox->y, boundingBox->width,
                        boundingBox->height)));
  }

  resolver->resolve(detectedFaces);
}

void FaceDetector::onFaceServiceConnectionError() {
  for (const auto& request : m_faceServiceRequests) {
    request->reject(DOMException::create(NotSupportedError,
                                         "Face Detection not implemented."));
  }
  m_faceServiceRequests.clear();
  m_faceService.reset();
}

DEFINE_TRACE(FaceDetector) {
  ShapeDetector::trace(visitor);
  visitor->trace(m_faceServiceRequests);
}

}  // namespace blink
