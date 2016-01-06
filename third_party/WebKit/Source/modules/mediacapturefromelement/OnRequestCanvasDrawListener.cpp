// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/OnRequestCanvasDrawListener.h"

#include "platform/Task.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

OnRequestCanvasDrawListener::OnRequestCanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
    :CanvasDrawListener(handler)
{
    m_requestFrame = false;
}

OnRequestCanvasDrawListener::~OnRequestCanvasDrawListener() {}

// static
OnRequestCanvasDrawListener* OnRequestCanvasDrawListener::create(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
{
    return new OnRequestCanvasDrawListener(handler);
}

bool OnRequestCanvasDrawListener::needsNewFrame() const
{
    return m_requestFrame && CanvasDrawListener::needsNewFrame();
}

void OnRequestCanvasDrawListener::sendNewFrame(const WTF::PassRefPtr<SkImage>& image)
{
    m_requestFrame = false;
    CanvasDrawListener::sendNewFrame(image);
}

void OnRequestCanvasDrawListener::requestFrame()
{
    m_requestFrame = true;
}

} // namespace blink
