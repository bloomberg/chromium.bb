// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas/OffscreenCanvasRenderingContext.h"

namespace blink {

OffscreenCanvasRenderingContext::OffscreenCanvasRenderingContext(OffscreenCanvas* canvas)
    : m_offscreenCanvas(canvas)
{
}

OffscreenCanvasRenderingContext::ContextType OffscreenCanvasRenderingContext::contextTypeFromId(const String& id)
{
    if (id == "2d")
        return Context2d;
    if (id == "webgl")
        return ContextWebgl;
    if (id == "webgl2")
        return ContextWebgl2;
    return ContextTypeCount;
}

DEFINE_TRACE(OffscreenCanvasRenderingContext)
{
    visitor->trace(m_offscreenCanvas);
}

} // namespace blink
