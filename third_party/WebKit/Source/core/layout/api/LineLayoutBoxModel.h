// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBoxModel_h
#define LineLayoutBoxModel_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutItem.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBoxModelObject;

class LineLayoutBoxModel : public LineLayoutItem {
public:
    explicit LineLayoutBoxModel(LayoutBoxModelObject* layoutBox)
        : LineLayoutItem(layoutBox)
    {
    }

    explicit LineLayoutBoxModel(const LineLayoutItem& item)
        : LineLayoutItem(item)
    {
        ASSERT(!item || item.isBoxModelObject());
    }

    LineLayoutBoxModel() { }

    DeprecatedPaintLayer* layer() const
    {
        return toBoxModel()->layer();
    }

    bool hasSelfPaintingLayer() const
    {
        return toBoxModel()->hasSelfPaintingLayer();
    }

    LayoutUnit paddingTop() const
    {
        return toBoxModel()->paddingTop();
    }

    LayoutUnit paddingRight() const
    {
        return toBoxModel()->paddingRight();
    }

    int borderTop() const
    {
        return toBoxModel()->borderTop();
    }

    int borderRight() const
    {
        return toBoxModel()->borderRight();
    }

    LayoutUnit borderAndPaddingLogicalHeight() const
    {
        return toBoxModel()->borderAndPaddingLogicalHeight();
    }

    LayoutUnit marginTop() const
    {
        return toBoxModel()->marginTop();
    }

    LayoutUnit marginBottom() const
    {
        return toBoxModel()->marginBottom();
    }

    LayoutUnit marginLeft() const
    {
        return toBoxModel()->marginLeft();
    }

    LayoutUnit marginRight() const
    {
        return toBoxModel()->marginRight();
    }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode lineDirectionMode, LinePositionMode linePositionMode) const
    {
        return toBoxModel()->lineHeight(firstLine, lineDirectionMode, linePositionMode);
    }

    int baselinePosition(FontBaseline fontBaseline, bool firstLine, LineDirectionMode lineDirectionMode, LinePositionMode linePositionMode) const
    {
        return toBoxModel()->baselinePosition(fontBaseline, firstLine, lineDirectionMode, linePositionMode);
    }

private:
    LayoutBoxModelObject* toBoxModel() { return toLayoutBoxModelObject(layoutObject()); }
    const LayoutBoxModelObject* toBoxModel() const { return toLayoutBoxModelObject(layoutObject()); }
};

} // namespace blink

#endif // LineLayoutBoxModel_h
