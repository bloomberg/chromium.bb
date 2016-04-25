// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas/OffscreenCanvasModules.h"

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

namespace blink {

OffscreenCanvasRenderingContext2D* OffscreenCanvasModules::getContext(OffscreenCanvas& offscreenCanvas, const String& id, const CanvasContextCreationAttributes& attributes)
{
    CanvasRenderingContext* context = offscreenCanvas.getCanvasRenderingContext(id, attributes);
    if (!context)
        return nullptr;

    return static_cast<OffscreenCanvasRenderingContext2D*>(context);
}

} // namespace blink
