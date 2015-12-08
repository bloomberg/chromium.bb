// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FixedPositionContainerDisplayItem_h
#define FixedPositionContainerDisplayItem_h

#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginFixedPositionContainerDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginFixedPositionContainerDisplayItem(const DisplayItemClient& client)
        : PairedBeginDisplayItem(client, BeginFixedPositionContainer, sizeof(*this)) { }

    void replay(GraphicsContext&) const final { }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final;
};

class PLATFORM_EXPORT EndFixedPositionContainerDisplayItem final : public PairedEndDisplayItem {
public:
    EndFixedPositionContainerDisplayItem(const DisplayItemClient& client)
        : PairedEndDisplayItem(client, EndFixedPositionContainer, sizeof(*this)) { }

    void replay(GraphicsContext&) const final { }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginFixedPositionContainer; }
#endif
};

} // namespace blink

#endif // FixedPositionContainerDisplayItem_h
