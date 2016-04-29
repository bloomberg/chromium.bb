/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "core/layout/LayoutBlock.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/DragCaretController.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMarqueeElement.h"
#include "core/layout/HitTestLocation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutDeprecatedFlexibleBox.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTextCombine.h"
#include "core/layout/LayoutTextControl.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/GlyphOverflow.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/shapes/ShapeOutsideInfo.h"
#include "core/page/Page.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/TransformState.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

using namespace HTMLNames;

struct SameSizeAsLayoutBlock : public LayoutBox {
    LayoutObjectChildList children;
    LineBoxList lineBoxes;
    uint32_t bitfields;
};

static_assert(sizeof(LayoutBlock) == sizeof(SameSizeAsLayoutBlock), "LayoutBlock should stay small");

// This map keeps track of the positioned objects associated with a containing
// block.
//
// This map is populated during layout. It is kept across layouts to handle
// that we skip unchanged sub-trees during layout, in such a way that we are
// able to lay out deeply nested out-of-flow descendants if their containing
// block got laid out. The map could be invalidated during style change but
// keeping track of containing blocks at that time is complicated (we are in
// the middle of recomputing the style so we can't rely on any of its
// information), which is why it's easier to just update it for every layout.
static TrackedDescendantsMap* gPositionedDescendantsMap = nullptr;
static TrackedContainerMap* gPositionedContainerMap = nullptr;

// This map keeps track of the descendants whose 'height' is percentage associated
// with a containing block. Like |gPositionedDescendantsMap|, it is also recomputed
// for every layout (see the comment above about why).
static TrackedDescendantsMap* gPercentHeightDescendantsMap = nullptr;

typedef WTF::HashSet<LayoutBlock*> DelayedUpdateScrollInfoSet;
static int gDelayUpdateScrollInfo = 0;
static DelayedUpdateScrollInfoSet* gDelayedUpdateScrollInfoSet = nullptr;

LayoutBlock::LayoutBlock(ContainerNode* node)
    : LayoutBox(node)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_beingDestroyed(false)
    , m_hasMarkupTruncation(false)
    , m_widthAvailableToChildrenChanged(false)
    , m_heightAvailableToChildrenChanged(false)
    , m_hasOnlySelfCollapsingChildren(false)
    , m_descendantsWithFloatsMarkedForLayout(false)
    , m_hasPositionedObjects(false)
    , m_hasPercentHeightDescendants(false)
{
    // LayoutBlockFlow calls setChildrenInline(true).
    // By default, subclasses do not have inline children.
}

void LayoutBlock::removeFromGlobalMaps()
{
    if (hasPositionedObjects()) {
        OwnPtr<TrackedLayoutBoxListHashSet> descendants = gPositionedDescendantsMap->take(this);
        ASSERT(!descendants->isEmpty());
        for (LayoutBox* descendant : *descendants) {
            ASSERT(gPositionedContainerMap->get(descendant) == this);
            gPositionedContainerMap->remove(descendant);
        }
    }
    if (hasPercentHeightDescendants()) {
        OwnPtr<TrackedLayoutBoxListHashSet> descendants = gPercentHeightDescendantsMap->take(this);
        ASSERT(!descendants->isEmpty());
        for (LayoutBox* descendant : *descendants) {
            ASSERT(descendant->percentHeightContainer() == this);
            descendant->setPercentHeightContainer(nullptr);
        }
    }
}

LayoutBlock::~LayoutBlock()
{
    removeFromGlobalMaps();
}

void LayoutBlock::willBeDestroyed()
{
    // Mark as being destroyed to avoid trouble with merges in removeChild().
    m_beingDestroyed = true;

    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from. Effects that do :before/:after only on hover could crash otherwise.
    children()->destroyLeftoverChildren();

    // Destroy our continuation before anything other than anonymous children.
    // The reason we don't destroy it before anonymous children is that they may
    // have continuations of their own that are anonymous children of our continuation.
    LayoutBoxModelObject* continuation = this->continuation();
    if (continuation) {
        continuation->destroy();
        setContinuation(nullptr);
    }

    if (!documentBeingDestroyed()) {
        if (firstLineBox()) {
            // We can't wait for LayoutBox::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            // FIXME: The FrameSelection should be responsible for this when it
            // is notified of DOM mutations.
            if (isSelectionBorder())
                view()->clearSelection();

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (InlineFlowBox* box = firstLineBox(); box; box = box->nextLineBox()) {
                    while (InlineBox* childBox = box->firstChild())
                        childBox->remove();
                }
            }
        } else if (parent()) {
            parent()->dirtyLinesFromChangedChild(this);
        }
    }

    m_lineBoxes.deleteLineBoxes();

    if (UNLIKELY(gDelayedUpdateScrollInfoSet != 0))
        gDelayedUpdateScrollInfoSet->remove(this);

    if (TextAutosizer* textAutosizer = document().textAutosizer())
        textAutosizer->destroy(this);

    LayoutBox::willBeDestroyed();
}

void LayoutBlock::styleWillChange(StyleDifference diff, const ComputedStyle& newStyle)
{
    const ComputedStyle* oldStyle = style();

    setIsAtomicInlineLevel(newStyle.isDisplayInlineType());

    if (oldStyle && parent()) {
        bool oldHasTransformRelatedProperty = oldStyle->hasTransformRelatedProperty();
        bool newHasTransformRelatedProperty = newStyle.hasTransformRelatedProperty();
        bool oldStyleIsContainer = oldStyle->position() != StaticPosition || oldHasTransformRelatedProperty;

        if (oldStyleIsContainer && (newStyle.position() == StaticPosition || (oldHasTransformRelatedProperty && !newHasTransformRelatedProperty))) {
            // Clear our positioned objects list. Our absolute and fixed positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(nullptr, NewContainingBlock);
        } else if (!oldStyleIsContainer && (newStyle.position() != StaticPosition || newHasTransformRelatedProperty)) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            if (LayoutBlock* cb = containingBlockForAbsolutePosition())
                cb->removePositionedObjects(this, NewContainingBlock);
        }
        if (!oldHasTransformRelatedProperty && newHasTransformRelatedProperty) {
            // Remove our fixed positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            if (LayoutBlock* cb = containerForFixedPosition())
                cb->removePositionedObjects(this, NewContainingBlock);
        }
    }

    LayoutBox::styleWillChange(diff, newStyle);
}

enum LogicalExtent { LogicalWidth, LogicalHeight };
static bool borderOrPaddingLogicalDimensionChanged(const ComputedStyle& oldStyle, const ComputedStyle& newStyle, LogicalExtent logicalExtent)
{
    if (newStyle.isHorizontalWritingMode() == (logicalExtent == LogicalWidth)) {
        return oldStyle.borderLeftWidth() != newStyle.borderLeftWidth()
            || oldStyle.borderRightWidth() != newStyle.borderRightWidth()
            || oldStyle.paddingLeft() != newStyle.paddingLeft()
            || oldStyle.paddingRight() != newStyle.paddingRight();
    }

    return oldStyle.borderTopWidth() != newStyle.borderTopWidth()
        || oldStyle.borderBottomWidth() != newStyle.borderBottomWidth()
        || oldStyle.paddingTop() != newStyle.paddingTop()
        || oldStyle.paddingBottom() != newStyle.paddingBottom();
}

static bool canMergeContiguousAnonymousBlocks(LayoutObject* prev, LayoutObject* next)
{
    if ((prev && (!prev->isAnonymousBlock() || toLayoutBlock(prev)->continuation() || toLayoutBlock(prev)->beingDestroyed()))
        || (next && (!next->isAnonymousBlock() || toLayoutBlock(next)->continuation() || toLayoutBlock(next)->beingDestroyed())))
        return false;

    if ((prev && (prev->isRubyRun() || prev->isRubyBase()))
        || (next && (next->isRubyRun() || next->isRubyBase())))
        return false;

    return true;
}

static bool mergeContiguousAnonymousBlocks(LayoutObject* prev, LayoutObject*& next)
{
    if (!prev || !next || !canMergeContiguousAnonymousBlocks(prev, next))
        return false;

    prev->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::AnonymousBlockChange);
    LayoutBlockFlow* nextBlock = toLayoutBlockFlow(next);
    LayoutBlockFlow* prevBlock = toLayoutBlockFlow(prev);

    // If the inlineness of children of the two block don't match, we'd need special code here
    // (but there should be no need for it).
    ASSERT(nextBlock->childrenInline() == prevBlock->childrenInline());
    // Take all the children out of the |next| block and put them in
    // the |prev| block.
    nextBlock->moveAllChildrenIncludingFloatsTo(prevBlock, nextBlock->hasLayer() || prevBlock->hasLayer());
    // Delete the now-empty block's lines and nuke it.
    nextBlock->deleteLineBoxTree();
    nextBlock->destroy();
    next = nullptr;
    return true;
}

static void addNextFloatingOrOutOfFlowSiblingsToBlock(LayoutBlock* block)
{
    if (!block->parent() || !block->parent()->isLayoutBlockFlow())
        return;
    if (block->beingDestroyed() || block->documentBeingDestroyed())
        return;

    LayoutObject* child = block->nextSibling();
    while (child && child->isFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = child->nextSibling();
        toLayoutBlock(block->parent())->moveChildTo(block, child, nullptr, false);
        child = sibling;
    }

    LayoutObject* next = block->nextSibling();
    mergeContiguousAnonymousBlocks(block, next);
}

static void addPreviousFloatingOrOutOfFlowSiblingsToBlock(LayoutBlock* block)
{
    if (!block->parent() || !block->parent()->isLayoutBlockFlow())
        return;
    if (block->beingDestroyed() || block->documentBeingDestroyed())
        return;

    LayoutObject* child = block->previousSibling();
    while (child && child->isFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = child->previousSibling();
        toLayoutBlock(block->parent())->moveChildTo(block, child, block->firstChild(), false);
        child = sibling;
    }
}

void LayoutBlock::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutBox::styleDidChange(diff, oldStyle);

    if (isFloatingOrOutOfFlowPositioned() && oldStyle && !oldStyle->isFloating() && !oldStyle->hasOutOfFlowPosition() && parent() && parent()->isLayoutBlockFlow()) {
        toLayoutBlock(parent())->makeChildrenInlineIfPossible();
        // Reparent to an adjacent anonymous block if one is available.
        if (previousSibling() && previousSibling()->isAnonymousBlock()) {
            LayoutBlock* newParent = toLayoutBlock(previousSibling());
            toLayoutBlock(parent())->moveChildTo(newParent, this, nullptr, false);
            // The anonymous block we've moved to may now be adjacent to former siblings of ours that it can contain also.
            addNextFloatingOrOutOfFlowSiblingsToBlock(newParent);
        } else if (nextSibling() && nextSibling()->isAnonymousBlock()) {
            toLayoutBlock(parent())->moveChildTo(toLayoutBlock(nextSibling()), this, nextSibling()->slowFirstChild(), false);
        }
    }

    const ComputedStyle& newStyle = styleRef();

    if (oldStyle && parent()) {
        if (oldStyle->position() != newStyle.position() && newStyle.position() != StaticPosition) {
            // Remove our absolute and fixed positioned descendants from their new containing block,
            // in case containingBlock() changes by the change to the position property.
            // See styleWillChange() for other cases.
            if (LayoutBlock* cb = containingBlock())
                cb->removePositionedObjects(this, NewContainingBlock);
        }
    }

    if (TextAutosizer* textAutosizer = document().textAutosizer())
        textAutosizer->record(this);

    propagateStyleToAnonymousChildren(true);

    // It's possible for our border/padding to change, but for the overall logical width or height of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    m_widthAvailableToChildrenChanged |= oldStyle && diff.needsFullLayout() && needsLayout() && borderOrPaddingLogicalDimensionChanged(*oldStyle, newStyle, LogicalWidth);
    m_heightAvailableToChildrenChanged |= oldStyle && diff.needsFullLayout() && needsLayout() && borderOrPaddingLogicalDimensionChanged(*oldStyle, newStyle, LogicalHeight);
}

