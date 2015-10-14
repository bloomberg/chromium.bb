// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemList_h
#define DisplayItemList_h

#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "wtf/Alignment.h"
#include "wtf/Assertions.h"

#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

// kDisplayItemAlignment must be a multiple of alignof(derived display item) for
// each derived display item; the ideal value is the least common multiple.
// Currently the limiting factor is TransformationMatrix (in
// BeginTransform3DDisplayItem), which requests 16-byte alignment.
static const size_t kDisplayItemAlignment = WTF_ALIGN_OF(BeginTransform3DDisplayItem);
static const size_t kMaximumDisplayItemSize = sizeof(BeginTransform3DDisplayItem);

// A container for a list of display items.
class DisplayItemList : public ContiguousContainer<DisplayItem, kDisplayItemAlignment> {
public:
    DisplayItemList(size_t initialSizeBytes)
        : ContiguousContainer(kMaximumDisplayItemSize, initialSizeBytes) {}

    DisplayItem& appendByMoving(DisplayItem& item)
    {
#ifndef NDEBUG
        WTF::String originalDebugString = item.asDebugString();
#endif
        ASSERT(item.isValid());
        DisplayItem& result = ContiguousContainer::appendByMoving(item, item.derivedSize());
        // ContiguousContainer::appendByMoving() called in-place constructor on item, which invalidated it.
        ASSERT(!item.isValid());
#ifndef NDEBUG
        // Save original debug string in the old item to help debugging.
        item.setClientDebugString(originalDebugString);
#endif
        return result;
    }
};

} // namespace blink

#endif // DisplayItemList_h
