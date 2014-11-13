// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"
#ifndef NDEBUG
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/text/StringBuilder.h"
#endif

namespace blink {

const PaintList& DisplayItemList::paintList()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    updatePaintList();
    return m_paintList;
}

void DisplayItemList::add(WTF::PassOwnPtr<DisplayItem> displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_newPaints.append(displayItem);
}

void DisplayItemList::invalidate(DisplayItemClient renderer)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_invalidated.add(renderer);
}

PaintList::iterator DisplayItemList::findDisplayItem(PaintList::iterator begin, const DisplayItem& displayItem)
{
    PaintList::iterator end = m_paintList.end();
    if (displayItem.client() && !m_paintListRenderers.contains(displayItem.client()))
        return end;

    for (PaintList::iterator it = begin; it != end; ++it) {
        DisplayItem& existing = **it;
        if (existing.idsEqual(displayItem))
            return it;
    }

    // FIXME: Properly handle clips.
    ASSERT(!displayItem.client());
    return end;
}

bool DisplayItemList::wasInvalidated(const DisplayItem& displayItem) const
{
    // FIXME: Use a bit on DisplayItemClient instead of tracking m_invalidated.
    return displayItem.client() && m_invalidated.contains(displayItem.client());
}

static void appendDisplayItem(PaintList& list, HashSet<DisplayItemClient>& renderers, WTF::PassOwnPtr<DisplayItem> displayItem)
{
    if (DisplayItemClient renderer = displayItem->client())
        renderers.add(renderer);
    list.append(displayItem);
}

// Update the existing paintList by removing invalidated entries, updating
// repainted ones, and appending new items.
//
// The algorithm is O(|existing paint list| + |newly painted list|): by using
// the ordering implied by the existing paint list, extra treewalks are avoided.
void DisplayItemList::updatePaintList()
{
    PaintList updatedList;
    HashSet<DisplayItemClient> updatedRenderers;

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

void DisplayItemList::showDebugData() const
{
    fprintf(stderr, "paint list: [%s]\n", paintListAsDebugString(m_paintList).utf8().data());
    fprintf(stderr, "new paints: [%s]\n", paintListAsDebugString(m_newPaints).utf8().data());
}

#endif

} // namespace blink
