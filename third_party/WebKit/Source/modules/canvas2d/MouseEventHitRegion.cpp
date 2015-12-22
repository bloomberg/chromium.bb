// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/MouseEventHitRegion.h"

#include "core/events/MouseEvent.h"
#include "core/html/HTMLCanvasElement.h"

namespace blink {

String MouseEventHitRegion::region(MouseEvent& event)
{
    if (!event.target() || !isHTMLCanvasElement(event.target()->toNode()))
        return String();

    HTMLCanvasElement* canvas = toHTMLCanvasElement(event.target()->toNode());
    return regionIdFromAbsoluteLocation(*canvas, event.absoluteLocation());
}

} // namespace blink

