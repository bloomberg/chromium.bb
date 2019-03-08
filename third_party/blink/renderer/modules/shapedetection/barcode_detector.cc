// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/barcode_detector.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"
#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"

namespace blink {

namespace {

WebString BarcodeFormatToString(
    const shape_detection::mojom::blink::BarcodeFormat format) {
  switch (format) {
    case shape_detection::mojom::blink::BarcodeFormat::AZTEC:
      return WebString::FromUTF8("aztec");
    case shape_detection::mojom::blink::BarcodeFormat::CODE_128:
      return WebString::FromUTF8("code_128");
    case shape_detection::mojom::blink::BarcodeFormat::CODE_39:
      return WebString::FromUTF8("code_39");
    case shape_detection::mojom::blink::BarcodeFormat::CODE_93:
      return WebString::FromUTF8("code_93");
    case shape_detection::mojom::blink::BarcodeFormat::CODABAR:
      return WebString::FromUTF8("codabar");
    case shape_detection::mojom::blink::BarcodeFormat::DATA_MATRIX:
      return WebString::FromUTF8("data_matrix");
    case shape_detection::mojom::blink::BarcodeFormat::EAN_13:
      return WebString::FromUTF8("ean_13");
    case shape_detection::mojom::blink::BarcodeFormat::EAN_8:
      return WebString::FromUTF8("ean_8");
    case shape_detection::mojom::blink::BarcodeFormat::ITF:
      return WebString::FromUTF8("itf");
    case shape_detection::mojom::blink::BarcodeFormat::PDF417:
      return WebString::FromUTF8("pdf417");
    case shape_detection::mojom::blink::BarcodeFormat::QR_CODE:
      return WebString::FromUTF8("qr_code");
    case shape_detection::mojom::blink::BarcodeFormat::UNKNOWN:
      return WebString::FromUTF8("unknown");
    case shape_detection::mojom::blink::BarcodeFormat::UPC_A:
      return WebString::FromUTF8("upc_a");
    case shape_detection::mojom::blink::BarcodeFormat::UPC_E:
      return WebString::FromUTF8("upc_e");
    default:
      NOTREACHED() << "Invalid BarcodeFormat";
  }
  return WebString();
}

}  // namespace

BarcodeDetector* BarcodeDetector::Create(ExecutionContext* context) {
  return MakeGarbageCollected<BarcodeDetector>(context);
}

BarcodeDetector::BarcodeDetector(ExecutionContext* context) : ShapeDetector() {
  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  auto request = mojo::MakeRequest(&barcode_provider_, task_runner);
  if (auto* interface_provider = context->GetInterfaceProvider()) {
    interface_provider->GetInterface(std::move(request));
  }
  barcode_provider_->CreateBarcodeDetection(
      mojo::MakeRequest(&barcode_service_, task_runner),
      shape_detection::mojom::blink::BarcodeDetectorOptions::New());

  barcode_service_.set_connection_error_handler(
      WTF::Bind(&BarcodeDetector::OnBarcodeServiceConnectionError,
                WrapWeakPersistent(this)));
}

ScriptPromise BarcodeDetector::getSupportedFormats(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  BarcodeDetector* detector = BarcodeDetector::Create(context);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!detector->barcode_service_) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kNotSupportedError,
                             "Barcode detection service unavailable."));
    return promise;
  }

  detector->barcode_service_requests_.insert(resolver);
  detector->barcode_provider_->EnumerateSupportedFormats(
      WTF::Bind(&BarcodeDetector::OnEnumerateSupportedFormats,
                WrapPersistent(detector), WrapPersistent(resolver)));
  return promise;
}

void BarcodeDetector::OnEnumerateSupportedFormats(
    ScriptPromiseResolver* resolver,
    const Vector<shape_detection::mojom::blink::BarcodeFormat>&
        format_results) {
  DCHECK(barcode_service_requests_.Contains(resolver));
  barcode_service_requests_.erase(resolver);

  Vector<WTF::String> formats;
  formats.ReserveInitialCapacity(format_results.size());
  for (const auto& format : format_results)
    formats.push_back(BarcodeFormatToString(format));
  resolver->Resolve(formats);
}

ScriptPromise BarcodeDetector::DoDetect(ScriptPromiseResolver* resolver,
                                        SkBitmap bitmap) {
  ScriptPromise promise = resolver->Promise();
  if (!barcode_service_) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kNotSupportedError,
                             "Barcode detection service unavailable."));
    return promise;
  }
  barcode_service_requests_.insert(resolver);
  barcode_service_->Detect(
      std::move(bitmap),
      WTF::Bind(&BarcodeDetector::OnDetectBarcodes, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void BarcodeDetector::OnDetectBarcodes(
    ScriptPromiseResolver* resolver,
    Vector<shape_detection::mojom::blink::BarcodeDetectionResultPtr>
        barcode_detection_results) {
  DCHECK(barcode_service_requests_.Contains(resolver));
  barcode_service_requests_.erase(resolver);

  HeapVector<Member<DetectedBarcode>> detected_barcodes;
  for (const auto& barcode : barcode_detection_results) {
    HeapVector<Member<Point2D>> corner_points;
    for (const auto& corner_point : barcode->corner_points) {
      Point2D* point = Point2D::Create();
      point->setX(corner_point.x);
      point->setY(corner_point.y);
      corner_points.push_back(point);
    }
    detected_barcodes.push_back(DetectedBarcode::Create(
        barcode->raw_value,
        DOMRectReadOnly::Create(
            barcode->bounding_box.x, barcode->bounding_box.y,
            barcode->bounding_box.width, barcode->bounding_box.height),
        corner_points));
  }

  resolver->Resolve(detected_barcodes);
}

void BarcodeDetector::OnBarcodeServiceConnectionError() {
  for (const auto& request : barcode_service_requests_) {
    request->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                         "Barcode Detection not implemented."));
  }
  barcode_service_requests_.clear();
  barcode_service_.reset();
  barcode_provider_.reset();
}

void BarcodeDetector::Trace(blink::Visitor* visitor) {
  ShapeDetector::Trace(visitor);
  visitor->Trace(barcode_service_requests_);
}

}  // namespace blink
