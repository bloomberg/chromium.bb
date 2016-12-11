// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "modules/shapedetection/DetectedFace.h"
#include "public/platform/InterfaceProvider.h"

namespace blink {

FaceDetector* FaceDetector::create(Document& document,
                                   const FaceDetectorOptions& options) {
  return new FaceDetector(*document.frame(), options);
}

FaceDetector::FaceDetector(LocalFrame& frame,
                           const FaceDetectorOptions& options)
    : ShapeDetector(frame),
      m_faceDetectorOptions(mojom::blink::FaceDetectorOptions::New()) {
  frame.interfaceProvider()->getInterface(mojo::GetProxy(&m_faceService));
  m_faceService.set_connection_error_handler(convertToBaseCallback(WTF::bind(
      &FaceDetector::onFaceServiceConnectionError, wrapWeakPersistent(this))));
  m_faceDetectorOptions->max_detected_faces = options.maxDetectedFaces();
  m_faceDetectorOptions->fast_mode = options.fastMode();
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
  m_faceServiceRequests.add(resolver);
  m_faceService->Detect(std::move(sharedBufferHandle), imageWidth, imageHeight,
                        m_faceDetectorOptions.Clone(),
                        convertToBaseCallback(WTF::bind(
                            &FaceDetector::onDetectFaces, wrapPersistent(this),
                            wrapPersistent(resolver))));
  return promise;
}

void FaceDetector::onDetectFaces(
    ScriptPromiseResolver* resolver,
    mojom::blink::FaceDetectionResultPtr faceDetectionResult) {
  DCHECK(m_faceServiceRequests.contains(resolver));
  m_faceServiceRequests.remove(resolver);

  HeapVector<Member<DetectedFace>> detectedFaces;
  for (const auto& boundingBox : faceDetectionResult->bounding_boxes) {
    detectedFaces.append(DetectedFace::create(
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
