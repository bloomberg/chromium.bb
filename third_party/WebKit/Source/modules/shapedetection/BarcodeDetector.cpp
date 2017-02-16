// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/BarcodeDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "modules/imagecapture/Point2D.h"
#include "modules/shapedetection/DetectedBarcode.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

BarcodeDetector* BarcodeDetector::create() {
  return new BarcodeDetector();
}

BarcodeDetector::BarcodeDetector() : ShapeDetector() {
  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&m_barcodeService));
  m_barcodeService.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&BarcodeDetector::onBarcodeServiceConnectionError,
                wrapWeakPersistent(this))));
}

ScriptPromise BarcodeDetector::doDetect(
    ScriptPromiseResolver* resolver,
    mojo::ScopedSharedBufferHandle sharedBufferHandle,
    int imageWidth,
    int imageHeight) {
  ScriptPromise promise = resolver->promise();
  if (!m_barcodeService) {
    resolver->reject(DOMException::create(
        NotSupportedError, "Barcode detection service unavailable."));
    return promise;
  }
  m_barcodeServiceRequests.insert(resolver);
  m_barcodeService->Detect(
      std::move(sharedBufferHandle), imageWidth, imageHeight,
      convertToBaseCallback(WTF::bind(&BarcodeDetector::onDetectBarcodes,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));
  return promise;
}

void BarcodeDetector::onDetectBarcodes(
    ScriptPromiseResolver* resolver,
    Vector<shape_detection::mojom::blink::BarcodeDetectionResultPtr>
        barcodeDetectionResults) {
  DCHECK(m_barcodeServiceRequests.contains(resolver));
  m_barcodeServiceRequests.erase(resolver);

  HeapVector<Member<DetectedBarcode>> detectedBarcodes;
  for (const auto& barcode : barcodeDetectionResults) {
    HeapVector<Point2D> cornerPoints;
    for (const auto& corner_point : barcode->corner_points) {
      Point2D point;
      point.setX(corner_point->x);
      point.setY(corner_point->y);
      cornerPoints.push_back(point);
    }
    detectedBarcodes.push_back(DetectedBarcode::create(
        barcode->raw_value,
        DOMRect::create(barcode->bounding_box->x, barcode->bounding_box->y,
                        barcode->bounding_box->width,
                        barcode->bounding_box->height),
        cornerPoints));
  }

  resolver->resolve(detectedBarcodes);
}

void BarcodeDetector::onBarcodeServiceConnectionError() {
  for (const auto& request : m_barcodeServiceRequests) {
    request->reject(DOMException::create(NotSupportedError,
                                         "Barcode Detection not implemented."));
  }
  m_barcodeServiceRequests.clear();
  m_barcodeService.reset();
}

DEFINE_TRACE(BarcodeDetector) {
  ShapeDetector::trace(visitor);
  visitor->trace(m_barcodeServiceRequests);
}

}  // namespace blink
