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
        ASSERT(item.hasValidClient());
        DisplayItem& result = ContiguousContainer::appendByMoving(item, item.derivedSize());
        // ContiguousContainer::appendByMoving() calls an in-place constructor
        // on item which replaces it with a tombstone/"dead display item" that
        // can be safely destructed but should never be used.
        ASSERT(!item.hasValidClient());
#ifndef NDEBUG
        // Save original debug string in the old item to help debugging.
        item.setClientDebugString(originalDebugString);
#endif
        return result;
    }

#if ENABLE(ASSERT)
    void assertDisplayItemClientsAreAlive() const
    {
        for (auto& item : *this) {
#ifdef NDEBUG
            ASSERT_WITH_MESSAGE(DisplayItemClient::isAlive(item.client()), "Short-lived DisplayItemClient. See crbug.com/570030.");
#else
            ASSERT_WITH_MESSAGE(DisplayItemClient::isAlive(item.client()), "Short-lived DisplayItemClient: %s. See crbug.com/570030.", item.clientDebugString().utf8().data());
#endif
        }
    }
#endif

};

} // namespace blink

#endif // DisplayItemList_h
