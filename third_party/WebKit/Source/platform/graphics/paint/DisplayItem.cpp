// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

#ifndef NDEBUG

static WTF::String paintPhaseAsDebugString(int paintPhase)
{
    // Must be kept in sync with PaintPhase.
    switch (paintPhase) {
    case 0: return "PaintPhaseBlockBackground";
    case 1: return "PaintPhaseChildBlockBackground";
    case 2: return "PaintPhaseChildBlockBackgrounds";
    case 3: return "PaintPhaseFloat";
    case 4: return "PaintPhaseForeground";
    case 5: return "PaintPhaseOutline";
    case 6: return "PaintPhaseChildOutlines";
    case 7: return "PaintPhaseSelfOutline";
    case 8: return "PaintPhaseSelection";
    case 9: return "PaintPhaseCollapsedTableBorders";
    case 10: return "PaintPhaseTextClip";
    case 11: return "PaintPhaseMask";
    case 12: return "PaintPhaseClippingMask";
    case 13: return "PaintPhaseCaret";
    default:
        ASSERT_NOT_REACHED();
        return "Unknown";
    }
}

#define PAINT_PHASE_BASED_DEBUG_STRINGS(Category) \
    if (type >= DisplayItem::Category##PaintPhaseFirst && type <= DisplayItem::Category##PaintPhaseLast) \
        return #Category + paintPhaseAsDebugString(type - DisplayItem::Category##PaintPhaseFirst);

static WTF::String drawingTypeAsDebugString(DisplayItem::Type type)
{
    PAINT_PHASE_BASED_DEBUG_STRINGS(Drawing);

    switch (type) {
    case DisplayItem::ColumnRules: return "ColumnRules";
    case DisplayItem::DragImage: return "DragImage";
    case DisplayItem::LinkHighlight: return "LinkHighlight";
    case DisplayItem::PageWidgetDelegateBackgroundFallback: return "PageWidgetDelegateBackgroundFallback";
    case DisplayItem::Resizer: return "Resizer";
    case DisplayItem::Scrollbar: return "Scrollbar";
    case DisplayItem::ScrollbarCorner: return "ScrollbarCorner";
    case DisplayItem::ScrollbarTickMark: return "ScrollbarTickMark";
    case DisplayItem::VideoBitmap: return "VideoBitmap";
    case DisplayItem::ViewBackground: return "ViewBackground";
    default:
        ASSERT_NOT_REACHED();
        return "Unknown";
    }
}

static WTF::String clipTypeAsDebugString(DisplayItem::Type type)
{
    PAINT_PHASE_BASED_DEBUG_STRINGS(ClipBox);
    PAINT_PHASE_BASED_DEBUG_STRINGS(ClipLayerFragment);

    switch (type) {
    case DisplayItem::ClipLayerOverflowControls: return "ClipLayerOverflowControls";
    case DisplayItem::ClipLayerBackground: return "ClipLayerBackground";
    case DisplayItem::ClipLayerFilter: return "ClipLayerFilter";
    case DisplayItem::ClipLayerForeground: return "ClipLayerForeground";
    case DisplayItem::ClipNodeImage: return "ClipNodeImage";
    case DisplayItem::ClipFrameToVisibleContentRect: return "ClipFrameToVisibleContentRect";
    case DisplayItem::ClipFrameScrollbars: return "ClipFrameScrollbars";
    case DisplayItem::ClipSelectionImage: return "ClipSelectionImage";
    case DisplayItem::PageWidgetDelegateClip: return "PageWidgetDelegateClip";
    case DisplayItem::TransparencyClip: return "TransparencyClip";
    default:
        ASSERT_NOT_REACHED();
        return "Unknown";
    }
}

WTF::String DisplayItem::typeAsDebugString(Type type)
{
    if (isDrawingType(type))
        return drawingTypeAsDebugString(type);
    if (isCachedType(type))
        return "Cached" + drawingTypeAsDebugString(cachedTypeToDrawingType(type));
    if (isClipType(type))
        return clipTypeAsDebugString(type);
    if (isEndClipType(type))
        return "End" + clipTypeAsDebugString(endClipTypeToClipType(type));
    PAINT_PHASE_BASED_DEBUG_STRINGS(FloatClip);

    switch (type) {
    case BeginFilter: return "BeginFilter";
    case EndFilter: return "EndFilter";
    case BeginCompositing: return "BeginCompositing";
    case EndCompositing: return "EndCompositing";
    case BeginTransform: return "BeginTransform";
    case EndTransform: return "EndTransform";
    case BeginClipPath: return "BeginClipPath";
    case EndClipPath: return "EndClipPath";
    case SVGFilter: return "SVGFilter";
    default:
        ASSERT_NOT_REACHED();
        return "Unknown";
    }
}

WTF::String DisplayItem::asDebugString() const
{
    WTF::StringBuilder stringBuilder;
    stringBuilder.append('{');
    dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append('}');
    return stringBuilder.toString();
}

void DisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    stringBuilder.append("name: \"");
    stringBuilder.append(name());
    stringBuilder.append("\", ");
    stringBuilder.append(String::format("client: \"%p\", ", client()));
    if (!clientDebugString().isEmpty()) {
        stringBuilder.append(clientDebugString());
        stringBuilder.append(", ");
    }
    stringBuilder.append("type: \"");
    stringBuilder.append(typeAsDebugString(type()));
    stringBuilder.append('"');
}

#endif

} // namespace blink
