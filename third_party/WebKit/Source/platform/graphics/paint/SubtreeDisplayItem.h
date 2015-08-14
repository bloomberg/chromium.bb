// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubtreeDisplayItem_h
#define SubtreeDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

class BeginSubtreeDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginSubtreeDisplayItem(const DisplayItemClientWrapper& client, Type type)
        : PairedBeginDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(isBeginSubtreeType(type));
    }
};

class EndSubtreeDisplayItem final : public PairedEndDisplayItem {
public:
    EndSubtreeDisplayItem(const DisplayItemClientWrapper& client, Type type)
        : PairedEndDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(isEndSubtreeType(type));
    }

#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return DisplayItem::isBeginSubtreeType(otherType); }
#endif
};

} // namespace blink

#endif // SubtreeDisplayItem_h
