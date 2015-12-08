// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FixedPositionDisplayItem_h
#define FixedPositionDisplayItem_h

#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginFixedPositionDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginFixedPositionDisplayItem(const DisplayItemClient& client)
        : PairedBeginDisplayItem(client, BeginFixedPosition, sizeof(*this)) { }

    void replay(GraphicsContext&) const final { }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final;
};

class PLATFORM_EXPORT EndFixedPositionDisplayItem final : public PairedEndDisplayItem {
public:
    EndFixedPositionDisplayItem(const DisplayItemClient& client)
        : PairedEndDisplayItem(client, EndFixedPosition, sizeof(*this)) { }

    void replay(GraphicsContext&) const final { }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginFixedPosition; }
#endif
};

} // namespace blink

#endif // FixedPositionDisplayItem_h