void LayoutBlock::updateFromStyle()
{
    LayoutBox::updateFromStyle();

    bool shouldClipOverflow = !styleRef().isOverflowVisible() && allowsOverflowClip();
    if (shouldClipOverflow != hasOverflowClip()) {
        if (!shouldClipOverflow)
            getScrollableArea()->invalidateAllStickyConstraints();
        for (LayoutObject* child = firstChild(); child; child = child->nextSibling())
            child->setMayNeedPaintInvalidation();
    }
    setHasOverflowClip(shouldClipOverflow);
}

bool LayoutBlock::allowsOverflowClip() const
{
    // If overflow has been propagated to the viewport, it has no effect here.
    return node() != document().viewportDefiningElement();
}

inline static void invalidateDisplayItemClientForStartOfContinuationsIfNeeded(const LayoutBlock& block)
{
    // If the block is a continuation or containing block of an inline continuation, invalidate the
    // start object of the continuations if it has focus ring because change of continuation may change
    // the shape of the focus ring.
    if (!block.isAnonymous())
        return;

    LayoutObject* startOfContinuations = nullptr;
    if (LayoutInline* inlineElementContinuation = block.inlineElementContinuation()) {
        // This block is an anonymous block continuation.
        startOfContinuations = inlineElementContinuation->node()->layoutObject();
    } else if (LayoutObject* firstChild = block.firstChild()) {
        // This block is the anonymous containing block of an inline element continuation.
        if (firstChild->isElementContinuation())
            startOfContinuations = firstChild->node()->layoutObject();
    }
    if (startOfContinuations && startOfContinuations->styleRef().outlineStyleIsAuto())
        startOfContinuations->invalidateDisplayItemClient(*startOfContinuations);
}

void LayoutBlock::invalidateDisplayItemClients(const LayoutBoxModelObject& paintInvalidationContainer, PaintInvalidationReason invalidationReason) const
{
    LayoutBox::invalidateDisplayItemClients(paintInvalidationContainer, invalidationReason);
    invalidateDisplayItemClientForStartOfContinuationsIfNeeded(*this);
}

void LayoutBlock::addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != this) {
        LayoutObject* beforeChildContainer = beforeChild->parent();
        while (beforeChildContainer->parent() != this)
            beforeChildContainer = beforeChildContainer->parent();
        ASSERT(beforeChildContainer);

        if (beforeChildContainer->isAnonymous()) {
            // If the requested beforeChild is not one of our children, then this is because
            // there is an anonymous container within this object that contains the beforeChild.
            LayoutObject* beforeChildAnonymousContainer = beforeChildContainer;
            if (beforeChildAnonymousContainer->isAnonymousBlock()
                // Full screen layoutObjects and full screen placeholders act as anonymous blocks, not tables:
                || beforeChildAnonymousContainer->isLayoutFullScreen()
                || beforeChildAnonymousContainer->isLayoutFullScreenPlaceholder()
                ) {
                // Insert the child into the anonymous block box instead of here.
                if (newChild->isInline() || newChild->isFloatingOrOutOfFlowPositioned() || beforeChild->parent()->slowFirstChild() != beforeChild)
                    beforeChild->parent()->addChild(newChild, beforeChild);
                else
                    addChild(newChild, beforeChild->parent());
                return;
            }

            ASSERT(beforeChildAnonymousContainer->isTable());
            if (newChild->isTablePart()) {
                // Insert into the anonymous table.
                beforeChildAnonymousContainer->addChild(newChild, beforeChild);
                return;
            }

            beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

            ASSERT(beforeChild->parent() == this);
            if (beforeChild->parent() != this) {
                // We should never reach here. If we do, we need to use the
                // safe fallback to use the topmost beforeChild container.
                beforeChild = beforeChildContainer;
            }
        }
    }

    bool madeBoxesNonInline = false;

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    if (childrenInline() && !newChild->isInline() && !newChild->isFloatingOrOutOfFlowPositioned()) {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        makeChildrenNonInline(beforeChild);
        madeBoxesNonInline = true;

        if (beforeChild && beforeChild->parent() != this) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == this);
        }
    } else if (!childrenInline() && (newChild->isFloatingOrOutOfFlowPositioned() || newChild->isInline())) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        LayoutObject* afterChild = beforeChild ? beforeChild->previousSibling() : lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            afterChild->addChild(newChild);
            return;
        }

        if (newChild->isInline()) {
            // No suitable existing anonymous box - create a new one.
            LayoutBlock* newBox = createAnonymousBlock();
            LayoutBox::addChild(newBox, beforeChild);
            // Reparent adjacent floating or out-of-flow siblings to the new box.
            addPreviousFloatingOrOutOfFlowSiblingsToBlock(newBox);
            newBox->addChild(newChild);
            addNextFloatingOrOutOfFlowSiblingsToBlock(newBox);
            return;
        }
    }

    LayoutBox::addChild(newChild, beforeChild);

    if (madeBoxesNonInline && parent() && isAnonymousBlock() && parent()->isLayoutBlock())
        toLayoutBlock(parent())->removeLeftoverAnonymousBlock(this);
    // this object may be dead here
}

void LayoutBlock::addChild(LayoutObject* newChild, LayoutObject* beforeChild)
{
    addChildIgnoringContinuation(newChild, beforeChild);
}

static void getInlineRun(LayoutObject* start, LayoutObject* boundary,
    LayoutObject*& inlineRunStart,
    LayoutObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can.  We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|.  Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point.  Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it.  It's as though we encountered
    // a non-inline.

    // Start by skipping as many non-inlines as we can.
    LayoutObject * curr = start;
    bool sawInline;
    do {
        while (curr && !(curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()))
            curr = curr->nextSibling();

        inlineRunStart = inlineRunEnd = curr;

        if (!curr)
            return; // No more inline children to be found.

        sawInline = curr->isInline();

        curr = curr->nextSibling();
        while (curr && (curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()) && (curr != boundary)) {
            inlineRunEnd = curr;
            if (curr->isInline())
                sawInline = true;
            curr = curr->nextSibling();
        }
    } while (!sawInline);
}

void LayoutBlock::deleteLineBoxTree()
{
    ASSERT(!m_lineBoxes.firstLineBox());
}

void LayoutBlock::makeChildrenNonInline(LayoutObject *insertionPoint)
{
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks.  If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines.  This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    ASSERT(isInlineBlockOrInlineTable() || !isInline());
    ASSERT(!insertionPoint || insertionPoint->parent() == this);

    setChildrenInline(false);

    LayoutObject* child = firstChild();
    if (!child)
        return;

    deleteLineBoxTree();

    while (child) {
        LayoutObject* inlineRunStart;
        LayoutObject* inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        LayoutBlock* block = createAnonymousBlock();
        children()->insertChildNode(this, block, inlineRunStart);
        moveChildrenTo(block, inlineRunStart, child);
    }

#if ENABLE(ASSERT)
    for (LayoutObject *c = firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif

    setShouldDoFullPaintInvalidation();
}

void LayoutBlock::removeLeftoverAnonymousBlock(LayoutBlock* child)
{
    ASSERT(child->isAnonymousBlock());
    ASSERT(!child->childrenInline());
    ASSERT(child->parent() == this);

    if (child->continuation())
        return;

    // Promote all the leftover anonymous block's children (to become children of this block
    // instead). We still want to keep the leftover block in the tree for a moment, for notification
    // purposes done further below (flow threads and grids).
    child->moveAllChildrenTo(this, child->nextSibling());

    // Remove all the information in the flow thread associated with the leftover anonymous block.
    child->removeFromLayoutFlowThread();

    // LayoutGrid keeps track of its children, we must notify it about changes in the tree.
    if (child->parent()->isLayoutGrid())
        toLayoutGrid(child->parent())->dirtyGrid();

    // Now remove the leftover anonymous block from the tree, and destroy it. We'll rip it out
    // manually from the tree before destroying it, because we don't want to trigger any tree
    // adjustments with regards to anonymous blocks (or any other kind of undesired chain-reaction).
    children()->removeChildNode(this, child, false);
    child->destroy();
}

void LayoutBlock::makeChildrenInlineIfPossible()
{
    ASSERT(isLayoutBlockFlow());
    // Collapsing away anonymous wrappers isn't relevant for the children of anonymous blocks, unless they are ruby bases.
    if (isAnonymousBlock() && !isRubyBase())
        return;

    Vector<LayoutBlock*, 3> blocksToRemove;
    for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isFloating())
            continue;
        if (child->isOutOfFlowPositioned())
            continue;

        // There are still block children in the container, so any anonymous wrappers are still needed.
        if (!child->isAnonymousBlock())
            return;
        // If one of the children is being destroyed then it is unsafe to clean up anonymous wrappers as the
        // entire branch may be being destroyed.
        if (toLayoutBlock(child)->beingDestroyed())
            return;
        // We can't remove anonymous wrappers if they contain continuations as this means there are block children present.
        if (toLayoutBlock(child)->continuation())
            return;
        // We are only interested in removing anonymous wrappers if there are inline siblings underneath them.
        if (!child->childrenInline())
            return;
        // Ruby elements use anonymous wrappers for ruby runs and ruby bases by design, so we don't remove them.
        if (child->isRubyRun() || child->isRubyBase())
            return;

        blocksToRemove.append(toLayoutBlock(child));
    }

    // If we make an object's children inline we are going to frustrate any future attempts to remove
    // floats from its children's float-lists before the next layout happens so clear down all the floatlists
    // now - they will be rebuilt at layout.
    toLayoutBlockFlow(this)->removeFloatingObjectsFromDescendants();

    for (size_t i = 0; i < blocksToRemove.size(); i++)
        collapseAnonymousBlockChild(this, blocksToRemove[i]);
    setChildrenInline(true);
}

void LayoutBlock::collapseAnonymousBlockChild(LayoutBlock* parent, LayoutBlock* child)
{
    if (!parent->canCollapseAnonymousBlockChild())
        return;
    // It's possible that this block's destruction may have been triggered by the
    // child's removal. Just bail if the anonymous child block is already being
    // destroyed. See crbug.com/282088
    if (child->beingDestroyed())
        return;
    if (child->continuation())
        return;
    // Ruby elements use anonymous wrappers for ruby runs and ruby bases by design, so we don't remove them.
    if (child->isRubyRun() || child->isRubyBase())
        return;
    parent->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::ChildAnonymousBlockChanged);

    child->moveAllChildrenTo(parent, child->nextSibling(), child->hasLayer());
    parent->setChildrenInline(child->childrenInline());

    parent->children()->removeChildNode(parent, child, child->hasLayer());
    child->destroy();
}

void LayoutBlock::removeChild(LayoutObject* oldChild)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (documentBeingDestroyed()) {
        LayoutBox::removeChild(oldChild);
        return;
    }

    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    LayoutObject* prev = oldChild->previousSibling();
    LayoutObject* next = oldChild->nextSibling();
    bool mergedAnonymousBlocks = !oldChild->documentBeingDestroyed() && !oldChild->isInline() && !oldChild->virtualContinuation() && mergeContiguousAnonymousBlocks(prev, next);

    LayoutBox::removeChild(oldChild);

    LayoutObject* child = prev ? prev : next;
    if (mergedAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        collapseAnonymousBlockChild(this, toLayoutBlock(child));
    }

    if (!firstChild()) {
        // If this was our last child be sure to clear out our line boxes.
        if (childrenInline())
            deleteLineBoxTree();

        // If we are an empty anonymous block in the continuation chain,
        // we need to remove ourself and fix the continuation chain.
        if (!beingDestroyed() && isAnonymousBlockContinuation() && !oldChild->isListMarker()) {
            LayoutObject* containingBlockIgnoringAnonymous = containingBlock();
            while (containingBlockIgnoringAnonymous && containingBlockIgnoringAnonymous->isAnonymous())
                containingBlockIgnoringAnonymous = containingBlockIgnoringAnonymous->containingBlock();
            for (LayoutObject* curr = this; curr; curr = curr->previousInPreOrder(containingBlockIgnoringAnonymous)) {
                if (curr->virtualContinuation() != this)
                    continue;

                // Found our previous continuation. We just need to point it to
                // |this|'s next continuation.
                LayoutBoxModelObject* nextContinuation = continuation();
                if (curr->isLayoutInline())
                    toLayoutInline(curr)->setContinuation(nextContinuation);
                else if (curr->isLayoutBlock())
                    toLayoutBlock(curr)->setContinuation(nextContinuation);
                else
                    ASSERT_NOT_REACHED();

                break;
            }
            setContinuation(nullptr);
            destroy();
        }
    } else if (!beingDestroyed() && !oldChild->isFloatingOrOutOfFlowPositioned() && isLayoutBlockFlow() && !oldChild->isAnonymousBlock()) {
        // If the child we're removing means that we can now treat all children as inline without the need for anonymous blocks, then do that.
        makeChildrenInlineIfPossible();
    }
}

