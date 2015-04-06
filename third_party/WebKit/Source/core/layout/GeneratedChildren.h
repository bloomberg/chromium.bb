// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeneratedChildren_h
#define GeneratedChildren_h

#include "core/layout/LayoutObject.h"

namespace blink {

// We only create "generated" child renderers like one for first-letter if:
// - the firstLetterBlock can have children in the DOM and
// - the block doesn't have any special assumption on its text children.
// This correctly prevents form controls from having such renderers.
static bool canHaveGeneratedChildren(const LayoutObject& renderer)
{
    // FIXME: LayoutMedia::layout makes assumptions about what children are allowed
    // so we can't support generated content.
    if (renderer.isMedia() || renderer.isTextControl() || renderer.isMenuList())
        return false;

    if (renderer.isLayoutRegion())
        return true;

    // Input elements can't have generated children, but button elements can. We'll
    // write the code assuming any other button types that might emerge in the future
    // can also have children.
    if (renderer.isLayoutButton())
        return !isHTMLInputElement(*renderer.node());

    return renderer.canHaveChildren();
}

} // namespace blink

#endif // GeneratedChildren_h
