// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewDisplayList.h"

#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"

#ifndef NDEBUG
#include "core/rendering/RenderObject.h"
#include "wtf/text/WTFString.h"
#endif

namespace blink {

// DisplayItem types must be kept in sync with PaintPhase.
COMPILE_ASSERT((unsigned)DisplayItem::DrawingPaintPhaseBlockBackground == (unsigned)PaintPhaseBlockBackground, DisplayItem_Type_should_stay_in_sync);
COMPILE_ASSERT((unsigned)DisplayItem::DrawingPaintPhaseClippingMask == (unsigned)PaintPhaseClippingMask, DisplayItem_Type_should_stay_in_sync);

const PaintList& ViewDisplayList::paintList()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    updatePaintList();
    return m_paintList;
}

void ViewDisplayList::add(WTF::PassOwnPtr<DisplayItem> displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_newPaints.append(displayItem);
}

void ViewDisplayList::invalidate(const RenderObject* renderer)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_invalidated.add(renderer);
}

PaintList::iterator ViewDisplayList::findDisplayItem(PaintList::iterator begin, const DisplayItem& displayItem)
{
    PaintList::iterator end = m_paintList.end();
    if (displayItem.renderer() && !m_paintListRenderers.contains(displayItem.renderer()))
        return end;

    for (PaintList::iterator it = begin; it != end; ++it) {
        DisplayItem& existing = **it;
        if (existing.idsEqual(displayItem))
            return it;
    }

    // FIXME: Properly handle clips.
    ASSERT(!displayItem.renderer());
    return end;
}

bool ViewDisplayList::wasInvalidated(const DisplayItem& displayItem) const
{
    // FIXME: Use a bit on RenderObject instead of tracking m_invalidated.
    return displayItem.renderer() && m_invalidated.contains(displayItem.renderer());
}

static void appendDisplayItem(PaintList& list, HashSet<const RenderObject*>& renderers, WTF::PassOwnPtr<DisplayItem> displayItem)
{
    if (const RenderObject* renderer = displayItem->renderer())
        renderers.add(renderer);
    list.append(displayItem);
}

// Update the existing paintList by removing invalidated entries, updating
// repainted ones, and appending new items.
//
// The algorithm is O(|existing paint list| + |newly painted list|): by using
// the ordering implied by the existing paint list, extra treewalks are avoided.
void ViewDisplayList::updatePaintList()
{
    PaintList updatedList;
    HashSet<const RenderObject*> updatedRenderers;

    if (int maxCapacity = m_newPaints.size() + std::max(0, (int)m_paintList.size() - (int)m_invalidated.size()))
        updatedList.reserveCapacity(maxCapacity);

    PaintList::iterator paintListIt = m_paintList.begin();
    PaintList::iterator paintListEnd = m_paintList.end();

    for (OwnPtr<DisplayItem>& newDisplayItem : m_newPaints) {
        if (!wasInvalidated(*newDisplayItem)) {
            PaintList::iterator repaintIt = findDisplayItem(paintListIt, *newDisplayItem);
            if (repaintIt != paintListEnd) {
                // Copy all of the existing items over until we hit the repaint.
                for (; paintListIt != repaintIt; ++paintListIt) {
                    if (!wasInvalidated(**paintListIt))
                        appendDisplayItem(updatedList, updatedRenderers, paintListIt->release());
                }
                paintListIt++;
            }
        }
        // Copy over the new item.
        appendDisplayItem(updatedList, updatedRenderers, newDisplayItem.release());
    }

    // Copy over any remaining items that were not invalidated.
    for (; paintListIt != paintListEnd; ++paintListIt) {
        if (!wasInvalidated(**paintListIt))
            appendDisplayItem(updatedList, updatedRenderers, paintListIt->release());
    }

    m_invalidated.clear();
    m_newPaints.clear();
    m_paintList.clear();
    m_paintList.swap(updatedList);
    m_paintListRenderers.clear();
    m_paintListRenderers.swap(updatedRenderers);
}

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

WTF::String DisplayItem::rendererDebugString(const RenderObject* renderer)
{
    if (renderer && renderer->node())
        return String::format("nodeName: \"%s\", renderer: \"%p\"", renderer->node()->nodeName().utf8().data(), renderer);
    return String::format("renderer: \"%p\"", renderer);
}

WTF::String DisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}", rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data());
}

static WTF::String paintListAsDebugString(const PaintList& list)
{
    StringBuilder stringBuilder;
    bool isFirst = true;
    for (auto& displayItem : list) {
        if (!isFirst)
            stringBuilder.append(", ");
        isFirst = false;
        stringBuilder.append(displayItem->asDebugString());
    }
    return stringBuilder.toString();
}

void ViewDisplayList::showDebugData() const
{
    fprintf(stderr, "paint list: [%s]\n", paintListAsDebugString(m_paintList).utf8().data());
    fprintf(stderr, "new paints: [%s]\n", paintListAsDebugString(m_newPaints).utf8().data());
}
#endif

} // namespace blink
