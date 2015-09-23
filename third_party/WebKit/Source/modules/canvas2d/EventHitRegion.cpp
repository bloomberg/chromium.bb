// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/canvas2d/EventHitRegion.h"

#include "core/dom/Document.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/layout/LayoutObject.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/canvas2d/HitRegion.h"

namespace blink {

String EventHitRegion::regionIdFromAbsoluteLocation(HTMLCanvasElement& canvas, const LayoutPoint& location)
{
    CanvasRenderingContext* context = canvas.renderingContext();
    if (!context || !context->is2d())
        return String();

    Document& document = canvas.document();
    document.updateLayoutTreeForNodeIfNeeded(&canvas);

    // Adjust offsetLocation to be relative to the canvas's position.
    LayoutObject* layoutObject = canvas.layoutObject();
    FloatPoint localPos = layoutObject->absoluteToLocal(FloatPoint(location), UseTransforms);

    LocalFrame* frame = document.frame();
    float zoomFactor = frame ? frame->pageZoomFactor() : 1;
    float scaleFactor = 1 / zoomFactor;
    if (scaleFactor != 1.0f)
        localPos.scale(scaleFactor, scaleFactor);

    HitRegion* hitRegion = toCanvasRenderingContext2D(context)->hitRegionAtPoint(localPos);
    if (!hitRegion || hitRegion->id().isEmpty())
        return String();

    return hitRegion->id();
}

} // namespace blink