bool LayoutBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    // (e) have specified that one of our margins can't collapse using a CSS extension
    // (f) establish a new block formatting context.

    // The early exit must be done before we check for clean layout.
    // We should be able to give a quick answer if the box is a relayout boundary.
    // Being a relayout boundary implies a block formatting context, and also
    // our internal layout shouldn't affect our container in any way.
    if (createsNewFormattingContext())
        return false;

    // Placeholder elements are not laid out until the dimensions of their parent text control are known, so they
    // don't get layout until their parent has had layout - this is unique in the layout tree and means
    // when we call isSelfCollapsingBlock on them we find that they still need layout.
    ASSERT(!needsLayout() || (node() && node()->isElementNode() && toElement(node())->shadowPseudoId() == "-webkit-input-placeholder"));

    if (logicalHeight() > 0
        || isTable() || borderAndPaddingLogicalHeight()
        || style()->logicalMinHeight().isPositive()
        || style()->marginBeforeCollapse() == MarginCollapseSeparate || style()->marginAfterCollapse() == MarginCollapseSeparate)
        return false;

    Length logicalHeightLength = style()->logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.hasPercent() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (LayoutBlock* cb = containingBlock(); !cb->isLayoutView(); cb = cb->containingBlock()) {
            if (cb->style()->logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.hasPercent()) && logicalHeightLength.isZero())) {
        // If the block has inline children, see if we generated any line boxes.  If we have any
        // line boxes, then we can't be self-collapsing, since we have content.
        if (childrenInline())
            return !firstLineBox();

        // Whether or not we collapse is dependent on whether all our normal flow children
        // are also self-collapsing.
        if (m_hasOnlySelfCollapsingChildren)
            return true;
        for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isFloatingOrOutOfFlowPositioned())
                continue;
            if (!child->isSelfCollapsingBlock())
                return false;
        }
        return true;
    }
    return false;
}

void LayoutBlock::startDelayUpdateScrollInfo()
{
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(!gDelayedUpdateScrollInfoSet);
        gDelayedUpdateScrollInfoSet = new DelayedUpdateScrollInfoSet;
    }
    ASSERT(gDelayedUpdateScrollInfoSet);
    ++gDelayUpdateScrollInfo;
}

bool LayoutBlock::finishDelayUpdateScrollInfo(SubtreeLayoutScope* layoutScope, ScrollPositionMap* scrollMap)
{
    bool childrenMarkedForRelayout = false;

    --gDelayUpdateScrollInfo;
    ASSERT(gDelayUpdateScrollInfo >= 0);
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(gDelayedUpdateScrollInfoSet);

        OwnPtr<DelayedUpdateScrollInfoSet> infoSet(adoptPtr(gDelayedUpdateScrollInfoSet));
        gDelayedUpdateScrollInfoSet = nullptr;

        for (auto* block : *infoSet) {
            if (block->hasOverflowClip()) {
                PaintLayerScrollableArea* scrollableArea = block->layer()->getScrollableArea();
                if (scrollMap)
                    scrollMap->add(scrollableArea, scrollableArea->scrollPositionDouble());
                childrenMarkedForRelayout |= scrollableArea->updateAfterLayout(layoutScope);
            }
        }
    }
    return childrenMarkedForRelayout;
}

void LayoutBlock::updateAfterLayout()
{
    invalidateStickyConstraints();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip()) {
        if (style()->isFlippedBlocksWritingMode()) {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=97937
            // Workaround for now. We cannot delay the scroll info for overflow
            // for items with opposite writing directions, as the contents needs
            // to overflow in that direction
            layer()->getScrollableArea()->updateAfterLayout();
            return;
        }

        if (gDelayUpdateScrollInfo)
            gDelayedUpdateScrollInfoSet->add(this);
        else
            layer()->getScrollableArea()->updateAfterLayout();
    }
}

void LayoutBlock::layout()
{
    LayoutAnalyzer::Scope analyzer(*this);

    bool needsScrollAnchoring = RuntimeEnabledFeatures::scrollAnchoringEnabled() && hasOverflowClip();
    if (needsScrollAnchoring)
        getScrollableArea()->scrollAnchor().save();

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);

    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    if (hasControlClip() && m_overflow)
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();

    if (needsScrollAnchoring)
        getScrollableArea()->scrollAnchor().restore();

    m_heightAvailableToChildrenChanged = false;
}

bool LayoutBlock::widthAvailableToChildrenHasChanged()
{
    // TODO(robhogan): Does m_widthAvailableToChildrenChanged always get reset when it needs to?
    bool widthAvailableToChildrenHasChanged = m_widthAvailableToChildrenChanged;
    m_widthAvailableToChildrenChanged = false;

    // If we use border-box sizing, have percentage padding, and our parent has changed width then the width available to our children has changed even
    // though our own width has remained the same.
    widthAvailableToChildrenHasChanged |= style()->boxSizing() == BoxSizingBorderBox && needsPreferredWidthsRecalculation() && view()->layoutState()->containingBlockLogicalWidthChanged();

    return widthAvailableToChildrenHasChanged;
}

bool LayoutBlock::updateLogicalWidthAndColumnWidth()
{
    LayoutUnit oldWidth = logicalWidth();
    updateLogicalWidth();
    return oldWidth != logicalWidth() || widthAvailableToChildrenHasChanged();
}

void LayoutBlock::layoutBlock(bool)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void LayoutBlock::addOverflowFromChildren()
{
    if (childrenInline())
        toLayoutBlockFlow(this)->addOverflowFromInlineChildren();
    else
        addOverflowFromBlockChildren();
}

void LayoutBlock::computeOverflow(LayoutUnit oldClientAfterEdge, bool)
{
    m_overflow.clear();

    // Add overflow from children.
    addOverflowFromChildren();

    // Add in the overflow from positioned objects.
    addOverflowFromPositionedObjects();

    if (hasOverflowClip()) {
        // When we have overflow clip, propagate the original spillout since it will include collapsed bottom margins
        // and bottom padding.  Set the axis we don't care about to be 1, since we want this overflow to always
        // be considered reachable.
        LayoutRect clientRect(noOverflowRect());
        LayoutRect rectToApply;
        if (isHorizontalWritingMode())
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), LayoutUnit(1), (oldClientAfterEdge - clientRect.y()).clampNegativeToZero());
        else
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), (oldClientAfterEdge - clientRect.x()).clampNegativeToZero(), LayoutUnit(1));
        addLayoutOverflow(rectToApply);
        if (hasOverflowModel())
            m_overflow->setLayoutClientAfterEdge(oldClientAfterEdge);
    }

    addVisualEffectOverflow();

    addVisualOverflowFromTheme();
}

void LayoutBlock::addOverflowFromBlockChildren()
{
    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!child->isFloatingOrOutOfFlowPositioned() && !child->isColumnSpanAll())
            addOverflowFromChild(child);
    }
}

void LayoutBlock::addOverflowFromPositionedObjects()
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    for (auto* positionedObject : *positionedDescendants) {
        // Fixed positioned elements don't contribute to layout overflow, since they don't scroll with the content.
        if (positionedObject->style()->position() != FixedPosition)
            addOverflowFromChild(positionedObject, toLayoutSize(positionedObject->location()));
    }
}

void LayoutBlock::addVisualOverflowFromTheme()
{
    if (!style()->hasAppearance())
        return;

    IntRect inflatedRect = pixelSnappedBorderBoxRect();
    LayoutTheme::theme().addVisualOverflow(*this, inflatedRect);
    addSelfVisualOverflow(LayoutRect(inflatedRect));
}

bool LayoutBlock::createsNewFormattingContext() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrOutOfFlowPositioned() || hasOverflowClip() || isFlexItemIncludingDeprecated()
        || style()->specifiesColumns() || isLayoutFlowThread() || isTableCell() || isTableCaption() || isFieldset() || isWritingModeRoot()
        || isDocumentElement() || isColumnSpanAll() || isGridItem() || style()->containsPaint() || style()->containsLayout();
}

static inline bool changeInAvailableLogicalHeightAffectsChild(LayoutBlock* parent, LayoutBox& child)
{
    if (parent->style()->boxSizing() != BoxSizingBorderBox)
        return false;
    return parent->style()->isHorizontalWritingMode() && !child.style()->isHorizontalWritingMode();
}

void LayoutBlock::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox& child)
{
    if (child.isOutOfFlowPositioned()) {
        // It's rather useless to mark out-of-flow children at this point. We may not be their
        // containing block (and if we are, it's just pure luck), so this would be the wrong place
        // for it. Furthermore, it would cause trouble for out-of-flow descendants of column
        // spanners, if the containing block is outside the spanner but inside the multicol container.
        return;
    }
    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    bool hasRelativeLogicalHeight = child.hasRelativeLogicalHeight()
        || (child.isAnonymous() && this->hasRelativeLogicalHeight())
        || child.stretchesToViewport();
    if (relayoutChildren || (hasRelativeLogicalHeight && !isLayoutView())
        || (m_heightAvailableToChildrenChanged && changeInAvailableLogicalHeightAffectsChild(this, child))) {
        child.setChildNeedsLayout(MarkOnlyThis);

        // If the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
        if (child.needsPreferredWidthsRecalculation())
            child.setPreferredLogicalWidthsDirty(MarkOnlyThis);
    }
}

void LayoutBlock::simplifiedNormalFlowLayout()
{
    if (childrenInline()) {
        ASSERT_WITH_SECURITY_IMPLICATION(isLayoutBlockFlow());
        LayoutBlockFlow* blockFlow = toLayoutBlockFlow(this);
        blockFlow->simplifiedNormalFlowInlineLayout();
    } else {
        for (LayoutBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (!box->isOutOfFlowPositioned()) {
                if (box->isLayoutMultiColumnSpannerPlaceholder())
                    toLayoutMultiColumnSpannerPlaceholder(box)->markForLayoutIfObjectInFlowThreadNeedsLayout();
                box->layoutIfNeeded();
            }
        }
    }
}

bool LayoutBlock::simplifiedLayout()
{
    // Check if we need to do a full layout.
    if (normalChildNeedsLayout() || selfNeedsLayout())
        return false;

    // Check that we actually need to do a simplified layout.
    if (!posChildNeedsLayout() && !(needsSimplifiedNormalFlowLayout() || needsPositionedMovementLayout()))
        return false;

    {
        // LayoutState needs this deliberate scope to pop before paint invalidation.
        LayoutState state(*this, locationOffset());

        if (needsPositionedMovementLayout() && !tryLayoutDoingPositionedMovementOnly())
            return false;

        if (LayoutFlowThread* flowThread = flowThreadContainingBlock()) {
            if (!flowThread->canSkipLayout(*this))
                return false;
        }

        TextAutosizer::LayoutScope textAutosizerLayoutScope(this);

        // Lay out positioned descendants or objects that just need to recompute overflow.
        if (needsSimplifiedNormalFlowLayout())
            simplifiedNormalFlowLayout();

        // Lay out our positioned objects if our positioned child bit is set.
        // Also, if an absolute position element inside a relative positioned container moves, and the absolute element has a fixed position
        // child, neither the fixed element nor its container learn of the movement since posChildNeedsLayout() is only marked as far as the
        // relative positioned container. So if we can have fixed pos objects in our positioned objects list check if any of them
        // are statically positioned and thus need to move with their absolute ancestors.
        bool canContainFixedPosObjects = canContainFixedPositionObjects();
        if (posChildNeedsLayout() || needsPositionedMovementLayout() || canContainFixedPosObjects)
            layoutPositionedObjects(false, needsPositionedMovementLayout() ? ForcedLayoutAfterContainingBlockMoved : (!posChildNeedsLayout() && canContainFixedPosObjects ? LayoutOnlyFixedPositionedObjects : DefaultLayout));

        // Recompute our overflow information.
        // FIXME: We could do better here by computing a temporary overflow object from layoutPositionedObjects and only
        // updating our overflow if we either used to have overflow or if the new temporary object has overflow.
        // For now just always recompute overflow. This is no worse performance-wise than the old code that called rightmostPosition and
        // lowestPosition on every relayout so it's not a regression.
        // computeOverflow expects the bottom edge before we clamp our height. Since this information isn't available during
        // simplifiedLayout, we cache the value in m_overflow.
        LayoutUnit oldClientAfterEdge = hasOverflowModel() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
        computeOverflow(oldClientAfterEdge, true);
    }

    updateLayerTransformAfterLayout();

    updateAfterLayout();

    clearNeedsLayout();

    if (LayoutAnalyzer* analyzer = frameView()->layoutAnalyzer())
        analyzer->increment(LayoutAnalyzer::LayoutObjectsThatNeedSimplifiedLayout);

    return true;
}

