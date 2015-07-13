/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
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

#ifndef LayoutSVGForeignObject_h
#define LayoutSVGForeignObject_h

#include "core/layout/svg/LayoutSVGBlock.h"

namespace blink {

class SVGForeignObjectElement;

class LayoutSVGForeignObject final : public LayoutSVGBlock {
public:
    explicit LayoutSVGForeignObject(SVGForeignObjectElement*);
    ~LayoutSVGForeignObject() override;

    const char* name() const override { return "LayoutSVGForeignObject"; }

    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    void paint(const PaintInfo&, const LayoutPoint&) override;

    void layout() override;

    FloatRect objectBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect strokeBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect paintInvalidationRectInLocalCoordinates() const override { return FloatRect(FloatPoint(), m_viewport.size()); }

    bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGForeignObject || LayoutSVGBlock::isOfType(type); }

    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    FloatRect viewportRect() { return m_viewport; }

private:
    void updateLogicalWidth() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    const AffineTransform& localToParentTransform() const override;

    bool m_needsTransformUpdate : 1;
    FloatRect m_viewport;
    mutable AffineTransform m_localToParentTransform;
};

}

#endif
