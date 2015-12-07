// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediacapturefromelement/CanvasCaptureMediaStream.h"

#include "core/html/HTMLCanvasElement.h"
#include "modules/mediacapturefromelement/AutoCanvasDrawListener.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamSource.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {


CanvasCaptureMediaStream* CanvasCaptureMediaStream::create(MediaStreamDescriptor* streamDescriptor, PassRefPtrWillBeRawPtr<HTMLCanvasElement> element)
{
    return new CanvasCaptureMediaStream(streamDescriptor, element);
}

CanvasCaptureMediaStream* CanvasCaptureMediaStream::create(MediaStreamDescriptor* streamDescriptor, PassRefPtrWillBeRawPtr<HTMLCanvasElement> element, const PassOwnPtr<WebCanvasCaptureHandler> handler)
{
    CanvasCaptureMediaStream* stream = new CanvasCaptureMediaStream(streamDescriptor, element);
    stream->initialize(handler);
    return stream;
}

HTMLCanvasElement* CanvasCaptureMediaStream::canvas() const
{
    return m_canvasElement.get();
}

void CanvasCaptureMediaStream::requestFrame()
{
    notImplemented();
    return;
}

DEFINE_TRACE(CanvasCaptureMediaStream)
{
    visitor->trace(m_canvasElement);
    visitor->trace(m_drawListener);
    MediaStream::trace(visitor);
}

CanvasCaptureMediaStream::CanvasCaptureMediaStream(MediaStreamDescriptor* streamDescriptor, PassRefPtrWillBeRawPtr<HTMLCanvasElement> element)
    : MediaStream(element->executionContext(), streamDescriptor)
    , m_canvasElement(element) { }

void CanvasCaptureMediaStream::initialize(const PassOwnPtr<WebCanvasCaptureHandler> handler)
{
    m_drawListener = AutoCanvasDrawListener::create(handler);
    m_canvasElement->addListener(m_drawListener.get());
}

} // namespace blink