void LayoutBlock::markFixedPositionObjectForLayoutIfNeeded(LayoutObject* child, SubtreeLayoutScope& layoutScope)
{
    if (child->style()->position() != FixedPosition)
        return;

    bool hasStaticBlockPosition = child->style()->hasStaticBlockPosition(isHorizontalWritingMode());
    bool hasStaticInlinePosition = child->style()->hasStaticInlinePosition(isHorizontalWritingMode());
    if (!hasStaticBlockPosition && !hasStaticInlinePosition)
        return;

    LayoutObject* o = child->parent();
    while (o && !o->isLayoutView() && o->style()->position() != AbsolutePosition)
        o = o->parent();
    if (o->style()->position() != AbsolutePosition)
        return;

    LayoutBox* box = toLayoutBox(child);
    if (hasStaticInlinePosition) {
        LogicalExtentComputedValues computedValues;
        box->computeLogicalWidth(computedValues);
        LayoutUnit newLeft = computedValues.m_position;
        if (newLeft != box->logicalLeft())
            layoutScope.setChildNeedsLayout(child);
    } else if (hasStaticBlockPosition) {
        LayoutUnit oldTop = box->logicalTop();
        box->updateLogicalHeight();
        if (box->logicalTop() != oldTop)
            layoutScope.setChildNeedsLayout(child);
    }
}

LayoutUnit LayoutBlock::marginIntrinsicLogicalWidthForChild(LayoutBox& child) const
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child.style()->marginStartUsing(style());
    Length marginRight = child.style()->marginEndUsing(style());
    LayoutUnit margin;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

static bool needsLayoutDueToStaticPosition(LayoutBox* child)
{
    // When a non-positioned block element moves, it may have positioned children that are
    // implicitly positioned relative to the non-positioned block.
    const ComputedStyle* style = child->style();
    bool isHorizontal = style->isHorizontalWritingMode();
    if (style->hasStaticBlockPosition(isHorizontal)) {
        LayoutBox::LogicalExtentComputedValues computedValues;
        LayoutUnit currentLogicalTop = child->logicalTop();
        LayoutUnit currentLogicalHeight = child->logicalHeight();
        child->computeLogicalHeight(currentLogicalHeight, currentLogicalTop, computedValues);
        if (computedValues.m_position != currentLogicalTop || computedValues.m_extent != currentLogicalHeight)
            return true;
    }
    if (style->hasStaticInlinePosition(isHorizontal)) {
        LayoutBox::LogicalExtentComputedValues computedValues;
        LayoutUnit currentLogicalLeft = child->logicalLeft();
        LayoutUnit currentLogicalWidth = child->logicalWidth();
        child->computeLogicalWidth(computedValues);
        if (computedValues.m_position != currentLogicalLeft || computedValues.m_extent != currentLogicalWidth)
            return true;
    }
    return false;
}

void LayoutBlock::layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior info)
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    bool isPaginated = view()->layoutState()->isPaginated();

    for (auto* positionedObject : *positionedDescendants) {
        positionedObject->setMayNeedPaintInvalidation();

        SubtreeLayoutScope layoutScope(*positionedObject);
        // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
        // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e.
        // it has static position.
        markFixedPositionObjectForLayoutIfNeeded(positionedObject, layoutScope);
        if (info == LayoutOnlyFixedPositionedObjects) {
            positionedObject->layoutIfNeeded();
            continue;
        }

        if (!positionedObject->normalChildNeedsLayout() && (relayoutChildren || m_heightAvailableToChildrenChanged || needsLayoutDueToStaticPosition(positionedObject)))
            layoutScope.setChildNeedsLayout(positionedObject);

        // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
        if (relayoutChildren && positionedObject->needsPreferredWidthsRecalculation())
            positionedObject->setPreferredLogicalWidthsDirty(MarkOnlyThis);

        LayoutUnit logicalTopEstimate;
        bool needsBlockDirectionLocationSetBeforeLayout = isPaginated && positionedObject->getPaginationBreakability() != ForbidBreaks;
        if (needsBlockDirectionLocationSetBeforeLayout) {
            // Out-of-flow objects are normally positioned after layout (while in-flow objects are
            // positioned before layout). If the child object is paginated in the same context as
            // we are, estimate its logical top now. We need to know this up-front, to correctly
            // evaluate if we need to mark for relayout, and, if our estimate is correct, we'll
            // even be able to insert correct pagination struts on the first attempt.
            LogicalExtentComputedValues computedValues;
            positionedObject->computeLogicalHeight(positionedObject->logicalHeight(), positionedObject->logicalTop(), computedValues);
            logicalTopEstimate = computedValues.m_position;
            positionedObject->setLogicalTop(logicalTopEstimate);
        }

        if (!positionedObject->needsLayout())
            positionedObject->markForPaginationRelayoutIfNeeded(layoutScope);

        // FIXME: We should be able to do a r->setNeedsPositionedMovementLayout() here instead of a full layout. Need
        // to investigate why it does not trigger the correct invalidations in that case. crbug.com/350756
        if (info == ForcedLayoutAfterContainingBlockMoved)
            positionedObject->setNeedsLayout(LayoutInvalidationReason::AncestorMoved, MarkOnlyThis);

        positionedObject->layoutIfNeeded();

        LayoutObject* parent = positionedObject->parent();
        bool layoutChanged = false;
        if (parent->isFlexibleBox() && toLayoutFlexibleBox(parent)->setStaticPositionForPositionedLayout(*positionedObject)) {
            // The static position of an abspos child of a flexbox depends on its size (for example,
            // they can be centered). So we may have to reposition the item after layout.
            // TODO(cbiesinger): We could probably avoid a layout here and just reposition?
            positionedObject->forceChildLayout();
            layoutChanged = true;
        }

        // Lay out again if our estimate was wrong.
        if (!layoutChanged && needsBlockDirectionLocationSetBeforeLayout && logicalTopEstimate != logicalTopForChild(*positionedObject))
            positionedObject->forceChildLayout();
    }
}

void LayoutBlock::markPositionedObjectsForLayout()
{
    if (TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects()) {
        for (auto* descendant : *positionedDescendants)
            descendant->setChildNeedsLayout();
    }
}

void LayoutBlock::markForPaginationRelayoutIfNeeded(SubtreeLayoutScope& layoutScope)
{
    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    if (view()->layoutState()->pageLogicalHeightChanged() || (view()->layoutState()->pageLogicalHeight() && view()->layoutState()->pageLogicalOffset(*this, logicalTop()) != pageLogicalOffset()))
        layoutScope.setChildNeedsLayout(this);
}

void LayoutBlock::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BlockPainter(*this).paint(paintInfo, paintOffset);
}

void LayoutBlock::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BlockPainter(*this).paintChildren(paintInfo, paintOffset);
}

void LayoutBlock::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BlockPainter(*this).paintObject(paintInfo, paintOffset);
}

LayoutInline* LayoutBlock::inlineElementContinuation() const
{
    LayoutBoxModelObject* continuation = this->continuation();
    return continuation && continuation->isInline() ? toLayoutInline(continuation) : 0;
}

bool LayoutBlock::isSelectionRoot() const
{
    if (isPseudoElement())
        return false;
    ASSERT(node() || isAnonymous());

    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isTable())
        return false;

    if (isBody() || isDocumentElement() || hasOverflowClip()
        || isPositioned() || isFloating()
        || isTableCell() || isInlineBlockOrInlineTable()
        || hasTransformRelatedProperty() || hasReflection() || hasMask() || isWritingModeRoot()
        || isLayoutFlowThread() || isFlexItemIncludingDeprecated())
        return true;

    if (view() && view()->selectionStart()) {
        Node* startElement = view()->selectionStart()->node();
        if (startElement && startElement->rootEditableElement() == node())
            return true;
    }

    return false;
}

LayoutUnit LayoutBlock::blockDirectionOffset(const LayoutSize& offsetFromBlock) const
{
    return isHorizontalWritingMode() ? offsetFromBlock.height() : offsetFromBlock.width();
}

LayoutUnit LayoutBlock::inlineDirectionOffset(const LayoutSize& offsetFromBlock) const
{
    return isHorizontalWritingMode() ? offsetFromBlock.width() : offsetFromBlock.height();
}

LayoutRect LayoutBlock::logicalRectToPhysicalRect(const LayoutPoint& rootBlockPhysicalPosition, const LayoutRect& logicalRect) const
{
    LayoutRect result;
    if (isHorizontalWritingMode())
        result = logicalRect;
    else
        result = LayoutRect(logicalRect.y(), logicalRect.x(), logicalRect.height(), logicalRect.width());
    flipForWritingMode(result);
    result.moveBy(rootBlockPhysicalPosition);
    return result;
}

LayoutUnit LayoutBlock::logicalLeftSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const
{
    // The border can potentially be further extended by our containingBlock().
    if (rootBlock != this)
        return containingBlock()->logicalLeftSelectionOffset(rootBlock, position + logicalTop());
    return logicalLeftOffsetForContent();
}

LayoutUnit LayoutBlock::logicalRightSelectionOffset(const LayoutBlock* rootBlock, LayoutUnit position) const
{
    // The border can potentially be further extended by our containingBlock().
    if (rootBlock != this)
        return containingBlock()->logicalRightSelectionOffset(rootBlock, position + logicalTop());
    return logicalRightOffsetForContent();
}

LayoutBlock* LayoutBlock::blockBeforeWithinSelectionRoot(LayoutSize& offset) const
{
    if (isSelectionRoot())
        return nullptr;

    const LayoutObject* object = this;
    LayoutObject* sibling;
    do {
        sibling = object->previousSibling();
        while (sibling && (!sibling->isLayoutBlock() || toLayoutBlock(sibling)->isSelectionRoot()))
            sibling = sibling->previousSibling();

        offset -= LayoutSize(toLayoutBlock(object)->logicalLeft(), toLayoutBlock(object)->logicalTop());
        object = object->parent();
    } while (!sibling && object && object->isLayoutBlock() && !toLayoutBlock(object)->isSelectionRoot());

    if (!sibling)
        return nullptr;

    LayoutBlock* beforeBlock = toLayoutBlock(sibling);

    offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

    LayoutObject* child = beforeBlock->lastChild();
    while (child && child->isLayoutBlock()) {
        beforeBlock = toLayoutBlock(child);
        offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
        child = beforeBlock->lastChild();
    }
    return beforeBlock;
}

void LayoutBlock::setSelectionState(SelectionState state)
{
    LayoutBox::setSelectionState(state);

    if (inlineBoxWrapper() && canUpdateSelectionOnRootLineBoxes())
        inlineBoxWrapper()->root().setHasSelectedChildren(state != SelectionNone);
}

TrackedLayoutBoxListHashSet* LayoutBlock::positionedObjectsInternal() const
{
    return gPositionedDescendantsMap ? gPositionedDescendantsMap->get(this) : nullptr;
}

