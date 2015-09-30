// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutSVGInlineText_h
#define LineLayoutSVGInlineText_h

#include "core/layout/api/LineLayoutText.h"
#include "core/layout/svg/LayoutSVGInlineText.h"

namespace blink {

class LineLayoutSVGInlineText : public LineLayoutText {
public:
    explicit LineLayoutSVGInlineText(LayoutSVGInlineText* layoutSVGInlineText)
        : LineLayoutText(layoutSVGInlineText)
    {
    }

    explicit LineLayoutSVGInlineText(const LineLayoutItem& item)
        : LineLayoutText(item)
    {
        ASSERT(!item || item.isSVGInlineText());
    }

    LineLayoutSVGInlineText() { }

    bool characterStartsNewTextChunk(int position) const
    {
        return toSVGInlineText()->characterStartsNewTextChunk(position);
    }

    float scalingFactor() const
    {
        return toSVGInlineText()->scalingFactor();
    }

    const Font& scaledFont() const
    {
        return toSVGInlineText()->scaledFont();
    }


private:
    LayoutSVGInlineText* toSVGInlineText()
    {
        return toLayoutSVGInlineText(layoutObject());
    }

    const LayoutSVGInlineText* toSVGInlineText() const
    {
        return toLayoutSVGInlineText(layoutObject());
    }
};

} // namespace blink

#endif // LineLayoutSVGInlineText_h
