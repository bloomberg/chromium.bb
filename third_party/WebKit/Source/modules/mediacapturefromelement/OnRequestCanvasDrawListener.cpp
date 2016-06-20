// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/OnRequestCanvasDrawListener.h"

namespace blink {

OnRequestCanvasDrawListener::OnRequestCanvasDrawListener(PassOwnPtr<WebCanvasCaptureHandler> handler)
    : CanvasDrawListener(std::move(handler))
{
}

OnRequestCanvasDrawListener::~OnRequestCanvasDrawListener() {}

// static
OnRequestCanvasDrawListener* OnRequestCanvasDrawListener::create(PassOwnPtr<WebCanvasCaptureHandler> handler)
{
    return new OnRequestCanvasDrawListener(std::move(handler));
}

void OnRequestCanvasDrawListener::sendNewFrame(const WTF::PassRefPtr<SkImage>& image)
{
    m_frameCaptureRequested = false;
    CanvasDrawListener::sendNewFrame(image);
}

} // namespace blink
