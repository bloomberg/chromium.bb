// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutItem_h
#define LayoutItem_h

#include "core/layout/LayoutObject.h"

#include "wtf/Allocator.h"

namespace blink {

class LayoutItem {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    explicit LayoutItem(LayoutObject* layoutObject)
        : m_layoutObject(layoutObject)
    {
    }

    LayoutItem(std::nullptr_t)
        : m_layoutObject(0)
    {
    }

    LayoutItem() : m_layoutObject(0) { }

    // TODO(leviw): This should be an UnspecifiedBoolType, but
    // using this operator allows the API to be landed in pieces.
    // https://crbug.com/499321
    operator LayoutObject*() const { return m_layoutObject; }

    bool needsLayout() { return m_layoutObject->needsLayout(); }
    void layout() { m_layoutObject->layout(); }

    LayoutItem container() const { return LayoutItem(m_layoutObject->container()); }

    void setMayNeedPaintInvalidation() { m_layoutObject->setMayNeedPaintInvalidation(); }

private:
    LayoutObject* m_layoutObject;
};

} // namespace blink

#endif // LayoutItem_h