void LayoutBlock::insertPositionedObject(LayoutBox* o)
{
    ASSERT(!isAnonymousBlock());
    ASSERT(o->containingBlock() == this);

    if (gPositionedContainerMap) {
        auto containerMapIt = gPositionedContainerMap->find(o);
        if (containerMapIt != gPositionedContainerMap->end()) {
            if (containerMapIt->value == this) {
                ASSERT(hasPositionedObjects() && positionedObjects()->contains(o));
                return;
            }
            removePositionedObject(o);
        }
    } else {
        gPositionedContainerMap = new TrackedContainerMap;
    }
    gPositionedContainerMap->set(o, this);

    if (!gPositionedDescendantsMap)
        gPositionedDescendantsMap = new TrackedDescendantsMap;
    TrackedLayoutBoxListHashSet* descendantSet = gPositionedDescendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedLayoutBoxListHashSet;
        gPositionedDescendantsMap->set(this, adoptPtr(descendantSet));
    }
    descendantSet->add(o);

    m_hasPositionedObjects = true;
}

void LayoutBlock::removePositionedObject(LayoutBox* o)
{
    if (!gPositionedContainerMap)
        return;

    LayoutBlock* container = gPositionedContainerMap->take(o);
    if (!container)
        return;

    TrackedLayoutBoxListHashSet* positionedDescendants = gPositionedDescendantsMap->get(container);
    ASSERT(positionedDescendants && positionedDescendants->contains(o));
    positionedDescendants->remove(o);
    if (positionedDescendants->isEmpty()) {
        gPositionedDescendantsMap->remove(container);
        container->m_hasPositionedObjects = false;
    }
}

void LayoutBlock::removePositionedObjects(LayoutBlock* o, ContainingBlockState containingBlockState)
{
    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    Vector<LayoutBox*, 16> deadObjects;
    for (auto* positionedObject : *positionedDescendants) {
        if (!o || (positionedObject->isDescendantOf(o) && o != positionedObject)) {
            if (containingBlockState == NewContainingBlock) {
                positionedObject->setChildNeedsLayout(MarkOnlyThis);
                if (positionedObject->needsPreferredWidthsRecalculation())
                    positionedObject->setPreferredLogicalWidthsDirty(MarkOnlyThis);

                // The positioned object changing containing block may change paint invalidation container.
                // Invalidate it (including non-compositing descendants) on its original paint invalidation container.
                if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
                    // This valid because we need to invalidate based on the current status.
                    DisableCompositingQueryAsserts compositingDisabler;
                    if (!positionedObject->isPaintInvalidationContainer())
                        positionedObject->invalidatePaintIncludingNonCompositingDescendants();
                }
            }

            // It is parent blocks job to add positioned child to positioned objects list of its containing block
            // Parent layout needs to be invalidated to ensure this happens.
            LayoutObject* p = positionedObject->parent();
            while (p && !p->isLayoutBlock())
                p = p->parent();
            if (p)
                p->setChildNeedsLayout();

            deadObjects.append(positionedObject);
        }
    }

    for (auto object : deadObjects) {
        ASSERT(gPositionedContainerMap->get(object) == this);
        positionedDescendants->remove(object);
        gPositionedContainerMap->remove(object);
    }
    if (positionedDescendants->isEmpty()) {
        gPositionedDescendantsMap->remove(this);
        m_hasPositionedObjects = false;
    }
}

void LayoutBlock::addPercentHeightDescendant(LayoutBox* descendant)
{
    if (descendant->percentHeightContainer()) {
        if (descendant->percentHeightContainer() == this) {
            ASSERT(hasPercentHeightDescendant(descendant));
            return;
        }
        descendant->removeFromPercentHeightContainer();
    }
    descendant->setPercentHeightContainer(this);

    if (!gPercentHeightDescendantsMap)
        gPercentHeightDescendantsMap = new TrackedDescendantsMap;
    TrackedLayoutBoxListHashSet* descendantSet = gPercentHeightDescendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedLayoutBoxListHashSet;
        gPercentHeightDescendantsMap->set(this, adoptPtr(descendantSet));
    }
    descendantSet->add(descendant);

    m_hasPercentHeightDescendants = true;
}

void LayoutBlock::removePercentHeightDescendant(LayoutBox* descendant)
{
    if (TrackedLayoutBoxListHashSet* descendants = percentHeightDescendants()) {
        descendants->remove(descendant);
        descendant->setPercentHeightContainer(nullptr);
        if (descendants->isEmpty()) {
            gPercentHeightDescendantsMap->remove(this);
            m_hasPercentHeightDescendants = false;
        }
    }
}

TrackedLayoutBoxListHashSet* LayoutBlock::percentHeightDescendantsInternal() const
{
    return gPercentHeightDescendantsMap ? gPercentHeightDescendantsMap->get(this) : nullptr;
}

void LayoutBlock::dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope& layoutScope)
{
    TrackedLayoutBoxListHashSet* descendants = percentHeightDescendants();
    if (!descendants)
        return;

    for (auto* box : *descendants) {
        ASSERT(box->isDescendantOf(this));
        while (box != this) {
            if (box->normalChildNeedsLayout())
                break;
            layoutScope.setChildNeedsLayout(box);
            box = box->containingBlock();
            ASSERT(box);
            if (!box)
                break;
        }
    }
}

LayoutUnit LayoutBlock::textIndentOffset() const
{
    LayoutUnit cw;
    if (style()->textIndent().hasPercent())
        cw = containingBlock()->availableLogicalWidth();
    return minimumValueForLength(style()->textIndent(), cw);
}

bool LayoutBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) const
{
    if (!scrollsOverflow())
        return false;

    return layer()->getScrollableArea()->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
}

Node* LayoutBlock::nodeForHitTest() const
{
    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    return isAnonymousBlockContinuation() ? continuation()->node() : node();
}

// TODO(pdr): This is too similar to LayoutBox::nodeAtPoint and should share
//            more code, or merge into LayoutBox::nodeAtPoint entirely.
bool LayoutBlock::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    LayoutPoint adjustedLocation(accumulatedOffset + location());
    LayoutSize localOffset = toLayoutSize(adjustedLocation);

    if (!isLayoutView()) {
        // Check if we need to do anything at all.
        // If we have clipping, then we can't have any spillout.
        LayoutRect overflowBox = hasOverflowClip() ? borderBoxRect() : visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedLocation);
        if (!locationInContainer.intersects(overflowBox))
            return false;
    }

    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground)
        && visibleToHitTestRequest(result.hitTestRequest())
        && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (result.addNodeToListBasedTestResult(nodeForHitTest(), locationInContainer) == StopHitTesting)
            return true;
    }

    // TODO(pdr): We should also include checks for hit testing border radius at
    //            the layer level (see: crbug.com/568904).

    if (hitTestChildren(result, locationInContainer, adjustedLocation, hitTestAction))
        return true;

    if (hitTestClippedOutByRoundedBorder(locationInContainer, adjustedLocation))
        return false;

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (visibleToHitTestRequest(result.hitTestRequest()) && locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            if (result.addNodeToListBasedTestResult(nodeForHitTest(), locationInContainer, boundsRect) == StopHitTesting)
                return true;
        }
    }

    return false;
}

bool LayoutBlock::hitTestChildren(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // TODO(pdr): We should also check for css clip in the !isSelfPaintingLayer
    //            case, similar to overflow clip below.
    if (hasOverflowClip() && !hasSelfPaintingLayer()) {
        if (!locationInContainer.intersects(overflowClipRect(accumulatedOffset, ExcludeOverlayScrollbarSizeForHitTesting)))
            return false;
        if (style()->hasBorderRadius()) {
            LayoutRect borderRect = borderBoxRect();
            borderRect.moveBy(accumulatedOffset);
            if (!locationInContainer.intersects(style()->getRoundedInnerBorderFor(borderRect)))
                return false;
        }
    }

    // A control clip can also clip out child hit testing.
    if (hasControlClip() && !locationInContainer.intersects(controlClipRect(accumulatedOffset)))
        return false;

    LayoutPoint scrolledOffset(hasOverflowClip() ? accumulatedOffset - scrolledContentOffset() : accumulatedOffset);
    if (childrenInline() && !isTable()) {
        if (m_lineBoxes.hitTest(LineLayoutBoxModel(this), result, locationInContainer, scrolledOffset, hitTestAction)) {
            updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - accumulatedOffset)));
            return true;
        }
    } else {
        HitTestAction childHitTest = hitTestAction;
        if (hitTestAction == HitTestChildBlockBackgrounds)
            childHitTest = HitTestChildBlockBackground;
        for (LayoutBox* child = lastChildBox(); child; child = child->previousSiblingBox()) {
            LayoutPoint childPoint = flipForWritingModeForChild(child, scrolledOffset);
            if (!child->hasSelfPaintingLayer() && !child->isFloating() && !child->isColumnSpanAll() && child->nodeAtPoint(result, locationInContainer, childPoint, childHitTest)) {
                updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - accumulatedOffset)));
                return true;
            }
        }
    }

    if (hitTestAction == HitTestFloat && hitTestFloats(result, locationInContainer, scrolledOffset))
        return true;

    return false;
}

