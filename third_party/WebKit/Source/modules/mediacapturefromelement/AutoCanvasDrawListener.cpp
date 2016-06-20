// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/AutoCanvasDrawListener.h"

namespace blink {

AutoCanvasDrawListener::AutoCanvasDrawListener(PassOwnPtr<WebCanvasCaptureHandler> handler)
    : CanvasDrawListener(std::move(handler))
{
}

// static
AutoCanvasDrawListener* AutoCanvasDrawListener::create(PassOwnPtr<WebCanvasCaptureHandler> handler)
{
    return new AutoCanvasDrawListener(std::move(handler));
}

} // namespace blink
