// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MouseEventHitRegion_h
#define MouseEventHitRegion_h

#include "core/events/MouseEvent.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"

namespace WebCore {

class MouseEventHitRegion {
public:
    static String region(MouseEvent& event, bool& isNull)
    {
        if (!isHTMLCanvasElement(event.target()->toNode())) {
            isNull = true;
            return String();
        }

        HTMLCanvasElement* canvas = toHTMLCanvasElement(event.target()->toNode());
        CanvasRenderingContext* context = canvas->renderingContext();
        if (!context || !context->is2d()) {
            isNull = true;
            return String();
        }

        HitRegion* hitRegion = toCanvasRenderingContext2D(context)->
            hitRegionAtPoint(LayoutPoint(event.offsetX(), event.offsetY()));

        String id;
        if (hitRegion)
            id = hitRegion->id();

        isNull = id.isEmpty();

        return id;
    }
};

} // namespace WebCore

#endif
