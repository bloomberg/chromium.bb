
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutViewItem_h
#define LayoutViewItem_h

#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutViewItem : public LayoutBlockItem {
public:
    explicit LayoutViewItem(LayoutView* layoutView)
        : LayoutBlockItem(layoutView)
    {
    }

    explicit LayoutViewItem(const LayoutBlockItem& item)
        : LayoutBlockItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isLayoutView());
    }

    explicit LayoutViewItem(std::nullptr_t) : LayoutBlockItem(nullptr) { }

    LayoutViewItem() { }

    IntRect documentRect() const
    {
        return toView()->documentRect();
    }

    IntSize layoutSize(IncludeScrollbarsInRect scrollbars = ExcludeScrollbars) const
    {
        return toView()->layoutSize(scrollbars);
    }

    LayoutRect overflowClipRect(const LayoutPoint& location, OverlayScrollbarSizeRelevancy scrollbars = IgnoreOverlayScrollbarSize) const
    {
        return toView()->overflowClipRect(location, scrollbars);
    }

private:
    LayoutView* toView() { return toLayoutView(layoutObject()); }
    const LayoutView* toView() const { return toLayoutView(layoutObject()); }
};

} // namespace blink

#endif // LayoutViewItem_h
