/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderInline_h
#define RenderInline_h

#include "core/editing/PositionWithAffinity.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLineBoxList.h"

namespace blink {

class RenderInline : public RenderBoxModelObject {
public:
    explicit RenderInline(Element*);

    static RenderInline* createAnonymous(Document*);

    LayoutObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a RenderInline, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    virtual void addChild(LayoutObject* newChild, LayoutObject* beforeChild = 0) override;

    Element* node() const { return toElement(RenderBoxModelObject::node()); }

    virtual LayoutRectOutsets marginBoxOutsets() const override final;
    virtual LayoutUnit marginLeft() const override final;
    virtual LayoutUnit marginRight() const override final;
    virtual LayoutUnit marginTop() const override final;
    virtual LayoutUnit marginBottom() const override final;
    virtual LayoutUnit marginBefore(const LayoutStyle* otherStyle = 0) const override final;
    virtual LayoutUnit marginAfter(const LayoutStyle* otherStyle = 0) const override final;
    virtual LayoutUnit marginStart(const LayoutStyle* otherStyle = 0) const override final;
    virtual LayoutUnit marginEnd(const LayoutStyle* otherStyle = 0) const override final;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override final;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    virtual LayoutSize offsetFromContainer(const LayoutObject*, const LayoutPoint&, bool* offsetDependsOnPoint = 0) const override final;

    IntRect linesBoundingBox() const;
    LayoutRect linesVisualOverflowBoundingBox() const;

    InlineFlowBox* createAndAppendInlineFlowBox();

    void dirtyLineBoxes(bool fullLayout);

    RenderLineBoxList* lineBoxes() { return &m_lineBoxes; }
    const RenderLineBoxList* lineBoxes() const { return &m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }
    InlineBox* firstLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? firstLineBox() : culledInlineFirstLineBox(); }
    InlineBox* lastLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? lastLineBox() : culledInlineLastLineBox(); }

    virtual RenderBoxModelObject* virtualContinuation() const override final { return continuation(); }
    RenderInline* inlineElementContinuation() const;

    virtual void updateDragState(bool dragOn) override final;

    LayoutSize offsetForInFlowPositionedInline(const RenderBox& child) const;

    virtual void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset) const override final;

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    bool alwaysCreateLineBoxes() const { return alwaysCreateLineBoxesForRenderInline(); }
    void setAlwaysCreateLineBoxes(bool alwaysCreateLineBoxes = true) { setAlwaysCreateLineBoxesForRenderInline(alwaysCreateLineBoxes); }
    void updateAlwaysCreateLineBoxes(bool fullLayout);

    virtual LayoutRect localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine) override final;

    bool hitTestCulledInline(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

protected:
    virtual void willBeDestroyed() override;

    virtual void styleDidChange(StyleDifference, const LayoutStyle* oldStyle) override;

    virtual void computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const override;

    virtual void invalidateDisplayItemClients(DisplayItemList*) const override;

private:
    virtual LayoutObjectChildList* virtualChildren() override final { return children(); }
    virtual const LayoutObjectChildList* virtualChildren() const override final { return children(); }
    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    virtual const char* renderName() const override;

    virtual bool isRenderInline() const override final { return true; }

    LayoutRect culledInlineVisualOverflowBoundingBox() const;
    InlineBox* culledInlineFirstLineBox() const;
    InlineBox* culledInlineLastLineBox() const;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;
    template<typename GeneratorContext>
    void generateCulledLineBoxRects(GeneratorContext& yield, const RenderInline* container) const;

    void addChildToContinuation(LayoutObject* newChild, LayoutObject* beforeChild);
    virtual void addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild = 0) override final;

    void moveChildrenToIgnoringContinuation(RenderInline* to, LayoutObject* startChild);

    void splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
        LayoutObject* beforeChild, RenderBoxModelObject* oldCont);
    void splitFlow(LayoutObject* beforeChild, RenderBlock* newBlockBox,
        LayoutObject* newChild, RenderBoxModelObject* oldCont);

    virtual void layout() override final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    virtual void paint(const PaintInfo&, const LayoutPoint&) override final;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override final;

    virtual LayerType layerTypeRequired() const override { return isRelPositioned() || createsGroup() || hasClipPath() || style()->shouldCompositeForCurrentAnimations() ? NormalLayer : NoLayer; }

    virtual LayoutUnit offsetLeft() const override final;
    virtual LayoutUnit offsetTop() const override final;
    virtual LayoutUnit offsetWidth() const override final { return linesBoundingBox().width(); }
    virtual LayoutUnit offsetHeight() const override final { return linesBoundingBox().height(); }

    virtual LayoutRect absoluteClippedOverflowRect() const override;
    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* = 0) const override;
    virtual LayoutRect rectWithOutlineForPaintInvalidation(const LayoutLayerModelObject* paintInvalidationContainer, LayoutUnit outlineWidth, const PaintInvalidationState* = 0) const override final;
    virtual void mapRectToPaintInvalidationBacking(const LayoutLayerModelObject* paintInvalidationContainer, LayoutRect&, const PaintInvalidationState*) const override final;

    // This method differs from clippedOverflowRectForPaintInvalidation in that it includes
    // the rects for culled inline boxes, which aren't necessary for paint invalidation.
    LayoutRect clippedOverflowRect(const LayoutLayerModelObject*, const PaintInvalidationState* = 0) const;

    virtual void mapLocalToContainer(const LayoutLayerModelObject* paintInvalidationContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0, const PaintInvalidationState* = 0) const override;

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) override final;

    virtual IntRect borderBoundingBox() const override final
    {
        IntRect boundingBox = linesBoundingBox();
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

    virtual InlineFlowBox* createInlineFlowBox(); // Subclassed by SVG and Ruby

    virtual void dirtyLinesFromChangedChild(LayoutObject* child) override final { m_lineBoxes.dirtyLinesFromChangedChild(this, child); }

    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override final;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override final;

    virtual void childBecameNonInline(LayoutObject* child) override final;

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override final;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override final;

    virtual void addAnnotatedRegions(Vector<AnnotatedRegionValue>&) override final;

    virtual void updateFromStyle() override final;

    RenderInline* clone() const;

    RenderBoxModelObject* continuationBefore(LayoutObject* beforeChild);

    LayoutObjectChildList m_children;
    RenderLineBoxList m_lineBoxes;   // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(RenderInline, isRenderInline());

} // namespace blink

#endif // RenderInline_h
