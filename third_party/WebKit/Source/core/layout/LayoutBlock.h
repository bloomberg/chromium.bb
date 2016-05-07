/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef LayoutBlock_h
#define LayoutBlock_h

#include "core/CoreExport.h"
#include "core/layout/FloatingObjects.h"
#include "core/layout/GapRects.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/LineBoxList.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/style/ShapeValue.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/ListHashSet.h"
#include "wtf/OwnPtr.h"

namespace blink {

class LineLayoutState;
struct PaintInfo;
class LayoutInline;
class WordMeasurement;

typedef WTF::ListHashSet<LayoutBox*, 16> TrackedLayoutBoxListHashSet;
typedef WTF::HashMap<const LayoutBlock*, OwnPtr<TrackedLayoutBoxListHashSet>> TrackedDescendantsMap;
typedef WTF::HashMap<const LayoutBox*, LayoutBlock*> TrackedContainerMap;
typedef Vector<WordMeasurement, 64> WordMeasurements;

enum ContainingBlockState { NewContainingBlock, SameContainingBlock };

// LayoutBlock is the class that is used by any LayoutObject
// that is a containing block.
// http://www.w3.org/TR/CSS2/visuren.html#containing-block
// See also LayoutObject::containingBlock() that is the function
// used to get the containing block of a LayoutObject.
//
// CSS is inconsistent and allows inline elements (LayoutInline) to be
// containing blocks, even though they are not blocks. Our
// implementation is as confused with inlines. See e.g.
// LayoutObject::containingBlock() vs LayoutObject::container().
//
// Containing blocks are a central concept for layout, in
// particular to the layout of out-of-flow positioned
// elements. They are used to determine the sizing as well
// as the positioning of the LayoutObjects.
//
// LayoutBlock is the class that handles out-of-flow positioned elements in
// Blink, in particular for layout (see layoutPositionedObjects()). That's why
// LayoutBlock keeps track of them through |gPositionedDescendantsMap| (see
// LayoutBlock.cpp).
// Note that this is a design decision made in Blink that doesn't reflect CSS:
// CSS allows relatively positioned inlines (LayoutInline) to be containing
// blocks, but they don't have the logic to handle out-of-flow positioned
// objects. This induces some complexity around choosing an enclosing
// LayoutBlock (for inserting out-of-flow objects during layout) vs the CSS
// containing block (for sizing, invalidation).
//
//
// ***** WHO LAYS OUT OUT-OF-FLOW POSITIONED OBJECTS? *****
// A positioned object gets inserted into an enclosing LayoutBlock's positioned
// map. This is determined by LayoutObject::containingBlock().
//
//
// ***** HANDLING OUT-OF-FLOW POSITIONED OBJECTS *****
// Care should be taken to handle out-of-flow positioned objects during
// certain tree walks (e.g. layout()). The rule is that anything that
// cares about containing blocks should skip the out-of-flow elements
// in the normal tree walk and do an optional follow-up pass for them
// using LayoutBlock::positionedObjects().
// Not doing so will result in passing the wrong containing
// block as tree walks will always pass the parent as the
// containing block.
//
// Sample code of how to handle positioned objects in LayoutBlock:
//
// for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
//     if (child->isOutOfFlowPositioned())
//         continue;
//
//     // Handle normal flow children.
//     ...
// }
// for (LayoutBox* positionedObject : positionedObjects()) {
//     // Handle out-of-flow positioned objects.
//     ...
// }
class CORE_EXPORT LayoutBlock : public LayoutBox {
public:
    friend class LineLayoutState;

protected:
    explicit LayoutBlock(ContainerNode*);
    ~LayoutBlock() override;

public:
    LayoutObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a LayoutBlock, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    bool beingDestroyed() const { return m_beingDestroyed; }

    // These two functions are overridden for inline-block.
    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    LayoutUnit minLineHeightForReplacedObject(bool isFirstLine, LayoutUnit replacedHeight) const;

    bool createsNewFormattingContext() const;

    const LineBoxList& lineBoxes() const { return m_lineBoxes; }
    LineBoxList* lineBoxes() { return &m_lineBoxes; }

    const char* name() const override;

protected:
    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }

