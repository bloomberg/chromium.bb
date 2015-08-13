// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutInline_h
#define LineLayoutInline_h

#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "platform/LayoutUnit.h"

namespace blink {

class ComputedStyle;
class LayoutInline;
class LayoutObject;

class LineLayoutInline : public LineLayoutBoxModel {
public:
    explicit LineLayoutInline(LayoutInline* layoutInline)
        : LineLayoutBoxModel(layoutInline)
    {
    }

    explicit LineLayoutInline(const LineLayoutItem& item)
        : LineLayoutBoxModel(item)
    {
        ASSERT(!item || item.isLayoutInline());
    }

    LineLayoutInline() { }

    LineLayoutItem firstChild() const
    {
        return LineLayoutItem(toInline()->firstChild());
    }

    LineLayoutItem lastChild() const
    {
        return LineLayoutItem(toInline()->lastChild());
    }

    LayoutUnit marginStart() const
    {
        return toInline()->marginStart();
    }

    LayoutUnit marginEnd() const
    {
        return toInline()->marginEnd();
    }

    int borderStart() const
    {
        return toInline()->borderStart();
    }

    int borderEnd() const
    {
        return toInline()->borderEnd();
    }

    LayoutUnit paddingStart() const
    {
        return toInline()->paddingStart();
    }

    LayoutUnit paddingEnd() const
    {
        return toInline()->paddingEnd();
    }

    bool hasInlineDirectionBordersPaddingOrMargin() const
    {
        return toInline()->hasInlineDirectionBordersPaddingOrMargin();
    }

    bool alwaysCreateLineBoxes() const
    {
        return toInline()->alwaysCreateLineBoxes();
    }

    InlineBox* firstLineBoxIncludingCulling() const
    {
        return toInline()->firstLineBoxIncludingCulling();
    }

protected:
    LayoutInline* toInline()
    {
        return toLayoutInline(layoutObject());
    }

    const LayoutInline* toInline() const
    {
        return toLayoutInline(layoutObject());
    }

};

} // namespace blink

#endif // LineLayoutInline_h
