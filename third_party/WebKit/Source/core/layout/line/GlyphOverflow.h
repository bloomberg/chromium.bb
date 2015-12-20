/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef GlyphOverflow_h
#define GlyphOverflow_h

#include "platform/geometry/FloatRect.h"
#include "wtf/Allocator.h"
#include <algorithm>

namespace blink {

struct GlyphOverflow {
    STACK_ALLOCATED();
    GlyphOverflow()
        : left(0)
        , right(0)
        , top(0)
        , bottom(0)
    {
    }

    bool isZero() const
    {
        return !left && !right && !top && !bottom;
    }

    // These helper functions are exposed to share logic with SVG which computes overflow
    // in floating point (see: SVGTextLayoutEngine).
    static float topOverflow(const FloatRect& bounds, float ascent)
    {
        return std::max(0.0f, -bounds.y() - ascent);
    }
    static float bottomOverflow(const FloatRect& bounds, float descent)
    {
        return std::max(0.0f, bounds.maxY() - descent);
    }
    static float leftOverflow(const FloatRect& bounds)
    {
        return std::max(0.0f, -bounds.x());
    }
    static float rightOverflow(const FloatRect& bounds, float textWidth)
    {
        return std::max(0.0f, bounds.maxX() - textWidth);
    }

    void setFromBounds(const FloatRect& bounds, float ascent, float descent, float textWidth)
    {
        top = ceilf(topOverflow(bounds, ascent));
        bottom = ceilf(bottomOverflow(bounds, descent));
        left = ceilf(leftOverflow(bounds));
        right = ceilf(rightOverflow(bounds, textWidth));
    }

    // Top and bottom are the amounts of glyph overflows exceeding the font metrics' ascent and descent, respectively.
    // Left and right are the amounts of glyph overflows exceeding the left and right edge of normal layout boundary, respectively.
    // All fields are in absolute number of pixels rounded up to the nearest integer.
    int left;
    int right;
    int top;
    int bottom;
};

} // namespace blink

#endif // GlyphOverflow_h