Position LayoutBlock::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->getLineLayoutItem().nonPseudoNode())
        return Position::editingPositionOf(nonPseudoNode(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return Position::editingPositionOf(box->getLineLayoutItem().nonPseudoNode(), start ? box->getLineLayoutItem().caretMinOffset() : box->getLineLayoutItem().caretMaxOffset());

    InlineTextBox* textBox = toInlineTextBox(box);
    return Position::editingPositionOf(box->getLineLayoutItem().nonPseudoNode(), start ? textBox->start() : textBox->start() + textBox->len());
}

static inline bool isEditingBoundary(LayoutObject* ancestor, LineLayoutBox child)
{
    ASSERT(!ancestor || ancestor->nonPseudoNode());
    ASSERT(child && child.nonPseudoNode());
    return !ancestor || !ancestor->parent() || (ancestor->hasLayer() && ancestor->parent()->isLayoutView())
        || ancestor->nonPseudoNode()->hasEditableStyle() == child.nonPseudoNode()->hasEditableStyle();
}

// FIXME: This function should go on LayoutObject as an instance method. Then
// all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
static PositionWithAffinity positionForPointRespectingEditingBoundaries(LayoutBlock* parent, LineLayoutBox child, const LayoutPoint& pointInParentCoordinates)
{
    LayoutPoint childLocation = child.location();
    if (child.isInFlowPositioned())
        childLocation += child.offsetForInFlowPosition();

    // FIXME: This is wrong if the child's writing-mode is different from the parent's.
    LayoutPoint pointInChildCoordinates(toLayoutPoint(pointInParentCoordinates - childLocation));

    // If this is an anonymous layoutObject, we just recur normally
    Node* childNode = child.nonPseudoNode();
    if (!childNode)
        return child.positionForPoint(pointInChildCoordinates);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    LayoutObject* ancestor = parent;
    while (ancestor && !ancestor->nonPseudoNode())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child.positionForPoint(pointInChildCoordinates);

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = parent->logicalWidthForChildSize(child.size()) / 2;
    LayoutUnit logicalLeft = parent->isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createPositionWithAffinity(childNode->nodeIndex());
    return ancestor->createPositionWithAffinity(childNode->nodeIndex() + 1, TextAffinity::Upstream);
}

PositionWithAffinity LayoutBlock::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents)
{
    ASSERT(childrenInline());

    if (!firstRootBox())
        return createPositionWithAffinity(0);

    bool linesAreFlipped = style()->isFlippedLinesWritingMode();
    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineBox* closestBox = nullptr;
    RootInlineBox* firstRootBoxWithChildren = nullptr;
    RootInlineBox* lastRootBoxWithChildren = nullptr;
    for (RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox()) {
        if (!root->firstLeafChild())
            continue;
        if (!firstRootBoxWithChildren)
            firstRootBoxWithChildren = root;

        if (!linesAreFlipped && root->isFirstAfterPageBreak() && (pointInLogicalContents.y() < root->lineTopWithLeading()
            || (blocksAreFlipped && pointInLogicalContents.y() == root->lineTopWithLeading())))
            break;

        lastRootBoxWithChildren = root;

        // check if this root line box is located at this y coordinate
        if (pointInLogicalContents.y() < root->selectionBottom() || (blocksAreFlipped && pointInLogicalContents.y() == root->selectionBottom())) {
            if (linesAreFlipped) {
                RootInlineBox* nextRootBoxWithChildren = root->nextRootBox();
                while (nextRootBoxWithChildren && !nextRootBoxWithChildren->firstLeafChild())
                    nextRootBoxWithChildren = nextRootBoxWithChildren->nextRootBox();

                if (nextRootBoxWithChildren && nextRootBoxWithChildren->isFirstAfterPageBreak() && (pointInLogicalContents.y() > nextRootBoxWithChildren->lineTopWithLeading()
                    || (!blocksAreFlipped && pointInLogicalContents.y() == nextRootBoxWithChildren->lineTopWithLeading())))
                    continue;
            }
            closestBox = root->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
            if (closestBox)
                break;
        }
    }

    bool moveCaretToBoundary = document().frame()->editor().behavior().shouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

    if (!moveCaretToBoundary && !closestBox && lastRootBoxWithChildren) {
        // y coordinate is below last root line box, pretend we hit it
        closestBox = lastRootBoxWithChildren->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
    }

    if (closestBox) {
        if (moveCaretToBoundary) {
            LayoutUnit firstRootBoxWithChildrenTop = std::min<LayoutUnit>(firstRootBoxWithChildren->selectionTop(), firstRootBoxWithChildren->logicalTop());
            if (pointInLogicalContents.y() < firstRootBoxWithChildrenTop
                || (blocksAreFlipped && pointInLogicalContents.y() == firstRootBoxWithChildrenTop)) {
                InlineBox* box = firstRootBoxWithChildren->firstLeafChild();
                if (box->isLineBreak()) {
                    if (InlineBox* newBox = box->nextLeafChildIgnoringLineBreak())
                        box = newBox;
                }
                // y coordinate is above first root line box, so return the start of the first
                return PositionWithAffinity(positionForBox(box, true));
            }
        }

        // pass the box a top position that is inside it
        LayoutPoint point(pointInLogicalContents.x(), closestBox->root().blockDirectionPointInLine());
        if (!isHorizontalWritingMode())
            point = point.transposedPoint();
        if (closestBox->getLineLayoutItem().isAtomicInlineLevel())
            return positionForPointRespectingEditingBoundaries(this, LineLayoutBox(closestBox->getLineLayoutItem()), point);
        return closestBox->getLineLayoutItem().positionForPoint(point);
    }

    if (lastRootBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        ASSERT(moveCaretToBoundary);
        InlineBox* logicallyLastBox;
        if (lastRootBoxWithChildren->getLogicalEndBoxWithNode(logicallyLastBox))
            return PositionWithAffinity(positionForBox(logicallyLastBox, false));
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createPositionWithAffinity(0);
}

static inline bool isChildHitTestCandidate(LayoutBox* box)
{
    return box->size().height() && box->style()->visibility() == VISIBLE && !box->isFloatingOrOutOfFlowPositioned() && !box->isLayoutFlowThread();
}

PositionWithAffinity LayoutBlock::positionForPoint(const LayoutPoint& point)
{
    if (isTable())
        return LayoutBox::positionForPoint(point);

    if (isAtomicInlineLevel()) {
        // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
        LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
        LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

        if (pointLogicalLeft < 0)
            return createPositionWithAffinity(caretMinOffset());
        if (pointLogicalLeft >= logicalWidth())
            return createPositionWithAffinity(caretMaxOffset());
        if (pointLogicalTop < 0)
            return createPositionWithAffinity(caretMinOffset());
        if (pointLogicalTop >= logicalHeight())
            return createPositionWithAffinity(caretMaxOffset());
    }

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    if (childrenInline())
        return positionForPointWithInlineChildren(pointInLogicalContents);

    LayoutBox* lastCandidateBox = lastChildBox();
    while (lastCandidateBox && !isChildHitTestCandidate(lastCandidateBox))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(*lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(*lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(this, LineLayoutBox(lastCandidateBox), pointInContents);

        for (LayoutBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
            if (!isChildHitTestCandidate(childBox))
                continue;
            LayoutUnit childLogicalBottom = logicalTopForChild(*childBox) + logicalHeightForChild(*childBox);
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (isChildHitTestCandidate(childBox) && (pointInLogicalContents.y() < childLogicalBottom
                || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom)))
                return positionForPointRespectingEditingBoundaries(this, LineLayoutBox(childBox), pointInContents);
        }
    }

    // We only get here if there are no hit test candidate children below the click.
    return LayoutBox::positionForPoint(point);
}

void LayoutBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);

    if (hasOverflowClip())
        offset += LayoutSize(scrolledContentOffset());

    offset = flipForWritingMode(offset);
}

int LayoutBlock::columnGap() const
{
    if (style()->hasNormalColumnGap())
        return style()->getFontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style()->columnGap());
}

void LayoutBlock::scrollbarsChanged(bool horizontalScrollbarChanged, bool verticalScrollbarChanged)
{
    m_widthAvailableToChildrenChanged |= verticalScrollbarChanged;
    m_heightAvailableToChildrenChanged |= horizontalScrollbarChanged;
}

void LayoutBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Size-contained elements don't consider their contents for preferred sizing.
    if (style()->containsSize())
        return;

    if (childrenInline()) {
        // FIXME: Remove this const_cast.
        toLayoutBlockFlow(const_cast<LayoutBlock*>(this))->computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    } else {
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    if (isHTMLMarqueeElement(node()) && toHTMLMarqueeElement(node())->isHorizontal())
        minLogicalWidth = LayoutUnit();

    if (isTableCell()) {
        Length tableCellWidth = toLayoutTableCell(this)->styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(tableCellWidth.value())));
    }

    int scrollbarWidth = intrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void LayoutBlock::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = LayoutUnit();
    m_maxPreferredLogicalWidth = LayoutUnit();

    // FIXME: The isFixed() calls here should probably be checking for isSpecified since you
    // should be able to use percentage, calc or viewport relative values for width.
    const ComputedStyle& styleToUse = styleRef();
    if (!isTableCell() && styleToUse.logicalWidth().isFixed() && styleToUse.logicalWidth().value() >= 0
        && !(isDeprecatedFlexItem() && !styleToUse.logicalWidth().intValue()))
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalWidth().value()));
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMinWidth().value())));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMinWidth().value())));
    }

    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMaxWidth().value())));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit(styleToUse.logicalMaxWidth().value())));
    }

    // Table layout uses integers, ceil the preferred widths to ensure that they can contain the contents.
    if (isTableCell()) {
        m_minPreferredLogicalWidth = LayoutUnit(m_minPreferredLogicalWidth.ceil());
        m_maxPreferredLogicalWidth = LayoutUnit(m_maxPreferredLogicalWidth.ceil());
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    clearPreferredLogicalWidthsDirty();
}

void LayoutBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const ComputedStyle& styleToUse = styleRef();
    bool nowrap = styleToUse.whiteSpace() == NOWRAP;

    LayoutObject* child = firstChild();
    LayoutBlock* containingBlock = this->containingBlock();
    LayoutUnit floatLeftWidth, floatRightWidth;
    while (child) {
        // Positioned children don't affect the min/max width. Spanners only affect the min/max
        // width of the multicol container, not the flow thread.
        if (child->isOutOfFlowPositioned() || child->isColumnSpanAll()) {
            child = child->nextSibling();
            continue;
        }

        RefPtr<ComputedStyle> childStyle = child->mutableStyle();
        if (child->isFloating() || (child->isBox() && toLayoutBox(child)->avoidsFloats())) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (childStyle->clear() & ClearLeft) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = LayoutUnit();
            }
            if (childStyle->clear() & ClearRight) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = LayoutUnit();
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length startMarginLength = childStyle->marginStartUsing(&styleToUse);
        Length endMarginLength = childStyle->marginEndUsing(&styleToUse);
        LayoutUnit margin;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        if (startMarginLength.isFixed())
            marginStart += startMarginLength.value();
        if (endMarginLength.isFixed())
            marginEnd += endMarginLength.value();
        margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
        computeChildPreferredLogicalWidths(*child, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);

        LayoutUnit w = childMinPreferredLogicalWidth + margin;
        minLogicalWidth = std::max(w, minLogicalWidth);

        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable())
            maxLogicalWidth = std::max(w, maxLogicalWidth);

        w = childMaxPreferredLogicalWidth + margin;

        if (!child->isFloating()) {
            if (child->isBox() && toLayoutBox(child)->avoidsFloats()) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                bool ltr = containingBlock ? containingBlock->style()->isLeftToRightDirection() : styleToUse.isLeftToRightDirection();
                LayoutUnit marginLogicalLeft = ltr ? marginStart : marginEnd;
                LayoutUnit marginLogicalRight = ltr ? marginEnd : marginStart;
                LayoutUnit maxLeft = marginLogicalLeft > 0 ? std::max(floatLeftWidth, marginLogicalLeft) : floatLeftWidth + marginLogicalLeft;
                LayoutUnit maxRight = marginLogicalRight > 0 ? std::max(floatRightWidth, marginLogicalRight) : floatRightWidth + marginLogicalRight;
                w = childMaxPreferredLogicalWidth + maxLeft + maxRight;
                w = std::max(w, floatLeftWidth + floatRightWidth);
            } else {
                maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
            }
            floatLeftWidth = floatRightWidth = LayoutUnit();
        }

        if (child->isFloating()) {
            if (childStyle->floating() == LeftFloat)
                floatLeftWidth += w;
            else
                floatRightWidth += w;
        } else {
            maxLogicalWidth = std::max(w, maxLogicalWidth);
        }

        child = child->nextSibling();
    }

    // Always make sure these values are non-negative.
    minLogicalWidth = minLogicalWidth.clampNegativeToZero();
    maxLogicalWidth = maxLogicalWidth.clampNegativeToZero();

    maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
}

void LayoutBlock::computeChildPreferredLogicalWidths(LayoutObject& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    if (child.isBox() && child.isHorizontalWritingMode() != isHorizontalWritingMode()) {
        // If the child is an orthogonal flow, child's height determines the width, but the height is not available until layout.
        // http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-shrink-to-fit
        if (!child.needsLayout()) {
            minPreferredLogicalWidth = maxPreferredLogicalWidth = toLayoutBox(child).logicalHeight();
            return;
        }
        minPreferredLogicalWidth = maxPreferredLogicalWidth = toLayoutBox(child).computeLogicalHeightWithoutLayout();
        return;
    }
    minPreferredLogicalWidth = child.minPreferredLogicalWidth();
    maxPreferredLogicalWidth = child.maxPreferredLogicalWidth();

    // For non-replaced blocks if the inline size is min|max-content or a definite size the min|max-content contribution
    // is that size plus border, padding and margin https://drafts.csswg.org/css-sizing/#block-intrinsic
    if (child.isLayoutBlock()) {
        const Length& computedInlineSize = child.styleRef().logicalWidth();
        if (computedInlineSize.isMaxContent())
            minPreferredLogicalWidth = maxPreferredLogicalWidth;
        else if (computedInlineSize.isMinContent())
            maxPreferredLogicalWidth = minPreferredLogicalWidth;
    }
}

bool LayoutBlock::hasLineIfEmpty() const
{
    if (!node())
        return false;

    if (node()->isRootEditableElement())
        return true;

    if (node()->isShadowRoot() && isHTMLInputElement(*toShadowRoot(node())->host()))
        return true;

    return false;
}

LayoutUnit LayoutBlock::lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isAtomicInlineLevel() && linePositionMode == PositionOnContainingLine)
        return LayoutBox::lineHeight(firstLine, direction, linePositionMode);

    const ComputedStyle& style = styleRef(firstLine && document().styleEngine().usesFirstLineRules());
    return LayoutUnit(style.computedLineHeight());
}

int LayoutBlock::beforeMarginInLineDirection(LineDirectionMode direction) const
{
    return direction == HorizontalLine ? marginTop() : marginRight();
}

int LayoutBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isInline() && linePositionMode == PositionOnContainingLine) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        // FIXME: Need to patch form controls to deal with vertical lines.
        if (style()->hasAppearance() && !LayoutTheme::theme().isControlContainer(style()->appearance()))
            return LayoutTheme::theme().baselinePosition(this);

        int baselinePos = (isWritingModeRoot() && !isRubyRun()) ? -1 : inlineBlockBaseline(direction);

        if (isDeprecatedFlexibleBox()) {
            // Historically, we did this check for all baselines. But we can't
            // remove this code from deprecated flexbox, because it effectively
            // breaks -webkit-line-clamp, which is used in the wild -- we would
            // calculate the baseline as if -webkit-line-clamp wasn't used.
            // For simplicity, we use this for all uses of deprecated flexbox.
            LayoutUnit bottomOfContent = direction == HorizontalLine ? size().height() - borderBottom() - paddingBottom() - horizontalScrollbarHeight() : size().width() - borderLeft() - paddingLeft() - verticalScrollbarWidth();
            if (baselinePos > bottomOfContent)
                baselinePos = -1;
        }
        if (baselinePos != -1)
            return beforeMarginInLineDirection(direction) + baselinePos;

        return LayoutBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
    }

    // If we're not replaced, we'll only get called with PositionOfInteriorLineBoxes.
    // Note that inline-block counts as replaced here.
    ASSERT(linePositionMode == PositionOfInteriorLineBoxes);

    const FontMetrics& fontMetrics = style(firstLine)->getFontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
}

LayoutUnit LayoutBlock::minLineHeightForReplacedObject(bool isFirstLine, LayoutUnit replacedHeight) const
{
    if (!document().inNoQuirksMode() && replacedHeight)
        return replacedHeight;

    return std::max<LayoutUnit>(replacedHeight, lineHeight(isFirstLine, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

int LayoutBlock::firstLineBoxBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (childrenInline()) {
        if (firstLineBox())
            return firstLineBox()->logicalTop() + style(true)->getFontMetrics().ascent(firstRootBox()->baselineType());
        return -1;
    }
    for (LayoutBox* curr = firstChildBox(); curr; curr = curr->nextSiblingBox()) {
        if (!curr->isFloatingOrOutOfFlowPositioned()) {
            int result = curr->firstLineBoxBaseline();
            if (result != -1)
                return curr->logicalTop() + result; // Translate to our coordinate space.
        }
    }
    return -1;
}

int LayoutBlock::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    // CSS2.1 states that the baseline of an 'inline-block' is:
    // the baseline of the last line box in the normal flow, unless it has
    // either no in-flow line boxes or if its 'overflow' property has a computed
    // value other than 'visible', in which case the baseline is the bottom
    // margin edge.
    // We likewise avoid using the last line box in the case of size containment,
    // where the block's contents shouldn't be considered when laying out its
    // ancestors or siblings.

    if ((!style()->isOverflowVisible() && !shouldIgnoreOverflowPropertyForInlineBlockBaseline()) || style()->containsSize()) {
        // We are not calling LayoutBox::baselinePosition here because the caller should add the margin-top/margin-right, not us.
        return lineDirection == HorizontalLine ? size().height() + marginBottom() : size().width() + marginLeft();
    }

    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (childrenInline()) {
        if (!firstLineBox() && hasLineIfEmpty()) {
            const FontMetrics& fontMetrics = firstLineStyle()->getFontMetrics();
            return fontMetrics.ascent()
                + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
                + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
        }
        if (lastLineBox())
            return lastLineBox()->logicalTop() + style(lastLineBox() == firstLineBox())->getFontMetrics().ascent(lastRootBox()->baselineType());
        return -1;
    }

    bool haveNormalFlowChild = false;
    for (LayoutBox* curr = lastChildBox(); curr; curr = curr->previousSiblingBox()) {
        if (!curr->isFloatingOrOutOfFlowPositioned()) {
            haveNormalFlowChild = true;
            int result = curr->inlineBlockBaseline(lineDirection);
            if (result != -1)
                return curr->logicalTop() + result; // Translate to our coordinate space.
        }
    }
    if (!haveNormalFlowChild && hasLineIfEmpty()) {
        const FontMetrics& fontMetrics = firstLineStyle()->getFontMetrics();
        return fontMetrics.ascent()
            + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
            + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
    }
    return -1;
}

LayoutBlock* LayoutBlock::enclosingFirstLineStyleBlock() const
{
    LayoutBlock* firstLineBlock = const_cast<LayoutBlock*>(this);
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style()->hasPseudoStyle(PseudoIdFirstLine);
        if (hasPseudo)
            break;
        LayoutObject* parentBlock = firstLineBlock->parent();
        if (firstLineBlock->isAtomicInlineLevel() || firstLineBlock->isFloatingOrOutOfFlowPositioned()
            || !parentBlock
            || !parentBlock->canHaveFirstLineOrFirstLetterStyle())
            break;
        ASSERT_WITH_SECURITY_IMPLICATION(parentBlock->isLayoutBlock());
        if (toLayoutBlock(parentBlock)->firstChild() != firstLineBlock)
            break;
        firstLineBlock = toLayoutBlock(parentBlock);
    }

    if (!hasPseudo)
        return nullptr;

    return firstLineBlock;
}

LayoutBlockFlow* LayoutBlock::nearestInnerBlockWithFirstLine() const
{
    if (childrenInline())
        return toLayoutBlockFlow(const_cast<LayoutBlock*>(this));
    for (LayoutObject* child = firstChild(); child && !child->isFloatingOrOutOfFlowPositioned() && child->isLayoutBlockFlow(); child = toLayoutBlock(child)->firstChild()) {
        if (child->childrenInline())
            return toLayoutBlockFlow(child);
    }
    return nullptr;
}

// Helper methods for obtaining the last line, computing line counts and heights for line counts
// (crawling into blocks).
static bool shouldCheckLines(LayoutObject* obj)
{
    return !obj->isFloatingOrOutOfFlowPositioned()
        && obj->isLayoutBlock() && obj->style()->height().isAuto()
        && (!obj->isDeprecatedFlexibleBox() || obj->style()->boxOrient() == VERTICAL);
}

static int getHeightForLineCount(LayoutBlock* block, int lineCount, bool includeBottom, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->isLayoutBlockFlow() && block->childrenInline()) {
            for (RootInlineBox* box = toLayoutBlockFlow(block)->firstRootBox(); box; box = box->nextRootBox()) {
                if (++count == lineCount)
                    return box->lineBottom() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : LayoutUnit());
            }
        } else {
            LayoutBox* normalFlowChildWithoutLines = nullptr;
            for (LayoutBox* obj = block->firstChildBox(); obj; obj = obj->nextSiblingBox()) {
                if (shouldCheckLines(obj)) {
                    int result = getHeightForLineCount(toLayoutBlock(obj), lineCount, false, count);
                    if (result != -1)
                        return result + obj->location().y() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : LayoutUnit());
                } else if (!obj->isFloatingOrOutOfFlowPositioned()) {
                    normalFlowChildWithoutLines = obj;
                }
            }
            if (normalFlowChildWithoutLines && lineCount == 0)
                return normalFlowChildWithoutLines->location().y() + normalFlowChildWithoutLines->size().height();
        }
    }

    return -1;
}

RootInlineBox* LayoutBlock::lineAtIndex(int i) const
{
    ASSERT(i >= 0);

    if (style()->visibility() != VISIBLE)
        return nullptr;

    if (childrenInline()) {
        for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox()) {
            if (!i--)
                return box;
        }
    } else {
        for (LayoutObject* child = firstChild(); child; child = child->nextSibling()) {
            if (!shouldCheckLines(child))
                continue;
            if (RootInlineBox* box = toLayoutBlock(child)->lineAtIndex(i))
                return box;
        }
    }

    return nullptr;
}

int LayoutBlock::lineCount(const RootInlineBox* stopRootInlineBox, bool* found) const
{
    int count = 0;

    if (style()->visibility() == VISIBLE) {
        if (childrenInline()) {
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox()) {
                count++;
                if (box == stopRootInlineBox) {
                    if (found)
                        *found = true;
                    break;
                }
            }
        } else {
            for (LayoutObject* obj = firstChild(); obj; obj = obj->nextSibling()) {
                if (shouldCheckLines(obj)) {
                    bool recursiveFound = false;
                    count += toLayoutBlock(obj)->lineCount(stopRootInlineBox, &recursiveFound);
                    if (recursiveFound) {
                        if (found)
                            *found = true;
                        break;
                    }
                }
            }
        }
    }
    return count;
}

int LayoutBlock::heightForLineCount(int lineCount)
{
    int count = 0;
    return getHeightForLineCount(this, lineCount, true, count);
}

void LayoutBlock::clearTruncation()
{
    if (style()->visibility() == VISIBLE) {
        if (childrenInline() && hasMarkupTruncation()) {
            setHasMarkupTruncation(false);
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                box->clearTruncation();
        } else {
            for (LayoutObject* obj = firstChild(); obj; obj = obj->nextSibling()) {
                if (shouldCheckLines(obj))
                    toLayoutBlock(obj)->clearTruncation();
            }
        }
    }
}

void LayoutBlock::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for vertical writing-modes.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        LayoutRect rect(accumulatedOffset, size());
        rect.expand(collapsedMarginBoxLogicalOutsets());
        rects.append(pixelSnappedIntRect(rect));
        continuation()->absoluteRects(rects, accumulatedOffset - toLayoutSize(location() +
            inlineElementContinuation()->containingBlock()->location()));
    } else {
        rects.append(pixelSnappedIntRect(accumulatedOffset, size()));
    }
}

void LayoutBlock::absoluteQuads(Vector<FloatQuad>& quads) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for vertical writing-modes.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        LayoutRect localRect(LayoutPoint(), size());
        localRect.expand(collapsedMarginBoxLogicalOutsets());
        quads.append(localToAbsoluteQuad(FloatRect(localRect)));
        continuation()->absoluteQuads(quads);
    } else {
        quads.append(LayoutBox::localToAbsoluteQuad(FloatRect(0, 0, size().width().toFloat(), size().height().toFloat())));
    }
}

LayoutObject* LayoutBlock::hoverAncestor() const
{
    return isAnonymousBlockContinuation() ? continuation() : LayoutBox::hoverAncestor();
}

void LayoutBlock::updateDragState(bool dragOn)
{
    LayoutBox::updateDragState(dragOn);
    if (LayoutBoxModelObject* continuation = this->continuation())
        continuation->updateDragState(dragOn);
}

void LayoutBlock::childBecameNonInline(LayoutObject*)
{
    makeChildrenNonInline();
    if (isAnonymousBlock() && parent() && parent()->isLayoutBlock())
        toLayoutBlock(parent())->removeLeftoverAnonymousBlock(this);
    // |this| may be dead here
}

void LayoutBlock::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    if (Node* n = nodeForHitTest())
        result.setNodeAndPosition(n, point);
}

// An inline-block uses its inlineBox as the inlineBoxWrapper,
// so the firstChild() is nullptr if the only child is an empty inline-block.
inline bool LayoutBlock::isInlineBoxWrapperActuallyChild() const
{
    return isInlineBlockOrInlineTable() && !size().isEmpty() && node() && editingIgnoresContent(node());
}

static inline bool caretBrowsingEnabled(const LocalFrame* frame)
{
    Settings* settings = frame->settings();
    return settings && settings->caretBrowsingEnabled();
}

bool LayoutBlock::hasCursorCaret() const
{
    LocalFrame* frame = this->frame();
    return frame->selection().caretLayoutObject() == this && (frame->selection().hasEditableStyle() || caretBrowsingEnabled(frame));
}

bool LayoutBlock::hasDragCaret() const
{
    LocalFrame* frame = this->frame();
    DragCaretController& dragCaretController = frame->page()->dragCaretController();
    return dragCaretController.caretLayoutObject() == this && (dragCaretController.isContentEditable() || caretBrowsingEnabled(frame));
}

LayoutRect LayoutBlock::localCaretRect(InlineBox* inlineBox, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    // Do the normal calculation in most cases.
    if (firstChild() || isInlineBoxWrapperActuallyChild())
        return LayoutBox::localCaretRect(inlineBox, caretOffset, extraWidthToEndOfLine);

    LayoutRect caretRect = localCaretRectForEmptyElement(size().width(), textIndentOffset());

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = size().width() - caretRect.maxX();

    return caretRect;
}

