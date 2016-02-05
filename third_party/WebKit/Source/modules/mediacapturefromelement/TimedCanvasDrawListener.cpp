// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/TimedCanvasDrawListener.h"

#include "platform/Task.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

TimedCanvasDrawListener::TimedCanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler>& handler, double frameRate)
    : CanvasDrawListener(handler)
{
    m_frameInterval = 1000 / frameRate;
}

TimedCanvasDrawListener::~TimedCanvasDrawListener() {}

// static
TimedCanvasDrawListener* TimedCanvasDrawListener::create(const PassOwnPtr<WebCanvasCaptureHandler>& handler, double frameRate)
{
    return new TimedCanvasDrawListener(handler, frameRate);
}

bool TimedCanvasDrawListener::needsNewFrame() const
{
    return m_requestFrame && CanvasDrawListener::needsNewFrame();
}

void TimedCanvasDrawListener::sendNewFrame(const WTF::PassRefPtr<SkImage>& image)
{
    m_requestFrame = false;
    CanvasDrawListener::sendNewFrame(image);
}

void TimedCanvasDrawListener::requestNewFrame()
{
    m_requestFrame = true;
    Platform::current()->currentThread()->taskRunner()->postDelayedTask(BLINK_FROM_HERE, new Task(bind(&TimedCanvasDrawListener::requestNewFrame, this)), m_frameInterval);
}

} // namespace blink
