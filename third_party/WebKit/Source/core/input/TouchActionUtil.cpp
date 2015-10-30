// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/input/TouchActionUtil.h"

#include "core/dom/Node.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace TouchActionUtil {

namespace {

// touch-action applies to all elements with both width AND height properties.
// According to the CSS Box Model Spec (http://dev.w3.org/csswg/css-box/#the-width-and-height-properties)
// width applies to all elements but non-replaced inline elements, table rows, and row groups and
// height applies to all elements but non-replaced inline elements, table columns, and column groups.
static bool supportsTouchAction(const LayoutObject& object)
{
    if (object.isInline() && !object.isReplaced())
        return false;
    if (object.isTableRow() || object.isLayoutTableCol())
        return false;

    return true;
}

} // namespace

TouchAction computeEffectiveTouchAction(const Node& node)
{
    // Start by permitting all actions, then walk the elements supporting
    // touch-action from the target node up to the nearest scrollable ancestor
    // and exclude any prohibited actions.
    TouchAction effectiveTouchAction = TouchActionAuto;
    for (const Node* curNode = &node; curNode; curNode = ComposedTreeTraversal::parent(*curNode)) {
        if (LayoutObject* layoutObject = curNode->layoutObject()) {
            if (supportsTouchAction(*layoutObject)) {
                TouchAction action = layoutObject->style()->touchAction();
                effectiveTouchAction &= action;
                if (effectiveTouchAction == TouchActionNone)
                    break;
            }

            // If we've reached an ancestor that supports a touch action, search no further.
            if (layoutObject->isBox() && toLayoutBox(layoutObject)->scrollsOverflow())
                break;
        }
    }
    return effectiveTouchAction;
}

} // namespace TouchActionUtil
} // namespace blink
