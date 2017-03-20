// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/TextDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "modules/shapedetection/DetectedText.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

TextDetector* TextDetector::create() {
  return new TextDetector();
}

TextDetector::TextDetector() : ShapeDetector() {
  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&m_textService));
  m_textService.set_connection_error_handler(convertToBaseCallback(WTF::bind(
      &TextDetector::onTextServiceConnectionError, wrapWeakPersistent(this))));
}

ScriptPromise TextDetector::doDetect(
    ScriptPromiseResolver* resolver,
    mojo::ScopedSharedBufferHandle sharedBufferHandle,
    int imageWidth,
    int imageHeight) {
  ScriptPromise promise = resolver->promise();
  if (!m_textService) {
    resolver->reject(DOMException::create(
        NotSupportedError, "Text detection service unavailable."));
    return promise;
  }
  m_textServiceRequests.insert(resolver);
  m_textService->Detect(std::move(sharedBufferHandle), imageWidth, imageHeight,
                        convertToBaseCallback(WTF::bind(
                            &TextDetector::onDetectText, wrapPersistent(this),
                            wrapPersistent(resolver))));
  return promise;
}

void TextDetector::onDetectText(
    ScriptPromiseResolver* resolver,
    Vector<shape_detection::mojom::blink::TextDetectionResultPtr>
        textDetectionResults) {
  DCHECK(m_textServiceRequests.contains(resolver));
  m_textServiceRequests.erase(resolver);

  HeapVector<Member<DetectedText>> detectedText;
  for (const auto& text : textDetectionResults) {
    detectedText.push_back(DetectedText::create(
        text->raw_value,
        DOMRect::create(text->bounding_box->x, text->bounding_box->y,
                        text->bounding_box->width,
                        text->bounding_box->height)));
  }

  resolver->resolve(detectedText);
}

void TextDetector::onTextServiceConnectionError() {
  for (const auto& request : m_textServiceRequests) {
    request->reject(DOMException::create(NotSupportedError,
                                         "Text Detection not implemented."));
  }
  m_textServiceRequests.clear();
  m_textService.reset();
}

DEFINE_TRACE(TextDetector) {
  ShapeDetector::trace(visitor);
  visitor->trace(m_textServiceRequests);
}

}  // namespace blink