    RootInlineBox* firstRootBox() const { return static_cast<RootInlineBox*>(firstLineBox()); }
    RootInlineBox* lastRootBox() const { return static_cast<RootInlineBox*>(lastLineBox()); }

public:
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to LayoutBlockFlow
    virtual void deleteLineBoxTree();

    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;
    void removeChild(LayoutObject*) override;

    virtual void layoutBlock(bool relayoutChildren);

    void insertPositionedObject(LayoutBox*);
    static void removePositionedObject(LayoutBox*);
    void removePositionedObjects(LayoutBlock*, ContainingBlockState = SameContainingBlock);

    TrackedLayoutBoxListHashSet* positionedObjects() const { return hasPositionedObjects() ? positionedObjectsInternal() : nullptr; }
    bool hasPositionedObjects() const
    {
        ASSERT(m_hasPositionedObjects ? (positionedObjectsInternal() && !positionedObjectsInternal()->isEmpty()) : !positionedObjectsInternal());
        return m_hasPositionedObjects;
    }

    void addPercentHeightDescendant(LayoutBox*);
    void removePercentHeightDescendant(LayoutBox*);
    bool hasPercentHeightDescendant(LayoutBox* o) const { return hasPercentHeightDescendants() && percentHeightDescendantsInternal()->contains(o); }

    TrackedLayoutBoxListHashSet* percentHeightDescendants() const { return hasPercentHeightDescendants() ? percentHeightDescendantsInternal() : nullptr; }
    bool hasPercentHeightDescendants() const
    {
        ASSERT(m_hasPercentHeightDescendants ? (percentHeightDescendantsInternal() && !percentHeightDescendantsInternal()->isEmpty()) : !percentHeightDescendantsInternal());
        return m_hasPercentHeightDescendants;
    }

    void notifyScrollbarThicknessChanged() { m_widthAvailableToChildrenChanged = true; }

    void setHasMarkupTruncation(bool b) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
    void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }

    bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
    bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }

    bool hasMarginBeforeQuirk(const LayoutBox* child) const;
    bool hasMarginAfterQuirk(const LayoutBox* child) const;

    void markPositionedObjectsForLayout();
    // FIXME: Do we really need this to be virtual? It's just so we can call this on
    // LayoutBoxes without needed to check whether they're LayoutBlocks first.
    void markForPaginationRelayoutIfNeeded(SubtreeLayoutScope&) final;

    LayoutUnit textIndentOffset() const;

    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    LayoutUnit blockDirectionOffset(const LayoutSize& offsetFromBlock) const;
    LayoutUnit inlineDirectionOffset(const LayoutSize& offsetFromBlock) const;

    LayoutBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const;

    void setSelectionState(SelectionState) override;

    LayoutRect logicalRectToPhysicalRect(const LayoutPoint& physicalPosition, const LayoutRect& logicalRect) const;

    LayoutBoxModelObject* virtualContinuation() const final { return continuation(); }
    bool isAnonymousBlockContinuation() const { return continuation() && isAnonymousBlock(); }
    LayoutInline* inlineElementContinuation() const;

    using LayoutBoxModelObject::continuation;
    using LayoutBoxModelObject::setContinuation;

    static LayoutBlock* createAnonymousWithParentAndDisplay(const LayoutObject*, EDisplay = BLOCK);
    LayoutBlock* createAnonymousBlock(EDisplay display = BLOCK) const { return createAnonymousWithParentAndDisplay(this, display); }

    LayoutBox* createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const override;

    int columnGap() const;

    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    LayoutUnit logicalWidthForChild(const LayoutBox& child) const { return logicalWidthForChildSize(child.size()); }
    LayoutUnit logicalWidthForChildSize(LayoutSize childSize) const { return isHorizontalWritingMode() ? childSize.width() : childSize.height(); }
    LayoutUnit logicalHeightForChild(const LayoutBox& child) const { return isHorizontalWritingMode() ? child.size().height() : child.size().width(); }
    LayoutSize logicalSizeForChild(const LayoutBox& child) const { return isHorizontalWritingMode() ? child.size() : child.size().transposedSize(); }
    LayoutUnit logicalTopForChild(const LayoutBox& child) const { return isHorizontalWritingMode() ? child.location().y() : child.location().x(); }
    LayoutUnit marginBeforeForChild(const LayoutBoxModelObject& child) const { return child.marginBefore(style()); }
    LayoutUnit marginAfterForChild(const LayoutBoxModelObject& child) const { return child.marginAfter(style()); }
    LayoutUnit marginStartForChild(const LayoutBoxModelObject& child) const { return child.marginStart(style()); }
    LayoutUnit marginEndForChild(const LayoutBoxModelObject& child) const { return child.marginEnd(style()); }
    void setMarginStartForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginStart(value, style()); }
    void setMarginEndForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginEnd(value, style()); }
    void setMarginBeforeForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginBefore(value, style()); }
    void setMarginAfterForChild(LayoutBox& child, LayoutUnit value) const { child.setMarginAfter(value, style()); }
    LayoutUnit collapsedMarginBeforeForChild(const LayoutBox& child) const;
    LayoutUnit collapsedMarginAfterForChild(const LayoutBox& child) const;

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    virtual void scrollbarsChanged(bool /*horizontalScrollbarChanged*/, bool /*verticalScrollbarChanged*/);

    LayoutUnit availableLogicalWidthForContent() const { return (logicalRightOffsetForContent() - logicalLeftOffsetForContent()).clampNegativeToZero(); }
    LayoutUnit logicalLeftOffsetForContent() const { return isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }
    LayoutUnit logicalRightOffsetForContent() const { return logicalLeftOffsetForContent() + availableLogicalWidth(); }
    LayoutUnit startOffsetForContent() const { return style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }
    LayoutUnit endOffsetForContent() const { return !style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }

    virtual LayoutUnit logicalLeftSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const;
    virtual LayoutUnit logicalRightSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const;

