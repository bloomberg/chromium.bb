// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/TouchHitRegion.h"

#include "core/dom/Touch.h"
#include "core/html/HTMLCanvasElement.h"

namespace blink {

String TouchHitRegion::region(Touch& touch)
{
    if (!touch.target() || !isHTMLCanvasElement(touch.target()->toNode()))
        return String();

    HTMLCanvasElement* canvas = toHTMLCanvasElement(touch.target()->toNode());
    return regionIdFromAbsoluteLocation(*canvas, touch.absoluteLocation());
}

} // namespace blink
