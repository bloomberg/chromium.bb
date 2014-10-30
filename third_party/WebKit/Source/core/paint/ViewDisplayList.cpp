// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewDisplayList.h"

#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const PaintList& ViewDisplayList::paintList()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    updatePaintList();
    return m_newPaints;
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

bool ViewDisplayList::isRepaint(PaintList::iterator begin, const DisplayItem& displayItem)
{
    notImplemented();
    return false;
}

// Update the existing paintList by removing invalidated entries, updating repainted existing ones, and
// appending new items.
//
// The algorithm should be O(|existing paint list| + |newly painted list|). By using the ordering
// implied by the existing paint list, extra treewalks are avoided.
void ViewDisplayList::updatePaintList()
{
    notImplemented();
}

} // namespace blink