void LayoutBlock::addOutlineRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot includeBlockOverflows) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    const LayoutInline* inlineElementContinuation = this->inlineElementContinuation();
    if (inlineElementContinuation) {
        // FIXME: This check really isn't accurate.
        bool nextInlineHasLineBox = inlineElementContinuation->firstLineBox();
        // FIXME: This is wrong. The principal layoutObject may not be the continuation preceding this block.
        // FIXME: This is wrong for vertical writing-modes.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = toLayoutInline(inlineElementContinuation->node()->layoutObject())->firstLineBox();
        LayoutUnit topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : LayoutUnit();
        LayoutUnit bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : LayoutUnit();
        if (topMargin || bottomMargin) {
            LayoutRect rect(additionalOffset, size());
            rect.expandEdges(topMargin, LayoutUnit(), bottomMargin, LayoutUnit());
            rects.append(rect);
        }
    } else if (!isAnonymous()) { // For anonymous blocks, the children add outline rects.
        rects.append(LayoutRect(additionalOffset, size()));
    }

    if (includeBlockOverflows == IncludeBlockVisualOverflow && !hasOverflowClip() && !hasControlClip()) {
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
            LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
            LayoutRect rect(additionalOffset.x() + curr->x(), additionalOffset.y() + top, curr->width(), bottom - top);
            if (!rect.isEmpty())
                rects.append(rect);
        }

        addOutlineRectsForNormalChildren(rects, additionalOffset, includeBlockOverflows);
        if (TrackedLayoutBoxListHashSet* positionedObjects = this->positionedObjects()) {
            for (auto* box : *positionedObjects)
                addOutlineRectsForDescendant(*box, rects, additionalOffset, includeBlockOverflows);
        }
    }

    if (inlineElementContinuation)
        inlineElementContinuation->addOutlineRects(rects, additionalOffset + (inlineElementContinuation->containingBlock()->location() - location()), includeBlockOverflows);
}

void LayoutBlock::computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const
{
    LayoutBox::computeSelfHitTestRects(rects, layerOffset);

    if (hasHorizontalLayoutOverflow() || hasVerticalLayoutOverflow()) {
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
            LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
            LayoutRect rect(layerOffset.x() + curr->x(), layerOffset.y() + top, curr->width(), bottom - top);
            // It's common for this rect to be entirely contained in our box, so exclude that simple case.
            if (!rect.isEmpty() && (rects.isEmpty() || !rects[0].contains(rect)))
                rects.append(rect);
        }
    }
}

LayoutBox* LayoutBlock::createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const
{
    return createAnonymousWithParentAndDisplay(parent, style()->display());
}

LayoutUnit LayoutBlock::nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return logicalOffset;

    return logicalOffset + pageRemainingLogicalHeightForOffset(logicalOffset, pageBoundaryRule);
}

LayoutUnit LayoutBlock::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    LayoutView* layoutView = view();
    LayoutFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread)
        return layoutView->layoutState()->pageLogicalHeight();
    return flowThread->pageLogicalHeightForOffset(offset + offsetFromLogicalTopOfFirstPage());
}

LayoutUnit LayoutBlock::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutView* layoutView = view();
    offset += offsetFromLogicalTopOfFirstPage();

    LayoutFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        LayoutUnit pageLogicalHeight = layoutView->layoutState()->pageLogicalHeight();
        LayoutUnit remainingHeight = pageLogicalHeight - intMod(offset, pageLogicalHeight);
        if (pageBoundaryRule == AssociateWithFormerPage) {
            // An offset exactly at a page boundary will act as being part of the former page in
            // question (i.e. no remaining space), rather than being part of the latter (i.e. one
            // whole page length of remaining space).
            remainingHeight = intMod(remainingHeight, pageLogicalHeight);
        }
        return remainingHeight;
    }

    return flowThread->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);
}

LayoutUnit LayoutBlock::calculatePaginationStrutToFitContent(LayoutUnit offset, LayoutUnit strutToNextPage, LayoutUnit contentLogicalHeight) const
{
    ASSERT(strutToNextPage == pageRemainingLogicalHeightForOffset(offset, AssociateWithLatterPage));
    LayoutUnit nextPageLogicalTop = offset + strutToNextPage;
    if (pageLogicalHeightForOffset(nextPageLogicalTop) >= contentLogicalHeight)
        return strutToNextPage; // Content fits just fine in the next page or column.

    // Moving to the top of the next page or column doesn't result in enough space for the content
    // that we're trying to fit. If we're in a nested fragmentation context, we may find enough
    // space if we move to a column further ahead, by effectively breaking to the next outer
    // fragmentainer.
    LayoutFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        // If there's no flow thread, we're not nested. All pages have the same height. Give up.
        return strutToNextPage;
    }
    // Start searching for a suitable offset at the top of the next page or column.
    LayoutUnit flowThreadOffset = offsetFromLogicalTopOfFirstPage() + nextPageLogicalTop;
    return strutToNextPage + flowThread->nextLogicalTopForUnbreakableContent(flowThreadOffset, contentLogicalHeight) - flowThreadOffset;
}

void LayoutBlock::paginatedContentWasLaidOut(LayoutUnit logicalBottomOffsetAfterPagination)
{
    if (LayoutFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->contentWasLaidOut(offsetFromLogicalTopOfFirstPage() + logicalBottomOffsetAfterPagination);
}

LayoutUnit LayoutBlock::collapsedMarginBeforeForChild(const LayoutBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginBefore();

    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginAfter();

    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" sides of the child box.  We can just return the raw margin in this case.
    return marginBeforeForChild(child);
}

LayoutUnit LayoutBlock::collapsedMarginAfterForChild(const LayoutBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginAfter();

    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginBefore();

    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" side of the child box.  We can just return the raw margin in this case.
    return marginAfterForChild(child);
}

bool LayoutBlock::hasMarginBeforeQuirk(const LayoutBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child->isWritingModeRoot())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginBeforeQuirk() : child->style()->hasMarginBeforeQuirk();

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginAfterQuirk() : child->style()->hasMarginAfterQuirk();

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

bool LayoutBlock::hasMarginAfterQuirk(const LayoutBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child->isWritingModeRoot())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginAfterQuirk() : child->style()->hasMarginAfterQuirk();

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isLayoutBlock() ? toLayoutBlock(child)->hasMarginBeforeQuirk() : child->style()->hasMarginBeforeQuirk();

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

const char* LayoutBlock::name() const
{
    ASSERT_NOT_REACHED();
    return "LayoutBlock";
}

LayoutBlock* LayoutBlock::createAnonymousWithParentAndDisplay(const LayoutObject* parent, EDisplay display)
{
    // FIXME: Do we need to convert all our inline displays to block-type in the anonymous logic ?
    EDisplay newDisplay;
    LayoutBlock* newBox = nullptr;
    if (display == FLEX || display == INLINE_FLEX) {
        newBox = LayoutFlexibleBox::createAnonymous(&parent->document());
        newDisplay = FLEX;
    } else {
        newBox = LayoutBlockFlow::createAnonymous(&parent->document());
        newDisplay = BLOCK;
    }

    RefPtr<ComputedStyle> newStyle = ComputedStyle::createAnonymousStyleWithDisplay(parent->styleRef(), newDisplay);
    parent->updateAnonymousChildStyle(*newBox, *newStyle);
    newBox->setStyle(newStyle.release());
    return newBox;
}

bool LayoutBlock::recalcNormalFlowChildOverflowIfNeeded(LayoutObject* layoutObject)
{
    if (layoutObject->isOutOfFlowPositioned() || !layoutObject->needsOverflowRecalcAfterStyleChange())
        return false;

    ASSERT(layoutObject->isLayoutBlock());
    return toLayoutBlock(layoutObject)->recalcOverflowAfterStyleChange();
}

bool LayoutBlock::recalcChildOverflowAfterStyleChange()
{
    ASSERT(childNeedsOverflowRecalcAfterStyleChange());
    clearChildNeedsOverflowRecalcAfterStyleChange();

    bool childrenOverflowChanged = false;

    if (childrenInline()) {
        ASSERT_WITH_SECURITY_IMPLICATION(isLayoutBlockFlow());
        childrenOverflowChanged = toLayoutBlockFlow(this)->recalcInlineChildrenOverflowAfterStyleChange();
    } else {
        for (LayoutBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (recalcNormalFlowChildOverflowIfNeeded(box))
                childrenOverflowChanged = true;
        }
    }

    TrackedLayoutBoxListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return childrenOverflowChanged;

    for (auto* box : *positionedDescendants) {
        if (!box->needsOverflowRecalcAfterStyleChange())
            continue;
        LayoutBlock* block = toLayoutBlock(box);
        if (!block->recalcOverflowAfterStyleChange() || box->style()->position() == FixedPosition)
            continue;

        childrenOverflowChanged = true;
    }
    return childrenOverflowChanged;
}

bool LayoutBlock::recalcOverflowAfterStyleChange()
{
    ASSERT(needsOverflowRecalcAfterStyleChange());

    bool childrenOverflowChanged = false;
    if (childNeedsOverflowRecalcAfterStyleChange())
        childrenOverflowChanged = recalcChildOverflowAfterStyleChange();

    if (!selfNeedsOverflowRecalcAfterStyleChange() && !childrenOverflowChanged)
        return false;

    clearSelfNeedsOverflowRecalcAfterStyleChange();
    // If the current block needs layout, overflow will be recalculated during
    // layout time anyway. We can safely exit here.
    if (needsLayout())
        return false;

    LayoutUnit oldClientAfterEdge = hasOverflowModel() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
    computeOverflow(oldClientAfterEdge, true);

    if (hasOverflowClip())
        layer()->getScrollableArea()->updateAfterOverflowRecalc();

    return !hasOverflowClip();
}

// Called when a positioned object moves but doesn't necessarily change size.  A simplified layout is attempted
// that just updates the object's position. If the size does change, the object remains dirty.
bool LayoutBlock::tryLayoutDoingPositionedMovementOnly()
{
    LayoutUnit oldWidth = logicalWidth();
    LogicalExtentComputedValues computedValues;
    logicalExtentAfterUpdatingLogicalWidth(logicalTop(), computedValues);
    // If we shrink to fit our width may have changed, so we still need full layout.
    if (oldWidth != computedValues.m_extent)
        return false;
    setLogicalWidth(computedValues.m_extent);
    setLogicalLeft(computedValues.m_position);
    setMarginStart(computedValues.m_margins.m_start);
    setMarginEnd(computedValues.m_margins.m_end);

    LayoutUnit oldHeight = logicalHeight();
    LayoutUnit oldIntrinsicContentLogicalHeight = intrinsicContentLogicalHeight();

    setIntrinsicContentLogicalHeight(contentLogicalHeight());
    computeLogicalHeight(oldHeight, logicalTop(), computedValues);

    if (hasPercentHeightDescendants() && oldHeight != computedValues.m_extent) {
        setIntrinsicContentLogicalHeight(oldIntrinsicContentLogicalHeight);
        return false;
    }

    setLogicalHeight(computedValues.m_extent);
    setLogicalTop(computedValues.m_position);
    setMarginBefore(computedValues.m_margins.m_before);
    setMarginAfter(computedValues.m_margins.m_after);

    return true;
}

#if ENABLE(ASSERT)
void LayoutBlock::checkPositionedObjectsNeedLayout()
{
    if (!gPositionedDescendantsMap)
        return;

    if (TrackedLayoutBoxListHashSet* positionedDescendantSet = positionedObjects()) {
        TrackedLayoutBoxListHashSet::const_iterator end = positionedDescendantSet->end();
        for (TrackedLayoutBoxListHashSet::const_iterator it = positionedDescendantSet->begin(); it != end; ++it) {
            LayoutBox* currBox = *it;
            ASSERT(!currBox->needsLayout());
        }
    }
}

#endif

#ifndef NDEBUG

void LayoutBlock::showLineTreeAndMark(const InlineBox* markedBox1, const char* markedLabel1, const InlineBox* markedBox2, const char* markedLabel2, const LayoutObject* obj) const
{
    showLayoutObject();
    for (const RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox())
        root->showLineTreeAndMark(markedBox1, markedLabel1, markedBox2, markedLabel2, obj, 1);
}

#endif

} // namespace blink
