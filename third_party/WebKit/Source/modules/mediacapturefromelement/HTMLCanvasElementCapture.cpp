// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediacapturefromelement/HTMLCanvasElementCapture.h"

#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLCanvasElement.h"
#include "modules/mediacapturefromelement/CanvasCaptureMediaStream.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace {
const double kDefaultFrameRate = 60.0;
} // anonymous namespace

namespace blink {

CanvasCaptureMediaStream* HTMLCanvasElementCapture::captureStream(HTMLCanvasElement& element, ExceptionState& exceptionState)
{
    WebMediaStream stream;
    stream.initialize(WebVector<WebMediaStreamTrack>(), Vector<WebMediaStreamTrack>());
    WebSize size(element.width(), element.height());

    if (!element.originClean()) {
        exceptionState.throwDOMException(SecurityError, "Canvas is not origin-clean.");
        return CanvasCaptureMediaStream::create(stream, &element);
    }

    OwnPtr<WebCanvasCaptureHandler> handler = adoptPtr(Platform::current()->createCanvasCaptureHandler(size, kDefaultFrameRate, &stream));
    ASSERT(handler);
    if (!handler) {
        exceptionState.throwDOMException(NotSupportedError, "No CanvasCapture handler can be created.");
        return CanvasCaptureMediaStream::create(stream, &element);
    }

    return CanvasCaptureMediaStream::create(stream, &element, handler.release());
}

} // namespace blink
