// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/OnRequestCanvasDrawListener.h"

namespace blink {

OnRequestCanvasDrawListener::OnRequestCanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
    :CanvasDrawListener(handler)
{
}

OnRequestCanvasDrawListener::~OnRequestCanvasDrawListener() {}

// static
OnRequestCanvasDrawListener* OnRequestCanvasDrawListener::create(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
{
    return new OnRequestCanvasDrawListener(handler);
}

void OnRequestCanvasDrawListener::sendNewFrame(const WTF::PassRefPtr<SkImage>& image)
{
    m_frameCaptureRequested = false;
    CanvasDrawListener::sendNewFrame(image);
}

} // namespace blink
