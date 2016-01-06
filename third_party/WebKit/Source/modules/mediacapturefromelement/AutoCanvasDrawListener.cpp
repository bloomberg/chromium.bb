// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/AutoCanvasDrawListener.h"

namespace blink {

// static
AutoCanvasDrawListener* AutoCanvasDrawListener::create(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
{
    return new AutoCanvasDrawListener(handler);
}

AutoCanvasDrawListener::AutoCanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler>& handler)
    : CanvasDrawListener(handler)
{
}

} // namespace blink
