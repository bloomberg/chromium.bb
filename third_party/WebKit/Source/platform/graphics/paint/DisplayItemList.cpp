// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/graphics/paint/PaintChunk.h"

namespace blink {

void DisplayItemList::appendVisualRect(const IntRect& visualRect)
{
    size_t itemIndex = m_visualRects.size();
    const DisplayItem& item = (*this)[itemIndex];

    // For paired display items such as transforms, since we are not guaranteed containment, the
    // visual rect must comprise the union of the visual rects for all items within its block.

    if (item.isBegin()) {
        m_visualRects.append(visualRect);
        m_beginItemIndices.append(itemIndex);

    } else if (item.isEnd()) {
        size_t lastBeginIndex = m_beginItemIndices.last();
        m_beginItemIndices.removeLast();

        // Ending bounds match the starting bounds.
        m_visualRects.append(m_visualRects[lastBeginIndex]);

        // The block that ended needs to be included in the bounds of the enclosing block.
        growCurrentBeginItemVisualRect(m_visualRects[lastBeginIndex]);

    } else {
        m_visualRects.append(visualRect);
        growCurrentBeginItemVisualRect(visualRect);
    }
}

void DisplayItemList::growCurrentBeginItemVisualRect(const IntRect& visualRect)
{
    if (!m_beginItemIndices.isEmpty())
        m_visualRects[m_beginItemIndices.last()].unite(visualRect);
}

DisplayItemList::Range<DisplayItemList::iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk)
{
    return Range<iterator>(begin() + paintChunk.beginIndex, begin() + paintChunk.endIndex);
}

DisplayItemList::Range<DisplayItemList::const_iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk) const
{
    return Range<const_iterator>(begin() + paintChunk.beginIndex, begin() + paintChunk.endIndex);
}

} // namespace blink
