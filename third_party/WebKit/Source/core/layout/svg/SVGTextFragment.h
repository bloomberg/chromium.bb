/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef SVGTextFragment_h
#define SVGTextFragment_h

#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"

namespace blink {

// A SVGTextFragment describes a text fragment of a LayoutSVGInlineText which can be laid out at once.
struct SVGTextFragment {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    SVGTextFragment()
        : characterOffset(0)
        , metricsListOffset(0)
        , length(0)
        , isTextOnPath(false)
        , x(0)
        , y(0)
        , width(0)
        , height(0)
    {
    }

    enum TransformType {
        TransformRespectingTextLength,
        TransformIgnoringTextLength
    };

    FloatRect boundingBox(float baseline) const
    {
        FloatRect fragmentRect(x, y - baseline, width, height);
        if (!isTransformed())
            return fragmentRect;
        return buildNormalFragmentTransform().mapRect(fragmentRect);
    }

    FloatRect overflowBoundingBox(float baseline) const
    {
        FloatRect fragmentRect(
            x - glyphOverflowLeft,
            y - baseline - glyphOverflowTop,
            width + glyphOverflowLeft + glyphOverflowRight,
            height + glyphOverflowTop + glyphOverflowBottom);
        if (!isTransformed())
            return fragmentRect;
        return buildNormalFragmentTransform().mapRect(fragmentRect);
    }

    FloatQuad boundingQuad(float baseline) const
    {
        FloatQuad fragmentQuad(FloatRect(x, y - baseline, width, height));
        if (!isTransformed())
            return fragmentQuad;
        return buildNormalFragmentTransform().mapQuad(fragmentQuad);
    }

    AffineTransform buildFragmentTransform(TransformType type = TransformRespectingTextLength) const
    {
        if (type == TransformIgnoringTextLength) {
            AffineTransform result = transform;
            transformAroundOrigin(result);
            return result;
        }
        return buildNormalFragmentTransform();
    }

    bool isTransformed() const { return affectedByTextLength() || !transform.isIdentity(); }

    // The first laid out character starts at LayoutSVGInlineText::characters() + characterOffset.
    unsigned characterOffset;
    unsigned metricsListOffset;
    unsigned length : 31;
    unsigned isTextOnPath : 1;

    float x;
    float y;
    float width;
    float height;

    // Top and bottom are the amounts of glyph overflows exceeding the font metrics' ascent and descent, respectively.
    float glyphOverflowTop;
    float glyphOverflowBottom;
    // Left and right are the amounts of glyph overflows exceeding the left and right edge of normal layout boundary, respectively.
    float glyphOverflowLeft;
    float glyphOverflowRight;

    // Includes rotation/glyph-orientation-(horizontal|vertical) transforms, as well as orientation related shifts
    // (see SVGTextLayoutEngine, which builds this transformation).
    AffineTransform transform;

    // Contains lengthAdjust related transformations, which are not allowd to influence the SVGTextQuery code.
    AffineTransform lengthAdjustTransform;

private:
    AffineTransform buildNormalFragmentTransform() const
    {
        if (isTextOnPath)
            return buildTransformForTextOnPath();
        return buildTransformForTextOnLine();
    }

    bool affectedByTextLength() const { return lengthAdjustTransform.a() != 1 || lengthAdjustTransform.d() != 1; }

    void transformAroundOrigin(AffineTransform& result) const
    {
        // Returns (translate(x, y) * result) * translate(-x, -y).
        result.setE(result.e() + x);
        result.setF(result.f() + y);
        result.translate(-x, -y);
    }

    AffineTransform buildTransformForTextOnPath() const
    {
        // For text-on-path layout, multiply the transform with the lengthAdjustTransform before orienting the resulting transform.
        AffineTransform result = !affectedByTextLength() ? transform : transform * lengthAdjustTransform;
        if (!result.isIdentity())
            transformAroundOrigin(result);
        return result;
    }

    AffineTransform buildTransformForTextOnLine() const
    {
        // For text-on-line layout, orient the transform first, then multiply the lengthAdjustTransform with the oriented transform.
        if (transform.isIdentity())
            return lengthAdjustTransform;

        AffineTransform result = transform;
        transformAroundOrigin(result);
        result.preMultiply(lengthAdjustTransform);
        return result;
    }
};

} // namespace blink

#endif