#if ENABLE(ASSERT)
    void checkPositionedObjectsNeedLayout();
#endif

protected:
    bool recalcNormalFlowChildOverflowIfNeeded(LayoutObject*);
public:
    virtual bool recalcChildOverflowAfterStyleChange();
    bool recalcOverflowAfterStyleChange();

    // An example explaining layout tree structure about first-line style:
    // <style>
    //   #enclosingFirstLineStyleBlock::first-line { ... }
    // </style>
    // <div id="enclosingFirstLineStyleBlock">
    //   <div>
    //     <div id="nearestInnerBlockWithFirstLine">
    //       [<span>]first line text[</span>]
    //     </div>
    //   </div>
    // </div>

    // Returns the nearest enclosing block (including this block) that contributes a first-line style to our first line.
    LayoutBlock* enclosingFirstLineStyleBlock() const;
    // Returns this block or the nearest inner block containing the actual first line.
    LayoutBlockFlow* nearestInnerBlockWithFirstLine() const;

protected:
    void willBeDestroyed() override;

    void dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope&);

    void layout() override;

    enum PositionedLayoutBehavior {
        DefaultLayout,
        LayoutOnlyFixedPositionedObjects,
        ForcedLayoutAfterContainingBlockMoved
    };

    void layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior = DefaultLayout);
    void markFixedPositionObjectForLayoutIfNeeded(LayoutObject* child, SubtreeLayoutScope&);

    LayoutUnit marginIntrinsicLogicalWidthForChild(LayoutBox& child) const;

    int beforeMarginInLineDirection(LineDirectionMode) const;

    void paint(const PaintInfo&, const LayoutPoint&) const override;
public:
    virtual void paintObject(const PaintInfo&, const LayoutPoint&) const;
    virtual void paintChildren(const PaintInfo&, const LayoutPoint&) const;

    // FIXME-BLOCKFLOW: Remove virtualization when all callers have moved to LayoutBlockFlow
    virtual void paintFloats(const PaintInfo&, const LayoutPoint&) const { }

