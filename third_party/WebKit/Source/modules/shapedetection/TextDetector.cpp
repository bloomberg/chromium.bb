// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/TextDetector.h"

#include "core/dom/DOMException.h"
#include "core/frame/LocalFrame.h"
#include "core/geometry/DOMRect.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/workers/WorkerThread.h"
#include "modules/imagecapture/Point2D.h"
#include "modules/shapedetection/DetectedText.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

TextDetector* TextDetector::Create(ExecutionContext* context) {
  return new TextDetector(context);
}

TextDetector::TextDetector(ExecutionContext* context) : ShapeDetector() {
  auto request = mojo::MakeRequest(&text_service_);
  if (context->IsDocument()) {
    LocalFrame* frame = ToDocument(context)->GetFrame();
    if (frame)
      frame->GetInterfaceProvider().GetInterface(std::move(request));
  } else {
    WorkerThread* thread = ToWorkerGlobalScope(context)->GetThread();
    thread->GetInterfaceProvider().GetInterface(std::move(request));
  }

  text_service_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &TextDetector::OnTextServiceConnectionError, WrapWeakPersistent(this))));
}

ScriptPromise TextDetector::DoDetect(ScriptPromiseResolver* resolver,
                                     skia::mojom::blink::BitmapPtr bitmap) {
  ScriptPromise promise = resolver->Promise();
  if (!text_service_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Text detection service unavailable."));
    return promise;
  }
  text_service_requests_.insert(resolver);
  text_service_->Detect(std::move(bitmap),
                        ConvertToBaseCallback(WTF::Bind(
                            &TextDetector::OnDetectText, WrapPersistent(this),
                            WrapPersistent(resolver))));
  return promise;
}

void TextDetector::OnDetectText(
    ScriptPromiseResolver* resolver,
    Vector<shape_detection::mojom::blink::TextDetectionResultPtr>
        text_detection_results) {
  DCHECK(text_service_requests_.Contains(resolver));
  text_service_requests_.erase(resolver);

  HeapVector<Member<DetectedText>> detected_text;
  for (const auto& text : text_detection_results) {
    HeapVector<Point2D> corner_points;
    for (const auto& corner_point : text->corner_points) {
      Point2D point;
      point.setX(corner_point.x);
      point.setY(corner_point.y);
      corner_points.push_back(point);
    }
    detected_text.push_back(DetectedText::Create(
        text->raw_value,
        DOMRect::Create(text->bounding_box.x, text->bounding_box.y,
                        text->bounding_box.width, text->bounding_box.height),
        corner_points));
  }

  resolver->Resolve(detected_text);
}

void TextDetector::OnTextServiceConnectionError() {
  for (const auto& request : text_service_requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Text Detection not implemented."));
  }
  text_service_requests_.clear();
  text_service_.reset();
}

void TextDetector::Trace(blink::Visitor* visitor) {
  ShapeDetector::Trace(visitor);
  visitor->Trace(text_service_requests_);
}

}  // namespace blink
