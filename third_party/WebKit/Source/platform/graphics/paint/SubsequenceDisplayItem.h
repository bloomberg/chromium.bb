// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubsequenceDisplayItem_h
#define SubsequenceDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

class BeginSubsequenceDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginSubsequenceDisplayItem(const DisplayItemClientWrapper& client, DisplayItem::Type type)
        : PairedBeginDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(DisplayItem::isSubsequenceType(type));
    }
};

class EndSubsequenceDisplayItem final : public PairedEndDisplayItem {
public:
    EndSubsequenceDisplayItem(const DisplayItemClientWrapper& client, DisplayItem::Type type)
        : PairedEndDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(DisplayItem::isEndSubsequenceType(type));
    }

#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final
    {
        return otherType == DisplayItem::endSubsequenceTypeToSubsequenceType(type());
    }
#endif
};

} // namespace blink

#endif // SubsequenceDisplayItem_h