protected:
    virtual void adjustInlineDirectionLineBounds(unsigned /* expansionOpportunityCount */, LayoutUnit& /* logicalLeft */, LayoutUnit& /* logicalWidth */) const { }

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;
    void computeChildPreferredLogicalWidths(LayoutObject& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const;

    int firstLineBoxBaseline() const override;
    int inlineBlockBaseline(LineDirectionMode) const override;

    // This function disables the 'overflow' check in inlineBlockBaseline.
    // For 'inline-block', CSS says that the baseline is the bottom margin edge
    // if 'overflow' is not visible. But some descendant classes want to ignore
    // this condition.
    virtual bool shouldIgnoreOverflowPropertyForInlineBlockBaseline() const { return false; }

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    // Delay update scrollbar until finishDelayUpdateScrollInfo() will be
    // called. This function is used when a flexbox is laying out its
    // descendant. If multiple calls are made to startDelayUpdateScrollInfo(),
    // finishDelayUpdateScrollInfo() will do nothing until finishDelayUpdateScrollInfo()
    // is called the same number of times.
    // finishDelayUpdateScrollInfo returns true when it marked something for layout.
    // It will also return a map of saved scroll positions that the caller should restore
    // on the given scrollable areas after performing the layout.
    // This can be necessary because Flexbox's multi-pass layout can lose the scroll position.
    // TODO(cbiesinger): This is a temporary hack. The right solution is to delay the scroll
    // clamping that currently happens in PaintLayerScrollableArea::updateAfterLayout to only
    // happen after all layout is done, i.e. during updateLayerPositionsAfterLayout. However,
    // that currently fails a layout test. To fix this bug in time for M50, we use this temporary
    // hack. The real fix is tracked in crbug.com/600036
    typedef PersistentHeapHashMap<Member<PaintLayerScrollableArea>, DoublePoint> ScrollPositionMap;
    static void startDelayUpdateScrollInfo();
    static bool finishDelayUpdateScrollInfo(SubtreeLayoutScope*, ScrollPositionMap*);

    void updateAfterLayout();

    void styleWillChange(StyleDifference, const ComputedStyle& newStyle) override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void updateFromStyle() override;

    // Returns true if non-visible overflow should be respected. Otherwise hasOverflowClip() will be
    // false and we won't create scrollable area for this object even if overflow is non-visible.
    virtual bool allowsOverflowClip() const;

    virtual bool hasLineIfEmpty() const;

    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

public:
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool = false);
protected:
    virtual void addOverflowFromChildren();
    void addOverflowFromPositionedObjects();
    void addOverflowFromBlockChildren();
    void addVisualOverflowFromTheme();

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const override;

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox&);

    // TODO(jchaffraix): We should rename this function as inline-flex and inline-grid as also covered.
    // Alternatively it should be removed as we clarify the meaning of isAtomicInlineLevel to imply
    // isInline.
    bool isInlineBlockOrInlineTable() const final { return isInline() && isAtomicInlineLevel(); }

    void invalidateDisplayItemClients(const LayoutBoxModelObject& paintInvalidationContainer, PaintInvalidationReason) const override;

private:
    LayoutObjectChildList* virtualChildren() final { return children(); }
    const LayoutObjectChildList* virtualChildren() const final { return children(); }

    bool isLayoutBlock() const final { return true; }

    void makeChildrenNonInline(LayoutObject* insertionPoint = nullptr);

    virtual void removeLeftoverAnonymousBlock(LayoutBlock* child);

    void makeChildrenInlineIfPossible();

    void dirtyLinesFromChangedChild(LayoutObject* child) final { m_lineBoxes.dirtyLinesFromChangedChild(LineLayoutItem(this), LineLayoutItem(child)); }

    void addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild) override;

    TrackedLayoutBoxListHashSet* positionedObjectsInternal() const;
    TrackedLayoutBoxListHashSet* percentHeightDescendantsInternal() const;

    Node* nodeForHitTest() const final;

    // Returns true if the positioned movement-only layout succeeded.
    bool tryLayoutDoingPositionedMovementOnly();

    bool avoidsFloats() const override { return true; }

    bool isInSelfHitTestingPhase(HitTestAction hitTestAction) const final
    {
        return hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground;
    }
    bool hitTestChildren(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to LayoutBlockFlow
    virtual bool hitTestFloats(HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }

    bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) const;

    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    LayoutObject* hoverAncestor() const final;
    void updateDragState(bool dragOn) final;
    void childBecameNonInline(LayoutObject* child) final;

    bool isSelectionRoot() const;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&) const override;

