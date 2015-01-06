// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/PaintInfo.h"

namespace blink {

DisplayItem::Type PaintInfo::displayItemTypeForClipping() const
{
    switch (phase) {
    case PaintPhaseChildBlockBackgrounds:
        return DisplayItem::ClipBoxChildBlockBackgrounds;
        break;
    case PaintPhaseFloat:
        return DisplayItem::ClipBoxFloat;
        break;
    case PaintPhaseForeground:
        return DisplayItem::ClipBoxChildBlockBackgrounds;
        break;
    case PaintPhaseChildOutlines:
        return DisplayItem::ClipBoxChildOutlines;
        break;
    case PaintPhaseSelection:
        return DisplayItem::ClipBoxSelection;
        break;
    case PaintPhaseCollapsedTableBorders:
        return DisplayItem::ClipBoxCollapsedTableBorders;
        break;
    case PaintPhaseTextClip:
        return DisplayItem::ClipBoxTextClip;
        break;
    case PaintPhaseClippingMask:
        return DisplayItem::ClipBoxClippingMask;
        break;
    case PaintPhaseChildBlockBackground:
    case PaintPhaseOutline:
    case PaintPhaseBlockBackground:
    case PaintPhaseSelfOutline:
    case PaintPhaseMask:
    case PaintPhaseCaret:
        ASSERT_NOT_REACHED();
    }
    // This should never happen.
    return DisplayItem::ClipBoxForeground;
}

} // namespace blink
