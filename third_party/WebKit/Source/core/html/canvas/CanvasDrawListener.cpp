// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasDrawListener.h"

namespace blink {

CanvasDrawListener::~CanvasDrawListener() {}

bool CanvasDrawListener::needsNewFrame() const
{
    return m_handler->needsNewFrame();
}

void CanvasDrawListener::sendNewFrame(const WTF::PassRefPtr<SkImage>& image)
{
    m_handler->sendNewFrame(image.get());
}

void CanvasDrawListener::requestFrame()
{
}

CanvasDrawListener::CanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler> handler)
    : m_handler(handler)
{
}

} // namespace blink
