// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterDisplayItem_h
#define FilterDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebFilterOperations.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginFilterDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginFilterDisplayItem(const DisplayItemClient& client, PassRefPtr<SkImageFilter> imageFilter, const FloatRect& bounds, PassOwnPtr<WebFilterOperations> filterOperations = nullptr)
        : PairedBeginDisplayItem(client, BeginFilter, sizeof(*this))
        , m_imageFilter(imageFilter)
        , m_webFilterOperations(std::move(filterOperations))
        , m_bounds(bounds) { }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;
    bool drawsContent() const override;

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
#if ENABLE(ASSERT)
    bool equals(const DisplayItem& other) const final
    {
        return DisplayItem::equals(other)
            // TODO(wangxianzhu): compare m_imageFilter and m_webFilterOperations.
            && m_bounds == static_cast<const BeginFilterDisplayItem&>(other).m_bounds;
    }
#endif

    // FIXME: m_imageFilter should be replaced with m_webFilterOperations when copying data to the compositor.
    RefPtr<SkImageFilter> m_imageFilter;
    OwnPtr<WebFilterOperations> m_webFilterOperations;
    const FloatRect m_bounds;
};

class PLATFORM_EXPORT EndFilterDisplayItem final : public PairedEndDisplayItem {
public:
    EndFilterDisplayItem(const DisplayItemClient& client)
        : PairedEndDisplayItem(client, EndFilter, sizeof(*this)) { }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginFilter; }
#endif
};

}

#endif // FilterDisplayItem_h
