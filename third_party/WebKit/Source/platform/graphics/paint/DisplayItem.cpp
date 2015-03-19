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
    case DisplayItem::PaintPhaseMax: return "PaintPhaseClippingMask";
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
    case DisplayItem::BoxDecorationBackground: return "DrawingBoxDecorationBackground";
    case DisplayItem::Caret: return "DrawingCaret";
    case DisplayItem::ColumnRules: return "DrawingColumnRules";
    case DisplayItem::DebugRedFill: return "DrawingDebugRedFill";
    case DisplayItem::DragImage: return "DrawingDragImage";
    case DisplayItem::LinkHighlight: return "DrawingLinkHighlight";
    case DisplayItem::PageOverlay: return "PageOverlay";
    case DisplayItem::PageWidgetDelegateBackgroundFallback: return "DrawingPageWidgetDelegateBackgroundFallback";
    case DisplayItem::PopupContainerBorder: return "DrawingPopupContainerBorder";
    case DisplayItem::PopupListBoxBackground: return "DrawingPopupListBoxBackground";
    case DisplayItem::PopupListBoxRow: return "DrawingPopupListBoxRow";
    case DisplayItem::Resizer: return "DrawingResizer";
    case DisplayItem::SVGClip: return "DrawingSVGClip";
    case DisplayItem::SVGFilter: return "DrawingSVGFilter";
    case DisplayItem::SVGMask: return "DrawingSVGMask";
    case DisplayItem::ScrollbarCorner: return "DrawingScrollbarCorner";
    case DisplayItem::ScrollbarHorizontal: return "DrawingScrollbarHorizontal";
    case DisplayItem::ScrollbarTickMark: return "DrawingScrollbarTickMark";
    case DisplayItem::ScrollbarVertical: return "DrawingScrollbarVertical";
    case DisplayItem::SelectionGap: return "DrawingSelectionGap";
    case DisplayItem::SelectionTint: return "DrawingSelectionTint";
    case DisplayItem::VideoBitmap: return "DrawingVideoBitmap";
    case DisplayItem::ViewBackground: return "DrawingViewBackground";
    case DisplayItem::WebPlugin: return "DrawingWebPlugin";
    default:
        ASSERT_NOT_REACHED();
        return "Unknown";
    }
}

static WTF::String clipTypeAsDebugString(DisplayItem::Type type)
{
    PAINT_PHASE_BASED_DEBUG_STRINGS(ClipBox);
    PAINT_PHASE_BASED_DEBUG_STRINGS(ClipColumnBounds);
    PAINT_PHASE_BASED_DEBUG_STRINGS(ClipLayerFragment);

    switch (type) {
    case DisplayItem::ClipFileUploadControlRect: return "ClipFileUploadControlRect";
    case DisplayItem::ClipFrameToVisibleContentRect: return "ClipFrameToVisibleContentRect";
    case DisplayItem::ClipFrameScrollbars: return "ClipFrameScrollbars";
    case DisplayItem::ClipLayerBackground: return "ClipLayerBackground";
    case DisplayItem::ClipLayerColumnBounds: return "ClipLayerColumnBounds";
    case DisplayItem::ClipLayerFilter: return "ClipLayerFilter";
    case DisplayItem::ClipLayerForeground: return "ClipLayerForeground";
    case DisplayItem::ClipLayerParent: return "ClipLayerParent";
    case DisplayItem::ClipLayerOverflowControls: return "ClipLayerOverflowControls";
    case DisplayItem::ClipNodeImage: return "ClipNodeImage";
    case DisplayItem::ClipPopupListBoxFrame: return "ClipPopupListBoxFrame";
    case DisplayItem::ClipSelectionImage: return "ClipSelectionImage";
    case DisplayItem::PageWidgetDelegateClip: return "PageWidgetDelegateClip";
    case DisplayItem::TransparencyClip: return "TransparencyClip";
    default:
        ASSERT_NOT_REACHED();
        return "Unknown";
    }
}

static String transform3DTypeAsDebugString(DisplayItem::Type type)
{
    switch (type) {
    case DisplayItem::Transform3DElementTransform: return "Transform3DElementTransform";
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
    if (isEndFloatClipType(type))
        return "End" + typeAsDebugString(endFloatClipTypeToFloatClipType(type));

    PAINT_PHASE_BASED_DEBUG_STRINGS(Scroll);
    if (isEndScrollType(type))
        return "End" + typeAsDebugString(endScrollTypeToScrollType(type));

    if (isTransform3DType(type))
        return transform3DTypeAsDebugString(type);
    if (isEndTransform3DType(type))
        return "End" + transform3DTypeAsDebugString(endTransform3DTypeToTransform3DType(type));

    PAINT_PHASE_BASED_DEBUG_STRINGS(SubtreeCached);
    PAINT_PHASE_BASED_DEBUG_STRINGS(BeginSubtree);
    PAINT_PHASE_BASED_DEBUG_STRINGS(EndSubtree);

    switch (type) {
    case BeginFilter: return "BeginFilter";
    case EndFilter: return "EndFilter";
    case BeginCompositing: return "BeginCompositing";
    case EndCompositing: return "EndCompositing";
    case BeginTransform: return "BeginTransform";
    case EndTransform: return "EndTransform";
    case BeginClipPath: return "BeginClipPath";
    case EndClipPath: return "EndClipPath";
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
    stringBuilder.append(String::format("client: \"%p\", ", client()));
    if (!clientDebugString().isEmpty()) {
        stringBuilder.append(clientDebugString());
        stringBuilder.append(", ");
    }
    stringBuilder.append("type: \"");
    stringBuilder.append(typeAsDebugString(type()));
    stringBuilder.append('"');
    if (m_id.scopeContainer)
        stringBuilder.append(String::format(", scope: \"%p,%d\"", m_id.scopeContainer, m_id.scopeId));
}

#endif

} // namespace blink