public:
    bool hasCursorCaret() const;
    bool hasDragCaret() const;
    bool hasCaret() const { return hasCursorCaret() || hasDragCaret(); }

private:
    LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = nullptr) final;
    bool isInlineBoxWrapperActuallyChild() const;

    Position positionForBox(InlineBox*, bool start = true) const;

    // End helper functions and structs used by layoutBlockChildren.

    void removeFromGlobalMaps();
    bool widthAvailableToChildrenHasChanged();

public:
    // Specify which page or column to associate with an offset, if said offset is exactly at a page
    // or column boundary.
    enum PageBoundaryRule { AssociateWithFormerPage, AssociateWithLatterPage };

    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule) const;

    // Calculate the strut to insert in order fit content of size |contentLogicalHeight|.
    // |strutToNextPage| is the strut to add to |offset| to merely get to the top of the next page
    // or column. This is what will be returned if the content can actually fit there. Otherwise,
    // return the distance to the next fragmentainer that can fit this piece of content.
    LayoutUnit calculatePaginationStrutToFitContent(LayoutUnit offset, LayoutUnit strutToNextPage, LayoutUnit contentLogicalHeight) const;

    static void collapseAnonymousBlockChild(LayoutBlock* parent, LayoutBlock* child);
protected:
    bool isPageLogicalHeightKnown(LayoutUnit logicalOffset) const { return pageLogicalHeightForOffset(logicalOffset); }

    // Returns the logical offset at the top of the next page, for a given offset.
    //
    // If the given offset is at a page boundary, using AssociateWithLatterPage as PageBoundaryRule
    // will move us one page ahead (since the offset is at the top of the "current" page). Using
    // AssociateWithFormerPage instead will keep us where we are (since the offset is at the bottom
    // of the "current" page, which is exactly the same offset as the top offset on the next page).
    //
    // For a page height of 800px, AssociateWithLatterPage will return 1600 if the value passed in
    // is 800. AssociateWithFormerPage will simply return 800.
    LayoutUnit nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule) const;

    // Paginated content inside this block was laid out.
    // |logicalBottomOffsetAfterPagination| is the logical bottom offset of the child content after
    // applying any forced or unforced breaks as needed.
    void paginatedContentWasLaidOut(LayoutUnit logicalBottomOffsetAfterPagination);

    // Adjust from painting offsets to the local coords of this layoutObject
    void offsetForContents(LayoutPoint&) const;

    PositionWithAffinity positionForPointRespectingEditingBoundaries(LineLayoutBox child, const LayoutPoint& pointInParentCoordinates);
    PositionWithAffinity positionForPointIfOutsideAtomicInlineLevel(const LayoutPoint&);

    virtual bool updateLogicalWidthAndColumnWidth();

    virtual bool canCollapseAnonymousBlockChild() const { return true; }

    LayoutObjectChildList m_children;
    LineBoxList m_lineBoxes; // All of the root line boxes created for this block flow.  For example, <div>Hello<br>world.</div> will have two total lines for the <div>.

    unsigned m_hasMarginBeforeQuirk : 1; // Note these quirk values can't be put in LayoutBlockRareData since they are set too frequently.
    unsigned m_hasMarginAfterQuirk : 1;
    unsigned m_beingDestroyed : 1;
    unsigned m_hasMarkupTruncation : 1;
    unsigned m_widthAvailableToChildrenChanged  : 1;
    unsigned m_heightAvailableToChildrenChanged  : 1;
    unsigned m_isSelfCollapsing : 1; // True if margin-before and margin-after are adjoining.
    unsigned m_descendantsWithFloatsMarkedForLayout : 1;

    unsigned m_hasPositionedObjects : 1;
    unsigned m_hasPercentHeightDescendants : 1;

    // LayoutRubyBase objects need to be able to split and merge, moving their children around
    // (calling moveChildTo, moveAllChildrenTo, and makeChildrenNonInline).
    friend class LayoutRubyBase;

    // FIXME: This is temporary as we move code that accesses block flow
    // member variables out of LayoutBlock and into LayoutBlockFlow.
    friend class LayoutBlockFlow;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBlock, isLayoutBlock());

} // namespace blink

#endif // LayoutBlock_h
