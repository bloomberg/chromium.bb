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
    case DrawingPaintPhaseCaret: return "DrawingPaintPhaseCaret";
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
    case DisplayItem::BeginFilter: return "BeginFilter";
    case DisplayItem::EndFilter: return "EndFilter";
    case DisplayItem::TransparencyClip: return "TransparencyClip";
    case DisplayItem::BeginTransparency: return "BeginTransparency";
    case DisplayItem::EndTransparency: return "EndTransparency";
    case ClipBoxChildBlockBackgrounds: return "ClipBoxChildBlockBackgrounds";
    case ClipBoxFloat: return "ClipBoxFloat";
    case ClipBoxForeground: return "ClipBoxForeground";
    case ClipBoxChildOutlines: return "ClipBoxChildOutlines";
    case ClipBoxSelection: return "ClipBoxSelection";
    case ClipBoxCollapsedTableBorders: return "ClipBoxCollapsedTableBorders";
    case ClipBoxTextClip: return "ClipBoxTextClip";
    case ClipBoxClippingMask: return "ClipBoxClippingMask";
    case BeginTransform: return "BeginTransform";
    case EndTransform: return "EndTransform";
    case ScrollbarCorner: return "ScrollbarCorner";
    case Scrollbar: return "Scrollbar";
    case Resizer: return "Resizer";
    case ColumnRules: return "ColumnRules";
    case ClipNodeImage: return "ClipNodeImage";
    case ClipFrameToVisibleContentRect: return "ClipFrameToVisibleContentRect";
    case ClipFrameScrollbars: return "ClipFrameScrollbars";
    case ClipSelectionImage: return "ClipSelectionImage";
    case FloatClipForeground: return "FloatClipForeground";
    case FloatClipSelection: return "FloatClipSelection";
    case FloatClipSelfOutline: return "FloatClipSelfOutline";
    case EndFloatClip: return "EndFloatClip";
    case BeginClipPath: return "BeginClipPath";
    case EndClipPath: return "EndClipPath";
    case VideoBitmap: return "VideoBitmap";
    case ImageBitmap: return "ImageBitmap";
    case DragImage: return "DragImage";
    }
    ASSERT_NOT_REACHED();
    return "Unknown";
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
