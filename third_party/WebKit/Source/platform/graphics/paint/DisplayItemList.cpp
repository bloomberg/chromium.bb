// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/graphics/paint/PaintChunk.h"

namespace blink {

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
