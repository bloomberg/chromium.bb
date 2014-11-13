// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

#ifndef NDEBUG

WTF::String DisplayItem::typeAsDebugString(DisplayItem::Type type)
{
    switch (type) {
    case DisplayItem::DrawingPaintPhaseBlockBackground: return "DrawingPaintPhaseBlockBackground";
    case DisplayItem::DrawingPaintPhaseChildBlockBackground: return "DrawingPaintPhaseChildBlockBackground";
    case DisplayItem::DrawingPaintPhaseChildBlockBackgrounds: return "DrawingPaintPhaseChildBlockBackgrounds";
    case DisplayItem::DrawingPaintPhaseFloat: return "DrawingPaintPhaseFloat";
    case DisplayItem::DrawingPaintPhaseForeground: return "DrawingPaintPhaseForeground";
    case DisplayItem::DrawingPaintPhaseOutline: return "DrawingPaintPhaseOutline";
    case DisplayItem::DrawingPaintPhaseChildOutlines: return "DrawingPaintPhaseChildOutlines";
    case DisplayItem::DrawingPaintPhaseSelfOutline: return "DrawingPaintPhaseSelfOutline";
    case DisplayItem::DrawingPaintPhaseSelection: return "DrawingPaintPhaseSelection";
    case DisplayItem::DrawingPaintPhaseCollapsedTableBorders: return "DrawingPaintPhaseCollapsedTableBorders";
    case DisplayItem::DrawingPaintPhaseTextClip: return "DrawingPaintPhaseTextClip";
    case DisplayItem::DrawingPaintPhaseMask: return "DrawingPaintPhaseMask";
    case DisplayItem::DrawingPaintPhaseClippingMask: return "DrawingPaintPhaseClippingMask";
    case DisplayItem::ClipLayerOverflowControls: return "ClipLayerOverflowControls";
    case DisplayItem::ClipLayerBackground: return "ClipLayerBackground";
    case DisplayItem::ClipLayerParent: return "ClipLayerParent";
    case DisplayItem::ClipLayerFilter: return "ClipLayerFilter";
    case DisplayItem::ClipLayerForeground: return "ClipLayerForeground";
    case DisplayItem::ClipLayerFragmentFloat: return "ClipLayerFragmentFloat";
    case DisplayItem::ClipLayerFragmentForeground: return "ClipLayerFragmentForeground";
    case DisplayItem::ClipLayerFragmentChildOutline: return "ClipLayerFragmentChildOutline";
    case DisplayItem::ClipLayerFragmentOutline: return "ClipLayerFragmentOutline";
    case DisplayItem::ClipLayerFragmentMask: return "ClipLayerFragmentMask";
    case DisplayItem::ClipLayerFragmentClippingMask: return "ClipLayerFragmentClippingMask";
    case DisplayItem::ClipLayerFragmentParent: return "ClipLayerFragmentParent";
    case DisplayItem::ClipLayerFragmentSelection: return "ClipLayerFragmentSelection";
    case DisplayItem::ClipLayerFragmentChildBlockBackgrounds: return "ClipLayerFragmentChildBlockBackgrounds";
    case DisplayItem::EndClip: return "EndClip";
    }
    ASSERT_NOT_REACHED();
    return "Unknown";
}

WTF::String DisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}", clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data());
}

#endif

} // namespace blink
