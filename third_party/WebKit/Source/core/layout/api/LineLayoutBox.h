// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBox_h
#define LineLayoutBox_h

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBox;

class LineLayoutBox : public LineLayoutBoxModel {
public:
    explicit LineLayoutBox(LayoutBox* layoutBox)
        : LineLayoutBoxModel(layoutBox)
    {
    }

    explicit LineLayoutBox(const LineLayoutItem& item)
        : LineLayoutBoxModel(item)
    {
        ASSERT(!item || item.isBox());
    }

    explicit LineLayoutBox(std::nullptr_t) : LineLayoutBoxModel(nullptr) { }

    LineLayoutBox() { }

    LayoutPoint location() const
    {
        return toBox()->location();
    }

    LayoutSize size() const
    {
        return toBox()->size();
    }

    void setLogicalHeight(LayoutUnit size)
    {
        toBox()->setLogicalHeight(size);
    }

    LayoutUnit logicalHeight() const
    {
        return toBox()->logicalHeight();
    }

    LayoutUnit flipForWritingMode(LayoutUnit unit) const
    {
        return toBox()->flipForWritingMode(unit);
    }

    void moveWithEdgeOfInlineContainerIfNecessary(bool isHorizontal)
    {
        toBox()->moveWithEdgeOfInlineContainerIfNecessary(isHorizontal);
    }

    bool hasOverflowModel() const
    {
        return toBox()->hasOverflowModel();
    }
    LayoutRect logicalVisualOverflowRectForPropagation(const ComputedStyle& style) const
    {
        return toBox()->logicalVisualOverflowRectForPropagation(style);
    }
    LayoutRect logicalLayoutOverflowRectForPropagation(const ComputedStyle& style) const
    {
        return toBox()->logicalLayoutOverflowRectForPropagation(style);
    }

private:
    LayoutBox* toBox()
    {
        return toLayoutBox(layoutObject());
    }

    const LayoutBox* toBox() const
    {
        return toLayoutBox(layoutObject());
    }
};

inline LineLayoutBox LineLayoutItem::containingBlock() const
{
    return LineLayoutBox(layoutObject()->containingBlock());
}

} // namespace blink

#endif // LineLayoutBox_h
