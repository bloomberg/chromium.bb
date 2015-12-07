// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediacapturefromelement/AutoCanvasDrawListener.h"

#include "public/platform/WebSkImage.h"

namespace blink {

// static
AutoCanvasDrawListener* AutoCanvasDrawListener::create(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
{
    return new AutoCanvasDrawListener(handler);
}

bool AutoCanvasDrawListener::needsNewFrame() const
{
    return m_handler->needsNewFrame();
}

void AutoCanvasDrawListener::sendNewFrame(const WTF::PassRefPtr<SkImage>& image)
{
    m_handler->sendNewFrame(WebSkImage(image));
}

AutoCanvasDrawListener::AutoCanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
{
    m_handler = handler;
}

} // namespace blink
