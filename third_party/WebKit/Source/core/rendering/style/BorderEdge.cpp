// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/style/BorderEdge.h"

namespace blink {

BorderEdge::BorderEdge(int edgeWidth, const Color& edgeColor, EBorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent)
    : width(edgeWidth)
    , color(edgeColor)
    , isTransparent(edgeIsTransparent)
    , isPresent(edgeIsPresent)
    , style(edgeStyle)
{
    if (style == DOUBLE && edgeWidth < 3)
        style = SOLID;
}

BorderEdge::BorderEdge()
    : width(0)
    , isTransparent(false)
    , isPresent(false)
    , style(BHIDDEN)
{
}

bool BorderEdge::hasVisibleColorAndStyle() const
{
    return style > BHIDDEN && !isTransparent;
}

bool BorderEdge::shouldRender() const { return isPresent && width && hasVisibleColorAndStyle(); }
bool BorderEdge::presentButInvisible() const { return usedWidth() && !hasVisibleColorAndStyle(); }
bool BorderEdge::obscuresBackgroundEdge(float scale) const
{
    if (!isPresent || isTransparent || (width * scale) < 2 || color.hasAlpha() || style == BHIDDEN)
        return false;

    if (style == DOTTED || style == DASHED)
        return false;

    if (style == DOUBLE)
        return width >= 5 * scale; // The outer band needs to be >= 2px wide at unit scale.

    return true;
}
bool BorderEdge::obscuresBackground() const
{
    if (!isPresent || isTransparent || color.hasAlpha() || style == BHIDDEN)
        return false;

    if (style == DOTTED || style == DASHED || style == DOUBLE)
        return false;

    return true;
}

int BorderEdge::usedWidth() const
{
    return isPresent ? width : 0;
}

void BorderEdge::getDoubleBorderStripeWidths(int& outerWidth, int& innerWidth) const
{
    int fullWidth = usedWidth();
    outerWidth = fullWidth / 3;
    innerWidth = fullWidth * 2 / 3;

    // We need certain integer rounding results
    if (fullWidth % 3 == 2)
        outerWidth += 1;

    if (fullWidth % 3 == 1)
        innerWidth += 1;
}

bool BorderEdge::sharesColorWith(const BorderEdge& other) const
{
    return color == other.color;
}

} // namespace blink
