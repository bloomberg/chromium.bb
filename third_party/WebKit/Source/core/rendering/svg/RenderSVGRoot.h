/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderSVGRoot_h
#define RenderSVGRoot_h

#include "core/rendering/RenderReplaced.h"

namespace blink {

class SVGElement;

class RenderSVGRoot final : public RenderReplaced {
public:
    explicit RenderSVGRoot(SVGElement*);
    virtual ~RenderSVGRoot();
    virtual void trace(Visitor*) override;

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    virtual void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const override;

    // If you have a RenderSVGRoot, use firstChild or lastChild instead.
    void slowFirstChild() const WTF_DELETED_FUNCTION;
    void slowLastChild() const WTF_DELETED_FUNCTION;

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    virtual void setNeedsBoundariesUpdate() override { m_needsBoundariesOrTransformUpdate = true; }
    virtual void setNeedsTransformUpdate() override { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize)
    {
        // SVGImage::draw() does a view layout prior to painting,
        // and we need that layout to know of the new size otherwise
        // the rendering may be incorrectly using the old size.
        if (m_containerSize != containerSize)
            setNeedsLayoutAndFullPaintInvalidation();
        m_containerSize = containerSize;
    }

    // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS box coordinates.
    const AffineTransform& localToBorderBoxTransform() const { return m_localToBorderBoxTransform; }
    bool shouldApplyViewportClip() const;

private:
    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    virtual RenderObjectChildList* virtualChildren() override { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const override { return children(); }

    virtual const char* renderName() const override { return "RenderSVGRoot"; }
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectSVG || type == RenderObjectSVGRoot || RenderReplaced::isOfType(type); }

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const override;
    virtual LayoutUnit computeReplacedLogicalHeight() const override;
    virtual void layout() override;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) override;

    virtual void willBeDestroyed() override;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const override;
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override;
    virtual void removeChild(RenderObject*) override;
    virtual bool canHaveWhitespaceChildren() const override { return false; }

    virtual void insertedIntoTree() override;
    virtual void willBeRemovedFromTree() override;

    virtual const AffineTransform& localToParentTransform() const override;

    virtual FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    virtual FloatRect strokeBoundingBox() const override { return m_strokeBoundingBox; }
    virtual FloatRect paintInvalidationRectInLocalCoordinates() const override { return m_paintInvalidationBoundingBox; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* = 0) const override;
    virtual void computeFloatRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, FloatRect& paintInvalidationRect, const PaintInvalidationState* = 0) const override;

    virtual void mapLocalToContainer(const RenderLayerModelObject* paintInvalidationContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0, const PaintInvalidationState* = 0) const override;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

    virtual bool canBeSelectionLeaf() const override { return false; }
    virtual bool canHaveChildren() const override { return true; }

    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    RenderObjectChildList m_children;
    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_paintInvalidationBoundingBox;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
    bool m_hasBoxDecorationBackground : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGRoot, isSVGRoot());

} // namespace blink

#endif // RenderSVGRoot_h
