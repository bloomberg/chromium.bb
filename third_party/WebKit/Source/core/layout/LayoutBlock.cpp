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

#include "config.h"
#include "core/layout/LayoutBlock.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/htmlediting.h"
#include "core/events/OverflowEvent.h"
#include "core/fetch/ResourceLoadPriorityOptimizer.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/HitTestLocation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/Layer.h"
#include "core/layout/LayoutDeprecatedFlexibleBox.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutRegion.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTextCombine.h"
#include "core/layout/LayoutTextControl.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/shapes/ShapeOutsideInfo.h"
#include "core/layout/style/ContentData.h"
#include "core/layout/style/LayoutStyle.h"
#include "core/page/Page.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/RenderDrawingRecorder.h"
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
    int pageLogicalOffset;
    uint32_t bitfields;
};

static_assert(sizeof(LayoutBlock) == sizeof(SameSizeAsLayoutBlock), "LayoutBlock should stay small");

typedef WTF::HashMap<const LayoutBox*, OwnPtr<ColumnInfo>> ColumnInfoMap;
static ColumnInfoMap* gColumnInfoMap = 0;

static TrackedDescendantsMap* gPositionedDescendantsMap = 0;
static TrackedDescendantsMap* gPercentHeightDescendantsMap = 0;

static TrackedContainerMap* gPositionedContainerMap = 0;
static TrackedContainerMap* gPercentHeightContainerMap = 0;

typedef WTF::HashSet<LayoutBlock*> DelayedUpdateScrollInfoSet;
static int gDelayUpdateScrollInfo = 0;
static DelayedUpdateScrollInfoSet* gDelayedUpdateScrollInfoSet = 0;

static bool gColumnFlowSplitEnabled = true;

// This class helps dispatching the 'overflow' event on layout change. overflow can be set on LayoutBoxes, yet the existing code
// only works on LayoutBlocks. If this changes, this class should be shared with other LayoutBoxes.
class OverflowEventDispatcher {
    WTF_MAKE_NONCOPYABLE(OverflowEventDispatcher);
public:
    OverflowEventDispatcher(const LayoutBlock* block)
        : m_block(block)
        , m_hadHorizontalLayoutOverflow(false)
        , m_hadVerticalLayoutOverflow(false)
    {
        m_shouldDispatchEvent = !m_block->isAnonymous() && m_block->hasOverflowClip() && m_block->document().hasListenerType(Document::OVERFLOWCHANGED_LISTENER);
        if (m_shouldDispatchEvent) {
            m_hadHorizontalLayoutOverflow = m_block->hasHorizontalLayoutOverflow();
            m_hadVerticalLayoutOverflow = m_block->hasVerticalLayoutOverflow();
        }
    }

    ~OverflowEventDispatcher()
    {
        if (!m_shouldDispatchEvent)
            return;

        bool hasHorizontalLayoutOverflow = m_block->hasHorizontalLayoutOverflow();
        bool hasVerticalLayoutOverflow = m_block->hasVerticalLayoutOverflow();

        bool horizontalLayoutOverflowChanged = hasHorizontalLayoutOverflow != m_hadHorizontalLayoutOverflow;
        bool verticalLayoutOverflowChanged = hasVerticalLayoutOverflow != m_hadVerticalLayoutOverflow;

        if (!horizontalLayoutOverflowChanged && !verticalLayoutOverflowChanged)
            return;

        RefPtrWillBeRawPtr<OverflowEvent> event = OverflowEvent::create(horizontalLayoutOverflowChanged, hasHorizontalLayoutOverflow, verticalLayoutOverflowChanged, hasVerticalLayoutOverflow);
        event->setTarget(m_block->node());
        m_block->document().enqueueAnimationFrameEvent(event.release());
    }

private:
    const LayoutBlock* m_block;
    bool m_shouldDispatchEvent;
    bool m_hadHorizontalLayoutOverflow;
    bool m_hadVerticalLayoutOverflow;
};

LayoutBlock::LayoutBlock(ContainerNode* node)
    : LayoutBox(node)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_beingDestroyed(false)
    , m_hasMarkupTruncation(false)
    , m_widthAvailableToChildrenChanged(false)
    , m_hasOnlySelfCollapsingChildren(false)
    , m_descendantsWithFloatsMarkedForLayout(false)
{
    // LayoutBlockFlow calls setChildrenInline(true).
    // By default, subclasses do not have inline children.
}

static void removeBlockFromDescendantAndContainerMaps(LayoutBlock* block, TrackedDescendantsMap*& descendantMap, TrackedContainerMap*& containerMap)
{
    if (OwnPtr<TrackedRendererListHashSet> descendantSet = descendantMap->take(block)) {
        TrackedRendererListHashSet::iterator end = descendantSet->end();
        for (TrackedRendererListHashSet::iterator descendant = descendantSet->begin(); descendant != end; ++descendant) {
            TrackedContainerMap::iterator it = containerMap->find(*descendant);
            ASSERT(it != containerMap->end());
            if (it == containerMap->end())
                continue;
            HashSet<LayoutBlock*>* containerSet = it->value.get();
            ASSERT(containerSet->contains(block));
            containerSet->remove(block);
            if (containerSet->isEmpty())
                containerMap->remove(it);
        }
    }
}

static void appendImageIfNotNull(Vector<ImageResource*>& imageResources, const StyleImage* styleImage)
{
    if (styleImage && styleImage->cachedImage()) {
        ImageResource* imageResource = styleImage->cachedImage();
        if (imageResource && !imageResource->isLoaded())
            imageResources.append(styleImage->cachedImage());
    }
}

static void appendLayers(Vector<ImageResource*>& images, const FillLayer& styleLayer)
{
    for (const FillLayer* layer = &styleLayer; layer; layer = layer->next())
        appendImageIfNotNull(images, layer->image());
}

static void appendImagesFromStyle(Vector<ImageResource*>& images, const LayoutStyle& blockStyle)
{
    appendLayers(images, blockStyle.backgroundLayers());
    appendLayers(images, blockStyle.maskLayers());

    const ContentData* contentData = blockStyle.contentData();
    if (contentData && contentData->isImage())
        appendImageIfNotNull(images, toImageContentData(contentData)->image());
    if (blockStyle.boxReflect())
        appendImageIfNotNull(images, blockStyle.boxReflect()->mask().image());
    appendImageIfNotNull(images, blockStyle.listStyleImage());
    appendImageIfNotNull(images, blockStyle.borderImageSource());
    appendImageIfNotNull(images, blockStyle.maskBoxImageSource());
    if (blockStyle.shapeOutside())
        appendImageIfNotNull(images, blockStyle.shapeOutside()->image());
}

void LayoutBlock::removeFromGlobalMaps()
{
    if (hasColumns())
        gColumnInfoMap->take(this);
    if (gPercentHeightDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
    if (gPositionedDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPositionedDescendantsMap, gPositionedContainerMap);
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
        setContinuation(0);
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

void LayoutBlock::styleWillChange(StyleDifference diff, const LayoutStyle& newStyle)
{
    LayoutStyle* oldStyle = style();

    setReplaced(newStyle.isDisplayInlineType());

    if (oldStyle && parent()) {
        bool oldStyleIsContainer = oldStyle->position() != StaticPosition || oldStyle->hasTransformRelatedProperty();
        bool newStyleIsContainer = newStyle.position() != StaticPosition || newStyle.hasTransformRelatedProperty();

        if (oldStyleIsContainer && !newStyleIsContainer) {
            // Clear our positioned objects list. Our absolutely positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(0, NewContainingBlock);
        } else if (!oldStyleIsContainer && newStyleIsContainer) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            LayoutObject* cb = parent();
            while (cb && (cb->style()->position() == StaticPosition || (cb->isInline() && !cb->isReplaced())) && !cb->isLayoutView()) {
                if (cb->style()->position() == RelativePosition && cb->isInline() && !cb->isReplaced()) {
                    cb = cb->containingBlock();
                    break;
                }
                cb = cb->parent();
            }

            if (cb->isLayoutBlock())
                toLayoutBlock(cb)->removePositionedObjects(this, NewContainingBlock);
        }
    }

    LayoutBox::styleWillChange(diff, newStyle);
}

static bool borderOrPaddingLogicalWidthChanged(const LayoutStyle& oldStyle, const LayoutStyle& newStyle)
{
    if (newStyle.isHorizontalWritingMode()) {
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

void LayoutBlock::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    LayoutBox::styleDidChange(diff, oldStyle);

    if (isFloatingOrOutOfFlowPositioned() && oldStyle && !oldStyle->isFloating() && !oldStyle->hasOutOfFlowPosition() && parent() && parent()->isLayoutBlockFlow())
        toLayoutBlock(parent())->removeAnonymousWrappersIfRequired();

    const LayoutStyle& newStyle = styleRef();

    if (!isAnonymousBlock()) {
        // Ensure that all of our continuation blocks pick up the new style.
        for (LayoutBlock* currCont = blockElementContinuation(); currCont; currCont = currCont->blockElementContinuation()) {
            LayoutBoxModelObject* nextCont = currCont->continuation();
            currCont->setContinuation(0);
            currCont->setStyle(style());
            currCont->setContinuation(nextCont);
        }
    }

    if (TextAutosizer* textAutosizer = document().textAutosizer())
        textAutosizer->record(this);

    propagateStyleToAnonymousChildren(true);

    // It's possible for our border/padding to change, but for the overall logical width of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    m_widthAvailableToChildrenChanged |= oldStyle && diff.needsFullLayout() && needsLayout() && borderOrPaddingLogicalWidthChanged(*oldStyle, newStyle);

    // If the style has unloaded images, want to notify the ResourceLoadPriorityOptimizer so that
    // network priorities can be set.
    Vector<ImageResource*> images;
    appendImagesFromStyle(images, newStyle);
    if (images.isEmpty())
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->removeLayoutObject(this);
    else
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->addLayoutObject(this);
}

void LayoutBlock::invalidatePaintOfSubtreesIfNeeded(const PaintInvalidationState& childPaintInvalidationState)
{
    LayoutBox::invalidatePaintOfSubtreesIfNeeded(childPaintInvalidationState);

    // Take care of positioned objects. This is required as PaintInvalidationState keeps a single clip rect.
    if (TrackedRendererListHashSet* positionedObjects = this->positionedObjects()) {
        TrackedRendererListHashSet::iterator end = positionedObjects->end();
        for (TrackedRendererListHashSet::iterator it = positionedObjects->begin(); it != end; ++it) {
            LayoutBox* box = *it;

            // One of the renderers we're skipping over here may be the child's paint invalidation container,
            // so we can't pass our own paint invalidation container along.
            const LayoutBoxModelObject& paintInvalidationContainerForChild = *box->containerForPaintInvalidation();

            // If it's a new paint invalidation container, we won't have properly accumulated the offset into the
            // PaintInvalidationState.
            // FIXME: Teach PaintInvalidationState to handle this case. crbug.com/371485
            if (paintInvalidationContainerForChild != childPaintInvalidationState.paintInvalidationContainer()) {
                ForceHorriblySlowRectMapping slowRectMapping(&childPaintInvalidationState);
                PaintInvalidationState disabledPaintInvalidationState(childPaintInvalidationState, *this, paintInvalidationContainerForChild);
                box->invalidateTreeIfNeeded(disabledPaintInvalidationState);
                continue;
            }

            // If the positioned renderer is absolutely positioned and it is inside
            // a relatively positioned inline element, we need to account for
            // the inline elements position in PaintInvalidationState.
            if (box->style()->position() == AbsolutePosition) {
                LayoutObject* container = box->container(&paintInvalidationContainerForChild, 0);
                if (container->isRelPositioned() && container->isLayoutInline()) {
                    // FIXME: We should be able to use PaintInvalidationState for this.
                    // Currently, we will place absolutely positioned elements inside
                    // relatively positioned inline blocks in the wrong location. crbug.com/371485
                    ForceHorriblySlowRectMapping slowRectMapping(&childPaintInvalidationState);
                    PaintInvalidationState disabledPaintInvalidationState(childPaintInvalidationState, *this, paintInvalidationContainerForChild);
                    box->invalidateTreeIfNeeded(disabledPaintInvalidationState);
                    continue;
                }
            }

            box->invalidateTreeIfNeeded(childPaintInvalidationState);
        }
    }
}

LayoutBlock* LayoutBlock::continuationBefore(LayoutObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;

    LayoutBlock* curr = toLayoutBlock(continuation());
    LayoutBlock* nextToLast = this;
    LayoutBlock* last = this;
    while (curr) {
        if (beforeChild && beforeChild->parent() == curr) {
            if (curr->firstChild() == beforeChild)
                return last;
            return curr;
        }

        nextToLast = last;
        last = curr;
        curr = toLayoutBlock(curr->continuation());
    }

    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void LayoutBlock::addChildToContinuation(LayoutObject* newChild, LayoutObject* beforeChild)
{
    LayoutBlock* flow = continuationBefore(beforeChild);
    LayoutBoxModelObject* beforeChildParent = 0;
    if (beforeChild) {
        beforeChildParent = toLayoutBoxModelObject(beforeChild->parent());
        // Don't attempt to insert into something that isn't a LayoutBlockFlow (block
        // container). While the DOM nodes of |beforeChild| and |newChild| are siblings, there may
        // be anonymous table wrapper objects around |beforeChild| on the layout side. Therefore,
        // find the nearest LayoutBlockFlow. If it turns out that the new renderer doesn't belong
        // inside the anonymous table, this will make sure that it's really put on the outside. If
        // it turns out that it does belong inside it, the normal child insertion machinery will
        // make sure it ends up there, and at the right place too. We cannot just guess that it's
        // going to be right under the parent of |beforeChild|.
        while (beforeChildParent && !beforeChildParent->isLayoutBlockFlow()) {
            ASSERT(!beforeChildParent->virtualContinuation());
            ASSERT(beforeChildParent->isAnonymous());
            RELEASE_ASSERT(beforeChildParent != this);
            beforeChildParent = toLayoutBoxModelObject(beforeChildParent->parent());
        }
        ASSERT(beforeChildParent);
    } else {
        LayoutBoxModelObject* cont = flow->continuation();
        if (cont)
            beforeChildParent = cont;
        else
            beforeChildParent = flow;
    }

    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }

    // A continuation always consists of two potential candidates: a block or an anonymous
    // column span box holding column span children.
    bool childIsNormal = newChild->isInline() || !newChild->style()->columnSpan();
    bool bcpIsNormal = beforeChildParent->isInline() || !beforeChildParent->style()->columnSpan();
    bool flowIsNormal = flow->isInline() || !flow->style()->columnSpan();

    if (flow == beforeChildParent) {
        flow->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }

    // The goal here is to match up if we can, so that we can coalesce and create the
    // minimal # of continuations needed for the inline.
    if (childIsNormal == bcpIsNormal) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }
    if (flowIsNormal == childIsNormal) {
        flow->addChildIgnoringContinuation(newChild, 0); // Just treat like an append.
        return;
    }
    beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
}


void LayoutBlock::addChildToAnonymousColumnBlocks(LayoutObject* newChild, LayoutObject* beforeChild)
{
    ASSERT(!continuation()); // We don't yet support column spans that aren't immediate children of the multi-column block.

    // The goal is to locate a suitable box in which to place our child.
    LayoutBlock* beforeChildParent = 0;
    if (beforeChild) {
        LayoutObject* curr = beforeChild;
        while (curr && curr->parent() != this)
            curr = curr->parent();
        beforeChildParent = toLayoutBlock(curr);
        ASSERT(beforeChildParent);
        ASSERT(beforeChildParent->isAnonymousColumnsBlock() || beforeChildParent->isAnonymousColumnSpanBlock());
    } else {
        beforeChildParent = toLayoutBlock(lastChild());
    }

    // If the new child is floating or positioned it can just go in that block.
    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
        return;
    }

    // See if the child can be placed in the box.
    bool newChildHasColumnSpan = newChild->style()->columnSpan() && !newChild->isInline();
    bool beforeChildParentHoldsColumnSpans = beforeChildParent->isAnonymousColumnSpanBlock();

    if (newChildHasColumnSpan == beforeChildParentHoldsColumnSpans) {
        beforeChildParent->addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
        return;
    }

    if (!beforeChild) {
        // Create a new block of the correct type.
        LayoutBlock* newBox = newChildHasColumnSpan ? createAnonymousColumnSpanBlock() : createAnonymousColumnsBlock();
        children()->appendChildNode(this, newBox);
        newBox->addChildIgnoringAnonymousColumnBlocks(newChild, 0);
        return;
    }

    LayoutObject* immediateChild = beforeChild;
    bool isPreviousBlockViable = true;
    while (immediateChild->parent() != this) {
        if (isPreviousBlockViable)
            isPreviousBlockViable = !immediateChild->previousSibling();
        immediateChild = immediateChild->parent();
    }
    if (isPreviousBlockViable && immediateChild->previousSibling()) {
        toLayoutBlock(immediateChild->previousSibling())->addChildIgnoringAnonymousColumnBlocks(newChild, 0); // Treat like an append.
        return;
    }

    // Split our anonymous blocks.
    LayoutObject* newBeforeChild = splitAnonymousBoxesAroundChild(beforeChild);


    // Create a new anonymous box of the appropriate type.
    LayoutBlock* newBox = newChildHasColumnSpan ? createAnonymousColumnSpanBlock() : createAnonymousColumnsBlock();
    children()->insertChildNode(this, newBox, newBeforeChild);
    newBox->addChildIgnoringAnonymousColumnBlocks(newChild, 0);
    return;
}

LayoutBlockFlow* LayoutBlock::containingColumnsBlock(bool allowAnonymousColumnBlock)
{
    LayoutBlock* firstChildIgnoringAnonymousWrappers = 0;
    for (LayoutObject* curr = this; curr; curr = curr->parent()) {
        if (!curr->isLayoutBlock() || curr->isFloatingOrOutOfFlowPositioned() || curr->isTableCell() || curr->isDocumentElement() || curr->isLayoutView() || curr->hasOverflowClip()
            || curr->isInlineBlockOrInlineTable())
            return 0;

        // FIXME: Renderers that do special management of their children (tables, buttons,
        // lists, flexboxes, etc.) breaks when the flow is split through them. Disabling
        // multi-column for them to avoid this problem.)
        if (!curr->isLayoutBlockFlow() || curr->isListItem())
            return 0;

        LayoutBlockFlow* currBlock = toLayoutBlockFlow(curr);
        if (!currBlock->createsAnonymousWrapper())
            firstChildIgnoringAnonymousWrappers = currBlock;

        if (currBlock->style()->specifiesColumns() && (allowAnonymousColumnBlock || !currBlock->isAnonymousColumnsBlock()))
            return toLayoutBlockFlow(firstChildIgnoringAnonymousWrappers);

        if (currBlock->isAnonymousColumnSpanBlock())
            return 0;
    }
    return 0;
}

LayoutBlock* LayoutBlock::clone() const
{
    LayoutBlock* cloneBlock;
    if (isAnonymousBlock()) {
        cloneBlock = createAnonymousBlock();
        cloneBlock->setChildrenInline(childrenInline());
    } else {
        LayoutObject* cloneRenderer = toElement(node())->createLayoutObject(styleRef());
        cloneBlock = toLayoutBlock(cloneRenderer);
        cloneBlock->setStyle(style());

        // This takes care of setting the right value of childrenInline in case
        // generated content is added to cloneBlock and 'this' does not have
        // generated content added yet.
        cloneBlock->setChildrenInline(cloneBlock->firstChild() ? cloneBlock->firstChild()->isInline() : childrenInline());
    }
    cloneBlock->setFlowThreadState(flowThreadState());
    return cloneBlock;
}

void LayoutBlock::splitBlocks(LayoutBlock* fromBlock, LayoutBlock* toBlock,
    LayoutBlock* middleBlock,
    LayoutObject* beforeChild, LayoutBoxModelObject* oldCont)
{
    ASSERT(isDescendantOf(fromBlock));

    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    Vector<LayoutBlock*> blocksToClone;
    for (LayoutObject* o = this; o != fromBlock; o = o->parent())
        blocksToClone.append(toLayoutBlock(o));

    // Create a new clone of the top-most block.
    LayoutBlock* topMostBlockToClone = blocksToClone.last();
    LayoutBlock* cloneBlock = topMostBlockToClone->clone();

    // Put |cloneBlock| as a child of |toBlock|.
    toBlock->children()->appendChildNode(toBlock, cloneBlock);

    // Now take all the children after |topMostBlockToClone| and remove them from the |fromBlock|
    // and put them in the |toBlock|.
    fromBlock->moveChildrenTo(toBlock, topMostBlockToClone->nextSibling(), nullptr, true);

    LayoutBlock* currentBlockParent = topMostBlockToClone;
    LayoutBlock* cloneBlockParent = cloneBlock;

    // Clone the blocks from top to down to ensure any new object will be added into a rooted tree.
    // Note that we have already cloned the top-most one, so the loop begins from size - 2.
    for (int i = static_cast<int>(blocksToClone.size()) - 2; i >= 0; --i) {
        // Hook the clone up as a continuation of |currentBlockParent|. Note we do encounter
        // anonymous blocks possibly as we walk down the block chain.  When we split an
        // anonymous block, there's no need to do any continuation hookup, since we haven't
        // actually split a real element.
        if (!currentBlockParent->isAnonymousBlock()) {
            LayoutBoxModelObject* oldCont = currentBlockParent->continuation();
            currentBlockParent->setContinuation(cloneBlock);
            cloneBlock->setContinuation(oldCont);
        }

        // Create a new clone.
        LayoutBlock* currentBlock = blocksToClone[i];
        cloneBlock = currentBlock->clone();

        // Insert the |cloneBlock| as the first child of |cloneBlockParent|.
        cloneBlockParent->addChildIgnoringContinuation(cloneBlock, nullptr);

        // Take all the children after |currentBlock| and remove them from the |currentBlockParent|
        // and put them to the end of the |cloneParent|.
        currentBlockParent->moveChildrenTo(cloneBlockParent, currentBlock->nextSibling(), nullptr, true);

        cloneBlockParent = cloneBlock;
        currentBlockParent = currentBlock;
    }

    // The last block to clone is |this|, and the current |cloneBlock| is cloned from |this|.
    ASSERT(this == blocksToClone.first());

    // Hook |cloneBlock| up as the continuation of the |middleBlock|.
    if (!isAnonymousBlock()) {
        cloneBlock->setContinuation(oldCont);
        middleBlock->setContinuation(cloneBlock);
    }

    // Now take all of the children from |beforeChild| to the end and remove
    // them from |this| and place them in the |cloneBlock|.
    moveChildrenTo(cloneBlock, beforeChild, nullptr, true);
}

void LayoutBlock::splitFlow(LayoutObject* beforeChild, LayoutBlock* newBlockBox,
    LayoutObject* newChild, LayoutBoxModelObject* oldCont)
{
    LayoutBlock* pre = 0;
    LayoutBlock* block = containingColumnsBlock();

    // Delete our line boxes before we do the inline split into continuations.
    block->deleteLineBoxTree();

    bool madeNewBeforeBlock = false;
    if (block->isAnonymousColumnsBlock()) {
        // We can reuse this block and make it the preBlock of the next continuation.
        pre = block;
        pre->removePositionedObjects(0);
        if (block->isLayoutBlockFlow())
            toLayoutBlockFlow(pre)->removeFloatingObjects();
        block = toLayoutBlock(block->parent());
    } else {
        // No anonymous block available for use.  Make one.
        pre = block->createAnonymousColumnsBlock();
        pre->setChildrenInline(false);
        madeNewBeforeBlock = true;
    }

    LayoutBlock* post = block->createAnonymousColumnsBlock();
    post->setChildrenInline(false);

    LayoutObject* boxFirst = madeNewBeforeBlock ? block->firstChild() : pre->nextSibling();
    if (madeNewBeforeBlock)
        block->children()->insertChildNode(block, pre, boxFirst);
    block->children()->insertChildNode(block, newBlockBox, boxFirst);
    block->children()->insertChildNode(block, post, boxFirst);
    block->setChildrenInline(false);

    if (madeNewBeforeBlock)
        block->moveChildrenTo(pre, boxFirst, 0, true);

    splitBlocks(pre, post, newBlockBox, beforeChild, oldCont);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    newBlockBox->addChild(newChild);

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moves from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    pre->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    block->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    post->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
}

void LayoutBlock::makeChildrenAnonymousColumnBlocks(LayoutObject* beforeChild, LayoutBlockFlow* newBlockBox, LayoutObject* newChild)
{
    LayoutBlockFlow* pre = 0;
    LayoutBlockFlow* post = 0;
    LayoutBlock* block = this; // Eventually block will not just be |this|, but will also be a block nested inside |this|.  Assign to a variable
        // so that we don't have to patch all of the rest of the code later on.

    // Delete the block's line boxes before we do the split.
    block->deleteLineBoxTree();

    if (beforeChild && beforeChild->parent() != this)
        beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    if (beforeChild != firstChild()) {
        pre = block->createAnonymousColumnsBlock();
        pre->setChildrenInline(block->childrenInline());
    }

    if (beforeChild) {
        post = block->createAnonymousColumnsBlock();
        post->setChildrenInline(block->childrenInline());
    }

    LayoutObject* boxFirst = block->firstChild();
    if (pre)
        block->children()->insertChildNode(block, pre, boxFirst);
    block->children()->insertChildNode(block, newBlockBox, boxFirst);
    if (post)
        block->children()->insertChildNode(block, post, boxFirst);
    block->setChildrenInline(false);

    // The pre/post blocks always have layers, so we know to always do a full insert/remove (so we pass true as the last argument).
    block->moveChildrenTo(pre, boxFirst, beforeChild, true);
    block->moveChildrenTo(post, beforeChild, 0, true);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    newBlockBox->addChild(newChild);

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moved from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    if (pre)
        pre->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    block->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    if (post)
        post->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
}

LayoutBlockFlow* LayoutBlock::columnsBlockForSpanningElement(LayoutObject* newChild)
{
    // FIXME: This function is the gateway for the addition of column-span support.  It will
    // be added to in three stages:
    // (1) Immediate children of a multi-column block can span.
    // (2) Nested block-level children with only block-level ancestors between them and the multi-column block can span.
    // (3) Nested children with block or inline ancestors between them and the multi-column block can span (this is when we
    // cross the streams and have to cope with both types of continuations mixed together).
    // This function currently supports (1) and (2).
    LayoutBlockFlow* columnsBlockAncestor = 0;
    if (!newChild->isText() && newChild->style()->columnSpan() && !newChild->isBeforeOrAfterContent()
        && !newChild->isFloatingOrOutOfFlowPositioned() && !newChild->isInline() && !newChild->isTablePart()
        && !isAnonymousColumnSpanBlock()) {
        columnsBlockAncestor = containingColumnsBlock(false);
        if (columnsBlockAncestor) {
            // Make sure that none of the parent ancestors have a continuation.
            // If yes, we do not want split the block into continuations.
            LayoutObject* curr = this;
            while (curr && curr != columnsBlockAncestor) {
                if (curr->isLayoutBlock() && toLayoutBlock(curr)->continuation()) {
                    columnsBlockAncestor = 0;
                    break;
                }
                curr = curr->parent();
            }
        }
    }
    return columnsBlockAncestor;
}

void LayoutBlock::addChildIgnoringAnonymousColumnBlocks(LayoutObject* newChild, LayoutObject* beforeChild)
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
                // Full screen renderers and full screen placeholders act as anonymous blocks, not tables:
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

    // Check for a spanning element in columns.
    if (gColumnFlowSplitEnabled && !document().regionBasedColumnsEnabled()) {
        LayoutBlockFlow* columnsBlockAncestor = columnsBlockForSpanningElement(newChild);
        if (columnsBlockAncestor) {
            TemporaryChange<bool> columnFlowSplitEnabled(gColumnFlowSplitEnabled, false);
            // We are placing a column-span element inside a block.
            LayoutBlockFlow* newBox = createAnonymousColumnSpanBlock();

            if (columnsBlockAncestor != this && !isLayoutFlowThread()) {
                // We are nested inside a multi-column element and are being split by the span. We have to break up
                // our block into continuations.
                LayoutBoxModelObject* oldContinuation = continuation();

                // When we split an anonymous block, there's no need to do any continuation hookup,
                // since we haven't actually split a real element.
                if (!isAnonymousBlock())
                    setContinuation(newBox);

                splitFlow(beforeChild, newBox, newChild, oldContinuation);
                return;
            }

            // We have to perform a split of this block's children. This involves creating an anonymous block box to hold
            // the column-spanning |newChild|. We take all of the children from before |newChild| and put them into
            // one anonymous columns block, and all of the children after |newChild| go into another anonymous block.
            makeChildrenAnonymousColumnBlocks(beforeChild, newBox, newChild);
            return;
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
            newBox->addChild(newChild);
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
    if (continuation() && !isAnonymousBlock())
        addChildToContinuation(newChild, beforeChild);
    else
        addChildIgnoringContinuation(newChild, beforeChild);
}

void LayoutBlock::addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild)
{
    if (!isAnonymousBlock() && firstChild() && (firstChild()->isAnonymousColumnsBlock() || firstChild()->isAnonymousColumnSpanBlock()))
        addChildToAnonymousColumnBlocks(newChild, beforeChild);
    else
        addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
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

void LayoutBlock::promoteAllChildrenAndInsertAfter()
{
    LayoutObject* firstPromotee = firstChild();
    if (!firstPromotee)
        return;
    LayoutObject* lastPromotee = lastChild();
    LayoutBlock* parent = toLayoutBlock(this->parent());
    LayoutObject* nextSiblingOfPromotees = nextSibling();
    for (LayoutObject* o = firstPromotee; o; o = o->nextSibling())
        o->setParent(parent);
    children()->setFirstChild(nullptr);
    children()->setLastChild(nullptr);
    firstPromotee->setPreviousSibling(this);
    setNextSibling(firstPromotee);
    lastPromotee->setNextSibling(nextSiblingOfPromotees);
    if (nextSiblingOfPromotees)
        nextSiblingOfPromotees->setPreviousSibling(lastPromotee);
    if (parent->children()->lastChild() == this)
        parent->children()->setLastChild(lastPromotee);
}

void LayoutBlock::removeLeftoverAnonymousBlock(LayoutBlock* child)
{
    ASSERT(child->isAnonymousBlock());
    ASSERT(!child->childrenInline());
    ASSERT(child->parent() == this);

    if (child->continuation() || (child->firstChild() && (child->isAnonymousColumnSpanBlock() || child->isAnonymousColumnsBlock())))
        return;

    // Promote all the leftover anonymous block's children (to become children of this block
    // instead). We still want to keep the leftover block in the tree for a moment, for notification
    // purposes done further below (flow threads and grids).
    child->promoteAllChildrenAndInsertAfter();

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

static bool canMergeContiguousAnonymousBlocks(LayoutObject* oldChild, LayoutObject* prev, LayoutObject* next)
{
    if (oldChild->documentBeingDestroyed() || oldChild->isInline() || oldChild->virtualContinuation())
        return false;

    if ((prev && (!prev->isAnonymousBlock() || toLayoutBlock(prev)->continuation() || toLayoutBlock(prev)->beingDestroyed()))
        || (next && (!next->isAnonymousBlock() || toLayoutBlock(next)->continuation() || toLayoutBlock(next)->beingDestroyed())))
        return false;

    if ((prev && (prev->isRubyRun() || prev->isRubyBase()))
        || (next && (next->isRubyRun() || next->isRubyBase())))
        return false;

    if (!prev || !next)
        return true;

    // Make sure the types of the anonymous blocks match up.
    return prev->isAnonymousColumnsBlock() == next->isAnonymousColumnsBlock()
        && prev->isAnonymousColumnSpanBlock() == next->isAnonymousColumnSpanBlock();
}

void LayoutBlock::removeAnonymousWrappersIfRequired()
{
    ASSERT(isLayoutBlockFlow());
    Vector<LayoutBox*, 16> blocksToRemove;
    for (LayoutBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        // There are still block children in the container, so any anonymous wrappers are still needed.
        if (!child->isAnonymousBlock() && !child->isFloatingOrOutOfFlowPositioned())
            return;
        // We can't remove anonymous wrappers if they contain continuations as this means there are block children present.
        if (child->isLayoutBlock() && toLayoutBlock(child)->continuation())
            return;
        // We are only interested in removing anonymous wrappers if there are inline siblings underneath them.
        if (!child->childrenInline())
            return;
        if (child->isAnonymousBlock())
            blocksToRemove.append(child);
    }

    for (size_t i = 0; i < blocksToRemove.size(); i++)
        collapseAnonymousBlockChild(this, toLayoutBlock(blocksToRemove[i]));
}

void LayoutBlock::collapseAnonymousBlockChild(LayoutBlock* parent, LayoutBlock* child)
{
    // It's possible that this block's destruction may have been triggered by the
    // child's removal. Just bail if the anonymous child block is already being
    // destroyed. See crbug.com/282088
    if (child->beingDestroyed())
        return;
    parent->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    parent->setChildrenInline(child->childrenInline());
    LayoutObject* nextSibling = child->nextSibling();

    parent->children()->removeChildNode(parent, child, child->hasLayer());
    // FIXME: Get rid of the temporary disabling of continuations. This is needed by the old
    // multicol implementation, because of buggy block continuation handling (which is hard and
    // rather pointless to fix at this point). Support for block continuations can be removed
    // together with the old multicol implementation. crbug.com/408123
    LayoutBoxModelObject* temporarilyInactiveContinuation = parent->continuation();
    if (temporarilyInactiveContinuation)
        parent->setContinuation(0);
    child->moveAllChildrenTo(parent, nextSibling, child->hasLayer());
    if (temporarilyInactiveContinuation)
        parent->setContinuation(temporarilyInactiveContinuation);
    // Explicitly delete the child's line box tree, or the special anonymous
    // block handling in willBeDestroyed will cause problems.
    child->deleteLineBoxTree();
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

    // This protects against column split flows when anonymous blocks are getting merged.
    TemporaryChange<bool> columnFlowSplitEnabled(gColumnFlowSplitEnabled, false);

    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    LayoutObject* prev = oldChild->previousSibling();
    LayoutObject* next = oldChild->nextSibling();
    bool canMergeAnonymousBlocks = canMergeContiguousAnonymousBlocks(oldChild, prev, next);
    if (canMergeAnonymousBlocks && prev && next) {
        prev->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        LayoutBlockFlow* nextBlock = toLayoutBlockFlow(next);
        LayoutBlockFlow* prevBlock = toLayoutBlockFlow(prev);

        if (prev->childrenInline() != next->childrenInline()) {
            LayoutBlock* inlineChildrenBlock = prev->childrenInline() ? prevBlock : nextBlock;
            LayoutBlock* blockChildrenBlock = prev->childrenInline() ? nextBlock : prevBlock;

            // Place the inline children block inside of the block children block instead of deleting it.
            // In order to reuse it, we have to reset it to just be a generic anonymous block.  Make sure
            // to clear out inherited column properties by just making a new style, and to also clear the
            // column span flag if it is set.
            ASSERT(!inlineChildrenBlock->continuation());
            RefPtr<LayoutStyle> newStyle = LayoutStyle::createAnonymousStyleWithDisplay(styleRef(), BLOCK);
            // Cache this value as it might get changed in setStyle() call.
            bool inlineChildrenBlockHasLayer = inlineChildrenBlock->hasLayer();
            inlineChildrenBlock->setStyle(newStyle);
            children()->removeChildNode(this, inlineChildrenBlock, inlineChildrenBlockHasLayer);

            // Now just put the inlineChildrenBlock inside the blockChildrenBlock.
            blockChildrenBlock->children()->insertChildNode(blockChildrenBlock, inlineChildrenBlock, prev == inlineChildrenBlock ? blockChildrenBlock->firstChild() : 0,
                inlineChildrenBlockHasLayer || blockChildrenBlock->hasLayer());
            next->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();

            // inlineChildrenBlock got reparented to blockChildrenBlock, so it is no longer a child
            // of "this". we null out prev or next so that is not used later in the function.
            if (inlineChildrenBlock == prevBlock)
                prev = 0;
            else
                next = 0;
        } else {
            // Take all the children out of the |next| block and put them in
            // the |prev| block.
            nextBlock->moveAllChildrenIncludingFloatsTo(prevBlock, nextBlock->hasLayer() || prevBlock->hasLayer());

            // Delete the now-empty block's lines and nuke it.
            nextBlock->deleteLineBoxTree();
            nextBlock->destroy();
            next = 0;
        }
    }

    LayoutBox::removeChild(oldChild);

    LayoutObject* child = prev ? prev : next;
    if (canMergeAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling() && canCollapseAnonymousBlockChild()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        collapseAnonymousBlockChild(this, toLayoutBlock(child));
    } else if (((prev && prev->isAnonymousBlock()) || (next && next->isAnonymousBlock())) && canCollapseAnonymousBlockChild()) {
        // It's possible that the removal has knocked us down to a single anonymous
        // block with pseudo-style element siblings (e.g. first-letter). If these
        // are floating, then we need to pull the content up also.
        LayoutBlock* anonymousBlock = toLayoutBlock((prev && prev->isAnonymousBlock()) ? prev : next);
        if ((anonymousBlock->previousSibling() || anonymousBlock->nextSibling())
            && (!anonymousBlock->previousSibling() || (anonymousBlock->previousSibling()->style()->styleType() != NOPSEUDO && anonymousBlock->previousSibling()->isFloating() && !anonymousBlock->previousSibling()->previousSibling()))
            && (!anonymousBlock->nextSibling() || (anonymousBlock->nextSibling()->style()->styleType() != NOPSEUDO && anonymousBlock->nextSibling()->isFloating() && !anonymousBlock->nextSibling()->nextSibling()))) {
            collapseAnonymousBlockChild(this, anonymousBlock);
        }
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
            setContinuation(0);
            destroy();
        }
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
        || style()->marginBeforeCollapse() == MSEPARATE || style()->marginAfterCollapse() == MSEPARATE)
        return false;

    Length logicalHeightLength = style()->logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.isPercent() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (LayoutBlock* cb = containingBlock(); !cb->isLayoutView(); cb = cb->containingBlock()) {
            if (cb->style()->logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.isPercent()) && logicalHeightLength.isZero())) {
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

void LayoutBlock::finishDelayUpdateScrollInfo()
{
    --gDelayUpdateScrollInfo;
    ASSERT(gDelayUpdateScrollInfo >= 0);
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(gDelayedUpdateScrollInfoSet);

        OwnPtr<DelayedUpdateScrollInfoSet> infoSet(adoptPtr(gDelayedUpdateScrollInfoSet));
        gDelayedUpdateScrollInfoSet = 0;

        for (DelayedUpdateScrollInfoSet::iterator it = infoSet->begin(); it != infoSet->end(); ++it) {
            LayoutBlock* block = *it;
            if (block->hasOverflowClip()) {
                block->layer()->scrollableArea()->updateAfterLayout();
            }
        }
    }
}

void LayoutBlock::updateScrollInfoAfterLayout()
{
    if (hasOverflowClip()) {
        if (style()->isFlippedBlocksWritingMode()) {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=97937
            // Workaround for now. We cannot delay the scroll info for overflow
            // for items with opposite writing directions, as the contents needs
            // to overflow in that direction
            layer()->scrollableArea()->updateAfterLayout();
            return;
        }

        if (gDelayUpdateScrollInfo)
            gDelayedUpdateScrollInfoSet->add(this);
        else
            layer()->scrollableArea()->updateAfterLayout();
    }
}

void LayoutBlock::layout()
{
    OverflowEventDispatcher dispatcher(this);

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);

    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    if (hasControlClip() && m_overflow)
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();
}

bool LayoutBlock::updateImageLoadingPriorities()
{
    Vector<ImageResource*> images;
    appendImagesFromStyle(images, styleRef());

    if (images.isEmpty())
        return false;

    LayoutRect viewBounds = viewRect();
    LayoutRect objectBounds(absoluteContentBox());
    // The object bounds might be empty right now, so intersects will fail since it doesn't deal
    // with empty rects. Use LayoutRect::contains in that case.
    bool isVisible;
    if (!objectBounds.isEmpty())
        isVisible =  viewBounds.intersects(objectBounds);
    else
        isVisible = viewBounds.contains(objectBounds);

    ResourceLoadPriorityOptimizer::VisibilityStatus status = isVisible ?
        ResourceLoadPriorityOptimizer::Visible : ResourceLoadPriorityOptimizer::NotVisible;

    LayoutRect screenArea;
    if (!objectBounds.isEmpty()) {
        screenArea = viewBounds;
        screenArea.intersect(objectBounds);
    }

    for (Vector<ImageResource*>::iterator it = images.begin(), end = images.end(); it != end; ++it)
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->notifyImageResourceVisibility(*it, status, screenArea);

    return true;
}

bool LayoutBlock::widthAvailableToChildrenHasChanged()
{
    bool widthAvailableToChildrenHasChanged = m_widthAvailableToChildrenChanged;
    m_widthAvailableToChildrenChanged = false;

    // If we use border-box sizing, have percentage padding, and our parent has changed width then the width available to our children has changed even
    // though our own width has remained the same.
    widthAvailableToChildrenHasChanged |= style()->boxSizing() == BORDER_BOX && needsPreferredWidthsRecalculation() && view()->layoutState()->containingBlockLogicalWidthChanged();

    return widthAvailableToChildrenHasChanged;
}

bool LayoutBlock::updateLogicalWidthAndColumnWidth()
{
    LayoutUnit oldWidth = logicalWidth();
    LayoutUnit oldColumnWidth = desiredColumnWidth();

    updateLogicalWidth();
    calcColumnWidth();

    return oldWidth != logicalWidth() || oldColumnWidth != desiredColumnWidth() || widthAvailableToChildrenHasChanged();
}

void LayoutBlock::layoutBlock(bool)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void LayoutBlock::addOverflowFromChildren()
{
    if (!hasColumns()) {
        if (childrenInline())
            toLayoutBlockFlow(this)->addOverflowFromInlineChildren();
        else
            addOverflowFromBlockChildren();
    } else {
        ColumnInfo* colInfo = columnInfo();
        if (columnCount(colInfo)) {
            LayoutRect lastRect = columnRectAt(colInfo, columnCount(colInfo) - 1);
            addLayoutOverflow(lastRect);
            addContentsVisualOverflow(lastRect);
        }
    }
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
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), 1, std::max<LayoutUnit>(0, oldClientAfterEdge - clientRect.y()));
        else
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), std::max<LayoutUnit>(0, oldClientAfterEdge - clientRect.x()), 1);
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
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    LayoutBox* positionedObject;
    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        positionedObject = *it;

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
    LayoutTheme::theme().adjustPaintInvalidationRect(this, inflatedRect);
    addVisualOverflow(LayoutRect(inflatedRect));
}

bool LayoutBlock::createsNewFormattingContext() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrOutOfFlowPositioned() || hasOverflowClip() || isFlexItemIncludingDeprecated()
        || style()->specifiesColumns() || isLayoutFlowThread() || isTableCell() || isTableCaption() || isFieldset() || isWritingModeRoot()
        || isDocumentElement() || (document().regionBasedColumnsEnabled() ? isColumnSpanAll() : style()->columnSpan()) || isGridItem();
}

void LayoutBlock::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, LayoutBox& child)
{
    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    bool hasRelativeLogicalHeight = child.hasRelativeLogicalHeight() || (child.isAnonymous() && this->hasRelativeLogicalHeight());
    if (relayoutChildren || (hasRelativeLogicalHeight && !isLayoutView()))
        child.setChildNeedsLayout(MarkOnlyThis);

    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren && child.needsPreferredWidthsRecalculation())
        child.setPreferredLogicalWidthsDirty(MarkOnlyThis);
}

void LayoutBlock::simplifiedNormalFlowLayout()
{
    if (childrenInline()) {
        ListHashSet<RootInlineBox*> lineBoxes;
        for (InlineWalker walker(this); !walker.atEnd(); walker.advance()) {
            LayoutObject* o = walker.current();
            if (!o->isOutOfFlowPositioned() && (o->isReplaced() || o->isFloating())) {
                o->layoutIfNeeded();
                if (toLayoutBox(o)->inlineBoxWrapper()) {
                    RootInlineBox& box = toLayoutBox(o)->inlineBoxWrapper()->root();
                    lineBoxes.add(&box);
                }
            } else if (o->isText() || (o->isLayoutInline() && !walker.atEndOfInline())) {
                o->clearNeedsLayout();
            }
        }

        // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
        GlyphOverflowAndFallbackFontsMap textBoxDataMap;
        for (ListHashSet<RootInlineBox*>::const_iterator it = lineBoxes.begin(); it != lineBoxes.end(); ++it) {
            RootInlineBox* box = *it;
            box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
        }
    } else {
        for (LayoutBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (!box->isOutOfFlowPositioned())
                box->layoutIfNeeded();
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

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
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
    LayoutUnit margin = 0;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

void LayoutBlock::layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior info)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    if (hasColumns())
        view()->layoutState()->clearPaginationInformation(); // Positioned objects are not part of the column flow, so they don't paginate with the columns.

    LayoutBox* r;
    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        r = *it;

        r->setMayNeedPaintInvalidation();

        SubtreeLayoutScope layoutScope(*r);
        // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
        // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e.
        // it has static position.
        markFixedPositionObjectForLayoutIfNeeded(r, layoutScope);
        if (info == LayoutOnlyFixedPositionedObjects) {
            r->layoutIfNeeded();
            continue;
        }

        // When a non-positioned block element moves, it may have positioned children that are implicitly positioned relative to the
        // non-positioned block.  Rather than trying to detect all of these movement cases, we just always lay out positioned
        // objects that are positioned implicitly like this.  Such objects are rare, and so in typical DHTML menu usage (where everything is
        // positioned explicitly) this should not incur a performance penalty.
        if (relayoutChildren || (r->style()->hasStaticBlockPosition(isHorizontalWritingMode()) && r->parent() != this))
            layoutScope.setChildNeedsLayout(r);

        // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
        if (relayoutChildren && r->needsPreferredWidthsRecalculation())
            r->setPreferredLogicalWidthsDirty(MarkOnlyThis);

        if (!r->needsLayout())
            r->markForPaginationRelayoutIfNeeded(layoutScope);

        // If we are paginated or in a line grid, go ahead and compute a vertical position for our object now.
        // If it's wrong we'll lay out again.
        LayoutUnit oldLogicalTop = 0;
        bool needsBlockDirectionLocationSetBeforeLayout = r->needsLayout() && view()->layoutState()->needsBlockDirectionLocationSetBeforeLayout();
        if (needsBlockDirectionLocationSetBeforeLayout) {
            if (isHorizontalWritingMode() == r->isHorizontalWritingMode())
                r->updateLogicalHeight();
            else
                r->updateLogicalWidth();
            oldLogicalTop = logicalTopForChild(*r);
        }

        // FIXME: We should be able to do a r->setNeedsPositionedMovementLayout() here instead of a full layout. Need
        // to investigate why it does not trigger the correct invalidations in that case. crbug.com/350756
        if (info == ForcedLayoutAfterContainingBlockMoved)
            r->setNeedsLayout(MarkOnlyThis);

        r->layoutIfNeeded();

        // Lay out again if our estimate was wrong.
        if (needsBlockDirectionLocationSetBeforeLayout && logicalTopForChild(*r) != oldLogicalTop)
            r->forceChildLayout();
    }

    if (hasColumns())
        view()->layoutState()->setColumnInfo(columnInfo()); // FIXME: Kind of gross. We just put this back into the layout state so that pop() will work.
}

void LayoutBlock::markPositionedObjectsForLayout()
{
    if (TrackedRendererListHashSet* positionedDescendants = positionedObjects()) {
        TrackedRendererListHashSet::iterator end = positionedDescendants->end();
        for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it)
            (*it)->setChildNeedsLayout();
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

void LayoutBlock::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    BlockPainter(*this).paint(paintInfo, paintOffset);
}

void LayoutBlock::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    BlockPainter(*this).paintChildren(paintInfo, paintOffset);
}

void LayoutBlock::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    BlockPainter(*this).paintObject(paintInfo, paintOffset);
}

LayoutInline* LayoutBlock::inlineElementContinuation() const
{
    LayoutBoxModelObject* continuation = this->continuation();
    return continuation && continuation->isInline() ? toLayoutInline(continuation) : 0;
}

LayoutBlock* LayoutBlock::blockElementContinuation() const
{
    LayoutBoxModelObject* currentContinuation = continuation();
    if (!currentContinuation || currentContinuation->isInline())
        return 0;
    LayoutBlock* nextContinuation = toLayoutBlock(currentContinuation);
    if (nextContinuation->isAnonymousBlock())
        return nextContinuation->blockElementContinuation();
    return nextContinuation;
}

ContinuationOutlineTableMap* continuationOutlineTable()
{
    DEFINE_STATIC_LOCAL(ContinuationOutlineTableMap, table, ());
    return &table;
}

void LayoutBlock::addContinuationWithOutline(LayoutInline* flow)
{
    // We can't make this work if the inline is in a layer.  We'll just rely on the broken
    // way of painting.
    ASSERT(!flow->layer() && !flow->isInlineElementContinuation());

    ContinuationOutlineTableMap* table = continuationOutlineTable();
    ListHashSet<LayoutInline*>* continuations = table->get(this);
    if (!continuations) {
        continuations = new ListHashSet<LayoutInline*>;
        table->set(this, adoptPtr(continuations));
    }

    continuations->add(flow);
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
        return 0;

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
        return 0;

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

void LayoutBlock::insertIntoTrackedRendererMaps(LayoutBox* descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap)
{
    if (!descendantsMap) {
        descendantsMap = new TrackedDescendantsMap;
        containerMap = new TrackedContainerMap;
    }

    TrackedRendererListHashSet* descendantSet = descendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedRendererListHashSet;
        descendantsMap->set(this, adoptPtr(descendantSet));
    }
    bool added = descendantSet->add(descendant).isNewEntry;
    if (!added) {
        ASSERT(containerMap->get(descendant));
        ASSERT(containerMap->get(descendant)->contains(this));
        return;
    }

    HashSet<LayoutBlock*>* containerSet = containerMap->get(descendant);
    if (!containerSet) {
        containerSet = new HashSet<LayoutBlock*>;
        containerMap->set(descendant, adoptPtr(containerSet));
    }
    ASSERT(!containerSet->contains(this));
    containerSet->add(this);
}

void LayoutBlock::removeFromTrackedRendererMaps(LayoutBox* descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap)
{
    if (!descendantsMap)
        return;

    OwnPtr<HashSet<LayoutBlock*>> containerSet = containerMap->take(descendant);
    if (!containerSet)
        return;

    HashSet<LayoutBlock*>::iterator end = containerSet->end();
    for (HashSet<LayoutBlock*>::iterator it = containerSet->begin(); it != end; ++it) {
        LayoutBlock* container = *it;

        // FIXME: Disabling this assert temporarily until we fix the layout
        // bugs associated with positioned objects not properly cleared from
        // their ancestor chain before being moved. See webkit bug 93766.
        // ASSERT(descendant->isDescendantOf(container));

        TrackedDescendantsMap::iterator descendantsMapIterator = descendantsMap->find(container);
        ASSERT(descendantsMapIterator != descendantsMap->end());
        if (descendantsMapIterator == descendantsMap->end())
            continue;
        TrackedRendererListHashSet* descendantSet = descendantsMapIterator->value.get();
        ASSERT(descendantSet->contains(descendant));
        descendantSet->remove(descendant);
        if (descendantSet->isEmpty())
            descendantsMap->remove(descendantsMapIterator);
    }
}

TrackedRendererListHashSet* LayoutBlock::positionedObjects() const
{
    if (gPositionedDescendantsMap)
        return gPositionedDescendantsMap->get(this);
    return 0;
}

void LayoutBlock::insertPositionedObject(LayoutBox* o)
{
    ASSERT(!isAnonymousBlock());
    insertIntoTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap);
}

void LayoutBlock::removePositionedObject(LayoutBox* o)
{
    removeFromTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap);
}

void LayoutBlock::removePositionedObjects(LayoutBlock* o, ContainingBlockState containingBlockState)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    LayoutBox* r;

    TrackedRendererListHashSet::iterator end = positionedDescendants->end();

    Vector<LayoutBox*, 16> deadObjects;

    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        r = *it;
        if (!o || r->isDescendantOf(o)) {
            if (containingBlockState == NewContainingBlock) {
                r->setChildNeedsLayout(MarkOnlyThis);
                if (r->needsPreferredWidthsRecalculation())
                    r->setPreferredLogicalWidthsDirty(MarkOnlyThis);
            }

            // It is parent blocks job to add positioned child to positioned objects list of its containing block
            // Parent layout needs to be invalidated to ensure this happens.
            LayoutObject* p = r->parent();
            while (p && !p->isLayoutBlock())
                p = p->parent();
            if (p)
                p->setChildNeedsLayout();

            deadObjects.append(r);
        }
    }

    for (unsigned i = 0; i < deadObjects.size(); i++)
        removePositionedObject(deadObjects.at(i));
}

void LayoutBlock::addPercentHeightDescendant(LayoutBox* descendant)
{
    insertIntoTrackedRendererMaps(descendant, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
}

void LayoutBlock::removePercentHeightDescendant(LayoutBox* descendant)
{
    removeFromTrackedRendererMaps(descendant, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
}

TrackedRendererListHashSet* LayoutBlock::percentHeightDescendants() const
{
    return gPercentHeightDescendantsMap ? gPercentHeightDescendantsMap->get(this) : 0;
}

bool LayoutBlock::hasPercentHeightContainerMap()
{
    return gPercentHeightContainerMap;
}

bool LayoutBlock::hasPercentHeightDescendant(LayoutBox* descendant)
{
    // We don't null check gPercentHeightContainerMap since the caller
    // already ensures this and we need to call this function on every
    // descendant in clearPercentHeightDescendantsFrom().
    ASSERT(gPercentHeightContainerMap);
    return gPercentHeightContainerMap->contains(descendant);
}

void LayoutBlock::dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope& layoutScope)
{
    if (!gPercentHeightDescendantsMap)
        return;

    TrackedRendererListHashSet* descendants = gPercentHeightDescendantsMap->get(this);
    if (!descendants)
        return;

    TrackedRendererListHashSet::iterator end = descendants->end();
    for (TrackedRendererListHashSet::iterator it = descendants->begin(); it != end; ++it) {
        LayoutBox* box = *it;
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

void LayoutBlock::removePercentHeightDescendantIfNeeded(LayoutBox* descendant)
{
    // We query the map directly, rather than looking at style's
    // logicalHeight()/logicalMinHeight()/logicalMaxHeight() since those
    // can change with writing mode/directional changes.
    if (!hasPercentHeightContainerMap())
        return;

    if (!hasPercentHeightDescendant(descendant))
        return;

    removePercentHeightDescendant(descendant);
}

void LayoutBlock::clearPercentHeightDescendantsFrom(LayoutBox* parent)
{
    ASSERT(gPercentHeightContainerMap);
    for (LayoutObject* curr = parent->slowFirstChild(); curr; curr = curr->nextInPreOrder(parent)) {
        if (!curr->isBox())
            continue;

        LayoutBox* box = toLayoutBox(curr);
        if (!hasPercentHeightDescendant(box))
            continue;

        removePercentHeightDescendant(box);
    }
}

LayoutUnit LayoutBlock::textIndentOffset() const
{
    LayoutUnit cw = 0;
    if (style()->textIndent().isPercent())
        cw = containingBlock()->availableLogicalWidth();
    return minimumValueForLength(style()->textIndent(), cw);
}

void LayoutBlock::markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest)
{
    if (logicalTop >= logicalBottom)
        return;

    RootInlineBox* lowestDirtyLine = lastRootBox();
    RootInlineBox* afterLowest = lowestDirtyLine;
    while (lowestDirtyLine && lowestDirtyLine->lineBottomWithLeading() >= logicalBottom && logicalBottom < LayoutUnit::max()) {
        afterLowest = lowestDirtyLine;
        lowestDirtyLine = lowestDirtyLine->prevRootBox();
    }

    while (afterLowest && afterLowest != highest && (afterLowest->lineBottomWithLeading() >= logicalTop || afterLowest->lineBottomWithLeading() < 0)) {
        afterLowest->markDirty();
        afterLowest = afterLowest->prevRootBox();
    }
}

bool LayoutBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!scrollsOverflow())
        return false;

    return layer()->scrollableArea()->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
}

Node* LayoutBlock::nodeForHitTest() const
{
    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    return isAnonymousBlockContinuation() ? continuation()->node() : node();
}

bool LayoutBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
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
        && visibleToHitTestRequest(request)
        && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (!result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer))
            return true;
    }

    if (style()->clipPath()) {
        switch (style()->clipPath()->type()) {
        case ClipPathOperation::SHAPE: {
            ShapeClipPathOperation* clipPath = toShapeClipPathOperation(style()->clipPath());
            // FIXME: handle marginBox etc.
            if (!clipPath->path(borderBoxRect()).contains(FloatPoint(locationInContainer.point() - localOffset), clipPath->windRule()))
                return false;
            break;
        }
        case ClipPathOperation::REFERENCE:
            // FIXME: handle REFERENCE
            break;
        }
    }

    // If we have clipping, then we can't have any spillout.
    bool useOverflowClip = hasOverflowClip() && !hasSelfPaintingLayer();
    bool useClip = (hasControlClip() || useOverflowClip);
    bool checkChildren = !useClip;
    if (!checkChildren) {
        if (hasControlClip()) {
            checkChildren = locationInContainer.intersects(controlClipRect(adjustedLocation));
        } else {
            LayoutRect clipRect = overflowClipRect(adjustedLocation, IncludeOverlayScrollbarSize);
            if (style()->hasBorderRadius())
                checkChildren = locationInContainer.intersects(style()->getRoundedBorderFor(clipRect));
            else
                checkChildren = locationInContainer.intersects(clipRect);
        }
    }
    if (checkChildren) {
        // Hit test descendants first.
        LayoutSize scrolledOffset(localOffset);
        if (hasOverflowClip())
            scrolledOffset -= scrolledContentOffset();

        // Hit test contents if we don't have columns.
        if (!hasColumns()) {
            if (hitTestContents(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
                updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
                return true;
            }
            if (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, toLayoutPoint(scrolledOffset)))
                return true;
        } else if (hitTestColumns(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            return true;
        }
    }

    // Check if the point is outside radii.
    if (style()->hasBorderRadius()) {
        LayoutRect borderRect = borderBoxRect();
        borderRect.moveBy(adjustedLocation);
        FloatRoundedRect border = style()->getRoundedBorderFor(borderRect);
        if (!locationInContainer.intersects(border))
            return false;
    }

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (visibleToHitTestRequest(request) && locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            if (!result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer, boundsRect))
                return true;
        }
    }

    return false;
}

class ColumnRectIterator {
    WTF_MAKE_NONCOPYABLE(ColumnRectIterator);
public:
    ColumnRectIterator(const LayoutBlock& block)
        : m_block(block)
        , m_colInfo(block.columnInfo())
        , m_direction(m_block.style()->isFlippedBlocksWritingMode() ? 1 : -1)
        , m_isHorizontal(block.isHorizontalWritingMode())
        , m_logicalLeft(block.logicalLeftOffsetForContent())
    {
        int colCount = m_colInfo->columnCount();
        m_colIndex = colCount - 1;
        m_currLogicalTopOffset = colCount * m_colInfo->columnHeight() * m_direction;
        update();
    }

    void advance()
    {
        ASSERT(hasMore());
        m_colIndex--;
        update();
    }

    LayoutRect columnRect() const { return m_colRect; }
    bool hasMore() const { return m_colIndex >= 0; }

    void adjust(LayoutSize& offset) const
    {
        LayoutUnit currLogicalLeftOffset = (m_isHorizontal ? m_colRect.x() : m_colRect.y()) - m_logicalLeft;
        offset += m_isHorizontal ? LayoutSize(currLogicalLeftOffset, m_currLogicalTopOffset) : LayoutSize(m_currLogicalTopOffset, currLogicalLeftOffset);
        if (m_colInfo->progressionAxis() == ColumnInfo::BlockAxis) {
            if (m_isHorizontal)
                offset.expand(0, m_colRect.y() - m_block.borderTop() - m_block.paddingTop());
            else
                offset.expand(m_colRect.x() - m_block.borderLeft() - m_block.paddingLeft(), 0);
        }
    }

private:
    void update()
    {
        if (m_colIndex < 0)
            return;

        m_colRect = m_block.columnRectAt(const_cast<ColumnInfo*>(m_colInfo), m_colIndex);
        m_block.flipForWritingMode(m_colRect);
        m_currLogicalTopOffset -= (m_isHorizontal ? m_colRect.height() : m_colRect.width()) * m_direction;
    }

    const LayoutBlock& m_block;
    const ColumnInfo* const m_colInfo;
    const int m_direction;
    const bool m_isHorizontal;
    const LayoutUnit m_logicalLeft;
    int m_colIndex;
    LayoutUnit m_currLogicalTopOffset;
    LayoutRect m_colRect;
};

bool LayoutBlock::hitTestColumns(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // We need to do multiple passes, breaking up our hit testing into strips.
    if (!hasColumns())
        return false;

    for (ColumnRectIterator it(*this); it.hasMore(); it.advance()) {
        LayoutRect hitRect = LayoutRect(locationInContainer.boundingBox());
        LayoutRect colRect = it.columnRect();
        colRect.moveBy(accumulatedOffset);
        if (locationInContainer.intersects(colRect)) {
            // The point is inside this column.
            // Adjust accumulatedOffset to change where we hit test.
            LayoutSize offset;
            it.adjust(offset);
            LayoutPoint finalLocation = accumulatedOffset + offset;
            if (!result.isRectBasedTest() || colRect.contains(hitRect))
                return hitTestContents(request, result, locationInContainer, finalLocation, hitTestAction) || (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, finalLocation));

            hitTestContents(request, result, locationInContainer, finalLocation, hitTestAction);
        }
    }

    return false;
}

void LayoutBlock::adjustForColumnRect(LayoutSize& offset, const LayoutPoint& locationInContainer) const
{
    for (ColumnRectIterator it(*this); it.hasMore(); it.advance()) {
        LayoutRect colRect = it.columnRect();
        if (colRect.contains(locationInContainer)) {
            it.adjust(offset);
            return;
        }
    }
}

bool LayoutBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (childrenInline() && !isTable()) {
        // We have to hit-test our line boxes.
        if (m_lineBoxes.hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction))
            return true;
    } else {
        // Hit test our children.
        HitTestAction childHitTest = hitTestAction;
        if (hitTestAction == HitTestChildBlockBackgrounds)
            childHitTest = HitTestChildBlockBackground;
        for (LayoutBox* child = lastChildBox(); child; child = child->previousSiblingBox()) {
            LayoutPoint childPoint = flipForWritingModeForChild(child, accumulatedOffset);
            if (!child->hasSelfPaintingLayer() && !child->isFloating() && !child->isColumnSpanAll() && child->nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest))
                return true;
        }
    }

    return false;
}

Position LayoutBlock::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->renderer().nonPseudoNode())
        return createLegacyEditingPosition(nonPseudoNode(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? box->renderer().caretMinOffset() : box->renderer().caretMaxOffset());

    InlineTextBox* textBox = toInlineTextBox(box);
    return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? textBox->start() : textBox->start() + textBox->len());
}

static inline bool isEditingBoundary(LayoutObject* ancestor, LayoutObject* child)
{
    ASSERT(!ancestor || ancestor->nonPseudoNode());
    ASSERT(child && child->nonPseudoNode());
    return !ancestor || !ancestor->parent() || (ancestor->hasLayer() && ancestor->parent()->isLayoutView())
        || ancestor->nonPseudoNode()->hasEditableStyle() == child->nonPseudoNode()->hasEditableStyle();
}

// FIXME: This function should go on LayoutObject as an instance method. Then
// all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
static PositionWithAffinity positionForPointRespectingEditingBoundaries(LayoutBlock* parent, LayoutBox* child, const LayoutPoint& pointInParentCoordinates)
{
    LayoutPoint childLocation = child->location();
    if (child->isRelPositioned())
        childLocation += child->offsetForInFlowPosition();

    // FIXME: This is wrong if the child's writing-mode is different from the parent's.
    LayoutPoint pointInChildCoordinates(toLayoutPoint(pointInParentCoordinates - childLocation));

    // If this is an anonymous renderer, we just recur normally
    Node* childNode = child->nonPseudoNode();
    if (!childNode)
        return child->positionForPoint(pointInChildCoordinates);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    LayoutObject* ancestor = parent;
    while (ancestor && !ancestor->nonPseudoNode())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child->positionForPoint(pointInChildCoordinates);

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = parent->logicalWidthForChild(*child) / 2;
    LayoutUnit logicalLeft = parent->isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createPositionWithAffinity(childNode->nodeIndex(), DOWNSTREAM);
    return ancestor->createPositionWithAffinity(childNode->nodeIndex() + 1, UPSTREAM);
}

PositionWithAffinity LayoutBlock::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents)
{
    ASSERT(childrenInline());

    if (!firstRootBox())
        return createPositionWithAffinity(0, DOWNSTREAM);

    bool linesAreFlipped = style()->isFlippedLinesWritingMode();
    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineBox* closestBox = 0;
    RootInlineBox* firstRootBoxWithChildren = 0;
    RootInlineBox* lastRootBoxWithChildren = 0;
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
                return PositionWithAffinity(positionForBox(box, true), DOWNSTREAM);
            }
        }

        // pass the box a top position that is inside it
        LayoutPoint point(pointInLogicalContents.x(), closestBox->root().blockDirectionPointInLine());
        if (!isHorizontalWritingMode())
            point = point.transposedPoint();
        if (closestBox->renderer().isReplaced())
            return positionForPointRespectingEditingBoundaries(this, &toLayoutBox(closestBox->renderer()), point);
        return closestBox->renderer().positionForPoint(point);
    }

    if (lastRootBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        ASSERT(moveCaretToBoundary);
        InlineBox* logicallyLastBox;
        if (lastRootBoxWithChildren->getLogicalEndBoxWithNode(logicallyLastBox))
            return PositionWithAffinity(positionForBox(logicallyLastBox, false), DOWNSTREAM);
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createPositionWithAffinity(0, DOWNSTREAM);
}

static inline bool isChildHitTestCandidate(LayoutBox* box)
{
    return box->size().height() && box->style()->visibility() == VISIBLE && !box->isFloatingOrOutOfFlowPositioned();
}

PositionWithAffinity LayoutBlock::positionForPoint(const LayoutPoint& point)
{
    if (isTable())
        return LayoutBox::positionForPoint(point);

    if (isReplaced()) {
        // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
        LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
        LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

        if (pointLogicalLeft < 0)
            return createPositionWithAffinity(caretMinOffset(), DOWNSTREAM);
        if (pointLogicalLeft >= logicalWidth())
            return createPositionWithAffinity(caretMaxOffset(), DOWNSTREAM);
        if (pointLogicalTop < 0)
            return createPositionWithAffinity(caretMinOffset(), DOWNSTREAM);
        if (pointLogicalTop >= logicalHeight())
            return createPositionWithAffinity(caretMaxOffset(), DOWNSTREAM);
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
            return positionForPointRespectingEditingBoundaries(this, lastCandidateBox, pointInContents);

        for (LayoutBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
            if (!isChildHitTestCandidate(childBox))
                continue;
            LayoutUnit childLogicalBottom = logicalTopForChild(*childBox) + logicalHeightForChild(*childBox);
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (isChildHitTestCandidate(childBox) && (pointInLogicalContents.y() < childLogicalBottom
                || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom)))
                return positionForPointRespectingEditingBoundaries(this, childBox, pointInContents);
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

    if (hasColumns())
        adjustPointToColumnContents(offset);

    offset = flipForWritingMode(offset);
}

LayoutUnit LayoutBlock::availableLogicalWidth() const
{
    // If we have multiple columns, then the available logical width is reduced to our column width.
    if (hasColumns())
        return desiredColumnWidth();
    return LayoutBox::availableLogicalWidth();
}

int LayoutBlock::columnGap() const
{
    if (style()->hasNormalColumnGap())
        return style()->fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style()->columnGap());
}

void LayoutBlock::calcColumnWidth()
{
    if (document().regionBasedColumnsEnabled())
        return;

    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned desiredColumnCount = 1;
    LayoutUnit desiredColumnWidth = contentLogicalWidth();

    // For now, we don't support multi-column layouts when printing, since we have to do a lot of work for proper pagination.
    if (document().paginated() || !style()->specifiesColumns()) {
        setDesiredColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
        return;
    }

    LayoutUnit availWidth = desiredColumnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = std::max<LayoutUnit>(1, LayoutUnit(style()->columnWidth()));
    int colCount = std::max<int>(1, style()->columnCount());

    if (style()->hasAutoColumnWidth() && !style()->hasAutoColumnCount()) {
        desiredColumnCount = colCount;
        desiredColumnWidth = std::max<LayoutUnit>(0, (availWidth - ((desiredColumnCount - 1) * colGap)) / desiredColumnCount);
    } else if (!style()->hasAutoColumnWidth() && style()->hasAutoColumnCount()) {
        desiredColumnCount = std::max<LayoutUnit>(1, (availWidth + colGap) / (colWidth + colGap));
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    } else {
        desiredColumnCount = std::max<LayoutUnit>(std::min<LayoutUnit>(colCount, (availWidth + colGap) / (colWidth + colGap)), 1);
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    }
    setDesiredColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
}

bool LayoutBlock::requiresColumns(int desiredColumnCount) const
{
    // Paged overflow is treated as multicol here, unless this element was the one that got its
    // overflow propagated to the viewport.
    bool isPaginated = style()->isOverflowPaged() && node() != document().viewportDefiningElement();

    return firstChild()
        && (desiredColumnCount != 1 || !style()->hasAutoColumnWidth() || isPaginated)
        && !firstChild()->isAnonymousColumnsBlock()
        && !firstChild()->isAnonymousColumnSpanBlock() && !isFlexibleBoxIncludingDeprecated();
}

void LayoutBlock::setDesiredColumnCountAndWidth(int count, LayoutUnit width)
{
    bool destroyColumns = !requiresColumns(count);
    if (destroyColumns) {
        if (hasColumns()) {
            gColumnInfoMap->take(this);
            setHasColumns(false);
        }
    } else {
        ColumnInfo* info;
        if (hasColumns()) {
            info = gColumnInfoMap->get(this);
        } else {
            if (!gColumnInfoMap)
                gColumnInfoMap = new ColumnInfoMap;
            info = new ColumnInfo;
            gColumnInfoMap->add(this, adoptPtr(info));
            setHasColumns(true);
        }
        info->setDesiredColumnWidth(width);
        if (style()->isOverflowPaged()) {
            info->setDesiredColumnCount(1);
            info->setProgressionAxis(style()->hasInlinePaginationAxis() ? ColumnInfo::InlineAxis : ColumnInfo::BlockAxis);
        } else {
            info->setDesiredColumnCount(count);
            info->setProgressionAxis(ColumnInfo::InlineAxis);
        }
    }
}

LayoutUnit LayoutBlock::desiredColumnWidth() const
{
    if (!hasColumns())
        return contentLogicalWidth();
    return gColumnInfoMap->get(this)->desiredColumnWidth();
}

ColumnInfo* LayoutBlock::columnInfo() const
{
    if (!hasColumns())
        return 0;
    return gColumnInfoMap->get(this);
}

unsigned LayoutBlock::columnCount(ColumnInfo* colInfo) const
{
    ASSERT(hasColumns());
    ASSERT(gColumnInfoMap->get(this) == colInfo);
    return colInfo->columnCount();
}

LayoutRect LayoutBlock::columnRectAt(ColumnInfo* colInfo, unsigned index) const
{
    ASSERT(hasColumns() && gColumnInfoMap->get(this) == colInfo);

    // Compute the appropriate rect based off our information.
    LayoutUnit colLogicalWidth = colInfo->desiredColumnWidth();
    LayoutUnit colLogicalHeight = colInfo->columnHeight();
    LayoutUnit colLogicalTop = borderBefore() + paddingBefore();
    LayoutUnit colLogicalLeft = logicalLeftOffsetForContent();
    LayoutUnit colGap = columnGap();
    if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
        if (style()->isLeftToRightDirection())
            colLogicalLeft += index * (colLogicalWidth + colGap);
        else
            colLogicalLeft += contentLogicalWidth() - colLogicalWidth - index * (colLogicalWidth + colGap);
    } else {
        colLogicalTop += index * (colLogicalHeight + colGap);
    }

    if (isHorizontalWritingMode())
        return LayoutRect(colLogicalLeft, colLogicalTop, colLogicalWidth, colLogicalHeight);
    return LayoutRect(colLogicalTop, colLogicalLeft, colLogicalHeight, colLogicalWidth);
}

void LayoutBlock::adjustPointToColumnContents(LayoutPoint& point) const
{
    // Just bail if we have no columns.
    if (!hasColumns())
        return;

    ColumnInfo* colInfo = columnInfo();
    if (!columnCount(colInfo))
        return;

    // Determine which columns we intersect.
    LayoutUnit colGap = columnGap();
    LayoutUnit halfColGap = colGap / 2;
    LayoutPoint columnPoint(columnRectAt(colInfo, 0).location());
    LayoutUnit logicalOffset = 0;
    for (unsigned i = 0; i < colInfo->columnCount(); i++) {
        // Add in half the column gap to the left and right of the rect.
        LayoutRect colRect = columnRectAt(colInfo, i);
        flipForWritingMode(colRect);
        if (isHorizontalWritingMode() == (colInfo->progressionAxis() == ColumnInfo::InlineAxis)) {
            LayoutRect gapAndColumnRect(colRect.x() - halfColGap, colRect.y(), colRect.width() + colGap, colRect.height());
            if (point.x() >= gapAndColumnRect.x() && point.x() < gapAndColumnRect.maxX()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
                    // FIXME: The clamping that follows is not completely right for right-to-left
                    // content.
                    if (point.y() < gapAndColumnRect.y()) {
                        // Clamp everything above the column to its top left.
                        point = gapAndColumnRect.location();
                    } else if (point.y() >= gapAndColumnRect.maxY()) {
                        // Clamp everything below the column to the next column's top left. If there is
                        // no next column, this still maps to just after this column.
                        point = gapAndColumnRect.location();
                        point.move(0, gapAndColumnRect.height());
                    }
                } else {
                    if (point.x() < colRect.x())
                        point.setX(colRect.x());
                    else if (point.x() >= colRect.maxX())
                        point.setX(colRect.maxX() - 1);
                }

                // We're inside the column.  Translate the x and y into our column coordinate space.
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    point.move(columnPoint.x() - colRect.x(), (!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset));
                else
                    point.move((!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset) - colRect.x() + borderLeft() + paddingLeft(), 0);
                return;
            }

            // Move to the next position.
            logicalOffset += colInfo->progressionAxis() == ColumnInfo::InlineAxis ? colRect.height() : colRect.width();
        } else {
            LayoutRect gapAndColumnRect(colRect.x(), colRect.y() - halfColGap, colRect.width(), colRect.height() + colGap);
            if (point.y() >= gapAndColumnRect.y() && point.y() < gapAndColumnRect.maxY()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
                    // FIXME: The clamping that follows is not completely right for right-to-left
                    // content.
                    if (point.x() < gapAndColumnRect.x()) {
                        // Clamp everything above the column to its top left.
                        point = gapAndColumnRect.location();
                    } else if (point.x() >= gapAndColumnRect.maxX()) {
                        // Clamp everything below the column to the next column's top left. If there is
                        // no next column, this still maps to just after this column.
                        point = gapAndColumnRect.location();
                        point.move(gapAndColumnRect.width(), 0);
                    }
                } else {
                    if (point.y() < colRect.y())
                        point.setY(colRect.y());
                    else if (point.y() >= colRect.maxY())
                        point.setY(colRect.maxY() - 1);
                }

                // We're inside the column.  Translate the x and y into our column coordinate space.
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    point.move((!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset), columnPoint.y() - colRect.y());
                else
                    point.move(0, (!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset) - colRect.y() + borderTop() + paddingTop());
                return;
            }

            // Move to the next position.
            logicalOffset += colInfo->progressionAxis() == ColumnInfo::InlineAxis ? colRect.width() : colRect.height();
        }
    }
}

void LayoutBlock::adjustRectForColumns(LayoutRect& r) const
{
    // Just bail if we have no columns.
    if (!hasColumns())
        return;

    ColumnInfo* colInfo = columnInfo();

    // Determine which columns we intersect.
    unsigned colCount = columnCount(colInfo);
    if (!colCount)
        return;

    // Begin with a result rect that is empty.
    LayoutRect result;

    bool isHorizontal = isHorizontalWritingMode();
    LayoutUnit beforeBorderPadding = borderBefore() + paddingBefore();
    LayoutUnit colHeight = colInfo->columnHeight();
    if (!colHeight)
        return;

    LayoutUnit startOffset = std::max(isHorizontal ? r.y() : r.x(), beforeBorderPadding);
    LayoutUnit endOffset = std::max(std::min<LayoutUnit>(isHorizontal ? r.maxY() : r.maxX(), beforeBorderPadding + colCount * colHeight), beforeBorderPadding);

    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned startColumn = (startOffset - beforeBorderPadding) / colHeight;
    unsigned endColumn = (endOffset - beforeBorderPadding) / colHeight;

    if (startColumn == endColumn) {
        // The rect is fully contained within one column. Adjust for our offsets
        // and issue paint invalidations only that portion.
        LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent();
        LayoutRect colRect = columnRectAt(colInfo, startColumn);
        LayoutRect paintInvalidationRect = r;

        if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
            if (isHorizontal)
                paintInvalidationRect.move(colRect.x() - logicalLeftOffset, - static_cast<int>(startColumn) * colHeight);
            else
                paintInvalidationRect.move(- static_cast<int>(startColumn) * colHeight, colRect.y() - logicalLeftOffset);
        } else {
            if (isHorizontal)
                paintInvalidationRect.move(0, colRect.y() - startColumn * colHeight - beforeBorderPadding);
            else
                paintInvalidationRect.move(colRect.x() - startColumn * colHeight - beforeBorderPadding, 0);
        }
        paintInvalidationRect.intersect(colRect);
        result.unite(paintInvalidationRect);
    } else {
        // We span multiple columns. We can just unite the start and end column to get the final
        // paint invalidation rect.
        result.unite(columnRectAt(colInfo, startColumn));
        result.unite(columnRectAt(colInfo, endColumn));
    }

    r = result;
}

LayoutPoint LayoutBlock::flipForWritingModeIncludingColumns(const LayoutPoint& point) const
{
    ASSERT(hasColumns());
    if (!hasColumns() || !style()->isFlippedBlocksWritingMode())
        return point;
    ColumnInfo* colInfo = columnInfo();
    LayoutUnit columnLogicalHeight = colInfo->columnHeight();
    LayoutUnit expandedLogicalHeight = borderBefore() + paddingBefore() + columnCount(colInfo) * columnLogicalHeight + borderAfter() + paddingAfter() + scrollbarLogicalHeight();
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), expandedLogicalHeight - point.y());
    return LayoutPoint(expandedLogicalHeight - point.x(), point.y());
}

void LayoutBlock::adjustStartEdgeForWritingModeIncludingColumns(LayoutRect& rect) const
{
    ASSERT(hasColumns());
    if (!hasColumns() || !style()->isFlippedBlocksWritingMode())
        return;

    ColumnInfo* colInfo = columnInfo();
    LayoutUnit columnLogicalHeight = colInfo->columnHeight();
    LayoutUnit expandedLogicalHeight = borderBefore() + paddingBefore() + columnCount(colInfo) * columnLogicalHeight + borderAfter() + paddingAfter() + scrollbarLogicalHeight();

    if (isHorizontalWritingMode())
        rect.setY(expandedLogicalHeight - rect.maxY());
    else
        rect.setX(expandedLogicalHeight - rect.maxX());
}

LayoutSize LayoutBlock::columnOffset(const LayoutPoint& point) const
{
    if (!hasColumns())
        return LayoutSize();

    ColumnInfo* colInfo = columnInfo();

    LayoutUnit logicalLeft = logicalLeftOffsetForContent();
    unsigned colCount = columnCount(colInfo);
    LayoutUnit colLogicalWidth = colInfo->desiredColumnWidth();
    LayoutUnit colLogicalHeight = colInfo->columnHeight();

    for (unsigned i = 0; i < colCount; ++i) {
        // Compute the edges for a given column in the block progression direction.
        LayoutRect sliceRect = LayoutRect(logicalLeft, borderBefore() + paddingBefore() + i * colLogicalHeight, colLogicalWidth, colLogicalHeight);
        if (!isHorizontalWritingMode())
            sliceRect = sliceRect.transposedRect();

        LayoutUnit logicalOffset = i * colLogicalHeight;

        // Now we're in the same coordinate space as the point.  See if it is inside the rectangle.
        if (isHorizontalWritingMode()) {
            if (point.y() >= sliceRect.y() && point.y() < sliceRect.maxY()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    return LayoutSize(columnRectAt(colInfo, i).x() - logicalLeft, -logicalOffset);
                return LayoutSize(0, columnRectAt(colInfo, i).y() - logicalOffset - borderBefore() - paddingBefore());
            }
        } else {
            if (point.x() >= sliceRect.x() && point.x() < sliceRect.maxX()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    return LayoutSize(-logicalOffset, columnRectAt(colInfo, i).y() - logicalLeft);
                return LayoutSize(columnRectAt(colInfo, i).x() - logicalOffset - borderBefore() - paddingBefore(), 0);
            }
        }
    }

    return LayoutSize();
}

void LayoutBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (childrenInline()) {
        // FIXME: Remove this const_cast.
        toLayoutBlockFlow(const_cast<LayoutBlock*>(this))->computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    } else {
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    // The flow thread based multicol implementation will do this adjustment on the flow thread, and
    // not here on the multicol container, so that spanners won't incorrectly be treated as column
    // content (and have spanners' preferred widths multiplied by the number of columns, etc.).
    if (style()->specifiesColumns() && !document().regionBasedColumnsEnabled())
        adjustIntrinsicLogicalWidthsForColumns(minLogicalWidth, maxLogicalWidth);

    if (isTableCell()) {
        Length tableCellWidth = toLayoutTableCell(this)->styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(tableCellWidth.value()));
    }

    int scrollbarWidth = intrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void LayoutBlock::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    // FIXME: The isFixed() calls here should probably be checking for isSpecified since you
    // should be able to use percentage, calc or viewport relative values for width.
    const LayoutStyle& styleToUse = styleRef();
    if (!isTableCell() && styleToUse.logicalWidth().isFixed() && styleToUse.logicalWidth().value() >= 0
        && !(isDeprecatedFlexItem() && !styleToUse.logicalWidth().intValue()))
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
    }

    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
    }

    // Table layout uses integers, ceil the preferred widths to ensure that they can contain the contents.
    if (isTableCell()) {
        m_minPreferredLogicalWidth = m_minPreferredLogicalWidth.ceil();
        m_maxPreferredLogicalWidth = m_maxPreferredLogicalWidth.ceil();
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    clearPreferredLogicalWidthsDirty();
}

void LayoutBlock::adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!document().regionBasedColumnsEnabled());
    if (!style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth()) {
        // The min/max intrinsic widths calculated really tell how much space elements need when
        // laid out inside the columns. In order to eventually end up with the desired column width,
        // we need to convert them to values pertaining to the multicol container.
        int columnCount = style()->hasAutoColumnCount() ? 1 : style()->columnCount();
        LayoutUnit columnWidth;
        LayoutUnit gapExtra = (columnCount - 1) * columnGap();
        if (style()->hasAutoColumnWidth()) {
            minLogicalWidth = minLogicalWidth * columnCount + gapExtra;
        } else {
            columnWidth = style()->columnWidth();
            minLogicalWidth = std::min(minLogicalWidth, columnWidth);
        }
        // FIXME: If column-count is auto here, we should resolve it to calculate the maximum
        // intrinsic width, instead of pretending that it's 1. The only way to do that is by
        // performing a layout pass, but this is not an appropriate time or place for layout. The
        // good news is that if height is unconstrained and there are no explicit breaks, the
        // resolved column-count really should be 1.
        maxLogicalWidth = std::max(maxLogicalWidth, columnWidth) * columnCount + gapExtra;
    }
}

void LayoutBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const LayoutStyle& styleToUse = styleRef();
    bool nowrap = styleToUse.whiteSpace() == NOWRAP;

    LayoutObject* child = firstChild();
    LayoutBlock* containingBlock = this->containingBlock();
    LayoutUnit floatLeftWidth = 0, floatRightWidth = 0;
    while (child) {
        // Positioned children don't affect the min/max width. Spanners only affect the min/max
        // width of the multicol container, not the flow thread.
        if (child->isOutOfFlowPositioned() || child->isColumnSpanAll()) {
            child = child->nextSibling();
            continue;
        }

        RefPtr<LayoutStyle> childStyle = child->style();
        if (child->isFloating() || (child->isBox() && toLayoutBox(child)->avoidsFloats())) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (childStyle->clear() & CLEFT) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = 0;
            }
            if (childStyle->clear() & CRIGHT) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = 0;
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length startMarginLength = childStyle->marginStartUsing(&styleToUse);
        Length endMarginLength = childStyle->marginEndUsing(&styleToUse);
        LayoutUnit margin = 0;
        LayoutUnit marginStart = 0;
        LayoutUnit marginEnd = 0;
        if (startMarginLength.isFixed())
            marginStart += startMarginLength.value();
        if (endMarginLength.isFixed())
            marginEnd += endMarginLength.value();
        margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
        if (child->isBox() && child->isHorizontalWritingMode() != isHorizontalWritingMode()) {
            LayoutBox* childBox = toLayoutBox(child);
            LogicalExtentComputedValues computedValues;
            childBox->computeLogicalHeight(childBox->borderAndPaddingLogicalHeight(), 0, computedValues);
            childMinPreferredLogicalWidth = childMaxPreferredLogicalWidth = computedValues.m_extent;
        } else {
            childMinPreferredLogicalWidth = child->minPreferredLogicalWidth();
            childMaxPreferredLogicalWidth = child->maxPreferredLogicalWidth();
        }

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
            floatLeftWidth = floatRightWidth = 0;
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
    minLogicalWidth = std::max<LayoutUnit>(0, minLogicalWidth);
    maxLogicalWidth = std::max<LayoutUnit>(0, maxLogicalWidth);

    maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
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
    if (isReplaced() && linePositionMode == PositionOnContainingLine)
        return LayoutBox::lineHeight(firstLine, direction, linePositionMode);

    const LayoutStyle& style = styleRef(firstLine && document().styleEngine().usesFirstLineRules());
    return style.computedLineHeight();
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

        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.
        // We give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved).
        bool ignoreBaseline = (layer() && layer()->scrollableArea()
            && (direction == HorizontalLine
                ? (layer()->scrollableArea()->verticalScrollbar() || layer()->scrollableArea()->scrollYOffset())
                : (layer()->scrollableArea()->horizontalScrollbar() || layer()->scrollableArea()->scrollXOffset())))
            || (isWritingModeRoot() && !isRubyRun());

        int baselinePos = ignoreBaseline ? -1 : inlineBlockBaseline(direction);

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

    const FontMetrics& fontMetrics = style(firstLine)->fontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
}

LayoutUnit LayoutBlock::minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const
{
    if (!document().inNoQuirksMode() && replacedHeight)
        return replacedHeight;

    if (!(style(isFirstLine)->lineBoxContain() & LineBoxContainBlock))
        return LayoutUnit();

    return std::max<LayoutUnit>(replacedHeight, lineHeight(isFirstLine, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

int LayoutBlock::firstLineBoxBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (childrenInline()) {
        if (firstLineBox())
            return firstLineBox()->logicalTop() + style(true)->fontMetrics().ascent(firstRootBox()->baselineType());
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

int LayoutBlock::inlineBlockBaseline(LineDirectionMode direction) const
{
    if (!style()->isOverflowVisible()) {
        // We are not calling LayoutBox::baselinePosition here because the caller should add the margin-top/margin-right, not us.
        return direction == HorizontalLine ? size().height() + marginBottom() : size().width() + marginLeft();
    }

    return lastLineBoxBaseline(direction);
}

int LayoutBlock::lastLineBoxBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (childrenInline()) {
        if (!firstLineBox() && hasLineIfEmpty()) {
            const FontMetrics& fontMetrics = firstLineStyle()->fontMetrics();
            return fontMetrics.ascent()
                + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
                + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
        }
        if (lastLineBox())
            return lastLineBox()->logicalTop() + style(lastLineBox() == firstLineBox())->fontMetrics().ascent(lastRootBox()->baselineType());
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
        const FontMetrics& fontMetrics = firstLineStyle()->fontMetrics();
        return fontMetrics.ascent()
            + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
            + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
    }
    return -1;
}

static inline bool isLayoutBlockFlowOrLayoutButton(LayoutObject* layoutObject)
{
    // We include isLayoutButton in this check because buttons are implemented
    // using flex box but should still support first-line|first-letter.
    // The flex box and grid specs require that flex box and grid do not
    // support first-line|first-letter, though.
    // FIXME: Remove when buttons are implemented with align-items instead
    // of flex box.
    return layoutObject->isLayoutBlockFlow() || layoutObject->isLayoutButton();
}

LayoutBlock* LayoutBlock::firstLineBlock() const
{
    LayoutBlock* firstLineBlock = const_cast<LayoutBlock*>(this);
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style()->hasPseudoStyle(FIRST_LINE);
        if (hasPseudo)
            break;
        LayoutObject* parentBlock = firstLineBlock->parent();
        if (firstLineBlock->isReplaced() || firstLineBlock->isFloatingOrOutOfFlowPositioned()
            || !parentBlock
            || !isLayoutBlockFlowOrLayoutButton(parentBlock))
            break;
        ASSERT_WITH_SECURITY_IMPLICATION(parentBlock->isLayoutBlock());
        if (toLayoutBlock(parentBlock)->firstChild() != firstLineBlock)
            break;
        firstLineBlock = toLayoutBlock(parentBlock);
    }

    if (!hasPseudo)
        return 0;

    return firstLineBlock;
}

// Helper methods for obtaining the last line, computing line counts and heights for line counts
// (crawling into blocks).
static bool shouldCheckLines(LayoutObject* obj)
{
    return !obj->isFloatingOrOutOfFlowPositioned()
        && obj->isLayoutBlock() && obj->style()->height().isAuto()
        && (!obj->isDeprecatedFlexibleBox() || obj->style()->boxOrient() == VERTICAL);
}

static int getHeightForLineCount(LayoutBlock* block, int l, bool includeBottom, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->isLayoutBlockFlow() && block->childrenInline()) {
            for (RootInlineBox* box = toLayoutBlockFlow(block)->firstRootBox(); box; box = box->nextRootBox()) {
                if (++count == l)
                    return box->lineBottom() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : LayoutUnit());
            }
        } else {
            LayoutBox* normalFlowChildWithoutLines = 0;
            for (LayoutBox* obj = block->firstChildBox(); obj; obj = obj->nextSiblingBox()) {
                if (shouldCheckLines(obj)) {
                    int result = getHeightForLineCount(toLayoutBlock(obj), l, false, count);
                    if (result != -1)
                        return result + obj->location().y() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : LayoutUnit());
                } else if (!obj->isFloatingOrOutOfFlowPositioned()) {
                    normalFlowChildWithoutLines = obj;
                }
            }
            if (normalFlowChildWithoutLines && l == 0)
                return normalFlowChildWithoutLines->location().y() + normalFlowChildWithoutLines->size().height();
        }
    }

    return -1;
}

RootInlineBox* LayoutBlock::lineAtIndex(int i) const
{
    ASSERT(i >= 0);

    if (style()->visibility() != VISIBLE)
        return 0;

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

    return 0;
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

int LayoutBlock::heightForLineCount(int l)
{
    int count = 0;
    return getHeightForLineCount(this, l, true, count);
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

void LayoutBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for vertical writing-modes.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        LayoutRect localRect(LayoutPoint(), size());
        localRect.expand(collapsedMarginBoxLogicalOutsets());
        quads.append(localToAbsoluteQuad(FloatRect(localRect), 0 /* mode */, wasFixed));
        continuation()->absoluteQuads(quads, wasFixed);
    } else {
        quads.append(LayoutBox::localToAbsoluteQuad(FloatRect(0, 0, size().width().toFloat(), size().height().toFloat()), 0 /* mode */, wasFixed));
    }
}

LayoutRect LayoutBlock::rectWithOutlineForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, LayoutUnit outlineWidth, const PaintInvalidationState* paintInvalidationState) const
{
    LayoutRect r(LayoutBox::rectWithOutlineForPaintInvalidation(paintInvalidationContainer, outlineWidth, paintInvalidationState));
    if (isAnonymousBlockContinuation())
        r.inflateY(collapsedMarginBefore()); // FIXME: This is wrong for vertical writing-modes.
    return r;
}

LayoutObject* LayoutBlock::hoverAncestor() const
{
    return isAnonymousBlockContinuation() ? continuation() : LayoutBox::hoverAncestor();
}

void LayoutBlock::updateDragState(bool dragOn)
{
    LayoutBox::updateDragState(dragOn);
    if (continuation())
        continuation()->updateDragState(dragOn);
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

    if (Node* n = nodeForHitTest()) {
        result.setInnerNode(n);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(n);
        result.setLocalPoint(point);
    }
}

// An inline-block uses its inlineBox as the inlineBoxWrapper,
// so the firstChild() is nullptr if the only child is an empty inline-block.
inline bool LayoutBlock::isInlineBoxWrapperActuallyChild() const
{
    return isInlineBlockOrInlineTable() && !size().isEmpty() && node() && editingIgnoresContent(node());
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

void LayoutBlock::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (inlineElementContinuation()) {
        // FIXME: This check really isn't accurate.
        bool nextInlineHasLineBox = inlineElementContinuation()->firstLineBox();
        // FIXME: This is wrong. The principal renderer may not be the continuation preceding this block.
        // FIXME: This is wrong for vertical writing-modes.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = toLayoutInline(inlineElementContinuation()->node()->renderer())->firstLineBox();
        LayoutUnit topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : LayoutUnit();
        LayoutUnit bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : LayoutUnit();
        LayoutRect rect(additionalOffset, size());
        rect.expandEdges(topMargin, 0, bottomMargin, 0);

        if (!rect.isEmpty())
            rects.append(rect);
    } else if (size().width() && size().height()) {
        rects.append(LayoutRect(additionalOffset, size()));
    }

    if (!hasOverflowClip() && !hasControlClip()) {
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
            LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
            LayoutRect rect(additionalOffset.x() + curr->x(), additionalOffset.y() + top, curr->width(), bottom - top);
            if (!rect.isEmpty())
                rects.append(rect);
        }

        addChildFocusRingRects(rects, additionalOffset);
    }

    if (inlineElementContinuation())
        inlineElementContinuation()->addFocusRingRects(rects, additionalOffset + (inlineElementContinuation()->containingBlock()->location() - location()));
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
    if (isAnonymousColumnsBlock())
        return createAnonymousColumnsWithParentRenderer(parent);
    if (isAnonymousColumnSpanBlock())
        return createAnonymousColumnSpanWithParentRenderer(parent);
    return createAnonymousWithParentRendererAndDisplay(parent, style()->display());
}

LayoutUnit LayoutBlock::nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return logicalOffset;

    // The logicalOffset is in our coordinate space.  We can add in our pushed offset.
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset);
    if (pageBoundaryRule == ExcludePageBoundary)
        return logicalOffset + (remainingLogicalHeight ? remainingLogicalHeight : pageLogicalHeight);
    return logicalOffset + remainingLogicalHeight;
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
        if (pageBoundaryRule == IncludePageBoundary) {
            // If includeBoundaryPoint is true the line exactly on the top edge of a
            // column will act as being part of the previous column.
            remainingHeight = intMod(remainingHeight, pageLogicalHeight);
        }
        return remainingHeight;
    }

    return flowThread->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);
}

void LayoutBlock::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (LayoutFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->setPageBreak(offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

void LayoutBlock::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (LayoutFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->updateMinimumPageHeight(offsetFromLogicalTopOfFirstPage() + offset, minHeight);
    else if (ColumnInfo* colInfo = view()->layoutState()->columnInfo())
        colInfo->updateMinimumColumnHeight(minHeight);
}

LayoutUnit LayoutBlock::offsetFromLogicalTopOfFirstPage() const
{
    LayoutState* layoutState = view()->layoutState();
    RELEASE_ASSERT(layoutState);
    if (!layoutState->isPaginated())
        return LayoutUnit();
    // It would be possible to remove the requirement that this block be the one currently being
    // laid out, but nobody needs that at the moment.
    ASSERT(layoutState->renderer() == this);
    LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
    return isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
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

LayoutBlock* LayoutBlock::createAnonymousWithParentRendererAndDisplay(const LayoutObject* parent, EDisplay display)
{
    // FIXME: Do we need to convert all our inline displays to block-type in the anonymous logic ?
    EDisplay newDisplay;
    LayoutBlock* newBox = 0;
    if (display == FLEX || display == INLINE_FLEX) {
        newBox = LayoutFlexibleBox::createAnonymous(&parent->document());
        newDisplay = FLEX;
    } else {
        newBox = LayoutBlockFlow::createAnonymous(&parent->document());
        newDisplay = BLOCK;
    }

    RefPtr<LayoutStyle> newStyle = LayoutStyle::createAnonymousStyleWithDisplay(parent->styleRef(), newDisplay);
    parent->updateAnonymousChildStyle(*newBox, *newStyle);
    newBox->setStyle(newStyle.release());
    return newBox;
}

LayoutBlockFlow* LayoutBlock::createAnonymousColumnsWithParentRenderer(const LayoutObject* parent)
{
    RefPtr<LayoutStyle> newStyle = LayoutStyle::createAnonymousStyleWithDisplay(parent->styleRef(), BLOCK);
    newStyle->inheritColumnPropertiesFrom(parent->styleRef());

    LayoutBlockFlow* newBox = LayoutBlockFlow::createAnonymous(&parent->document());
    parent->updateAnonymousChildStyle(*newBox, *newStyle);
    newBox->setStyle(newStyle.release());
    return newBox;
}

LayoutBlockFlow* LayoutBlock::createAnonymousColumnSpanWithParentRenderer(const LayoutObject* parent)
{
    RefPtr<LayoutStyle> newStyle = LayoutStyle::createAnonymousStyleWithDisplay(parent->styleRef(), BLOCK);
    newStyle->setColumnSpan(ColumnSpanAll);

    LayoutBlockFlow* newBox = LayoutBlockFlow::createAnonymous(&parent->document());
    parent->updateAnonymousChildStyle(*newBox, *newStyle);
    newBox->setStyle(newStyle.release());
    return newBox;
}

static bool recalcNormalFlowChildOverflowIfNeeded(LayoutObject* renderer)
{
    if (renderer->isOutOfFlowPositioned() || !renderer->needsOverflowRecalcAfterStyleChange())
        return false;

    ASSERT(renderer->isLayoutBlock());
    return toLayoutBlock(renderer)->recalcOverflowAfterStyleChange();
}

bool LayoutBlock::recalcChildOverflowAfterStyleChange()
{
    ASSERT(childNeedsOverflowRecalcAfterStyleChange());
    setChildNeedsOverflowRecalcAfterStyleChange(false);

    bool childrenOverflowChanged = false;

    if (childrenInline()) {
        ListHashSet<RootInlineBox*> lineBoxes;
        for (InlineWalker walker(this); !walker.atEnd(); walker.advance()) {
            LayoutObject* renderer = walker.current();
            if (recalcNormalFlowChildOverflowIfNeeded(renderer)) {
                childrenOverflowChanged = true;
                if (InlineBox* inlineBoxWrapper = toLayoutBlock(renderer)->inlineBoxWrapper())
                    lineBoxes.add(&inlineBoxWrapper->root());
            }
        }

        // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
        GlyphOverflowAndFallbackFontsMap textBoxDataMap;
        for (ListHashSet<RootInlineBox*>::const_iterator it = lineBoxes.begin(); it != lineBoxes.end(); ++it) {
            RootInlineBox* box = *it;
            box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
        }
    } else {
        for (LayoutBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (recalcNormalFlowChildOverflowIfNeeded(box))
                childrenOverflowChanged = true;
        }
    }

    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return childrenOverflowChanged;

    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        LayoutBox* box = *it;

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

    setSelfNeedsOverflowRecalcAfterStyleChange(false);
    // If the current block needs layout, overflow will be recalculated during
    // layout time anyway. We can safely exit here.
    if (needsLayout())
        return false;

    LayoutUnit oldClientAfterEdge = hasOverflowModel() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
    computeOverflow(oldClientAfterEdge, true);

    if (hasOverflowClip())
        layer()->scrollableArea()->updateAfterOverflowRecalc();

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
    updateLogicalHeight();
    return !hasPercentHeightDescendants() || oldHeight == logicalHeight();
}

#if ENABLE(ASSERT)
void LayoutBlock::checkPositionedObjectsNeedLayout()
{
    if (!gPositionedDescendantsMap)
        return;

    if (TrackedRendererListHashSet* positionedDescendantSet = positionedObjects()) {
        TrackedRendererListHashSet::const_iterator end = positionedDescendantSet->end();
        for (TrackedRendererListHashSet::const_iterator it = positionedDescendantSet->begin(); it != end; ++it) {
            LayoutBox* currBox = *it;
            ASSERT(!currBox->needsLayout());
        }
    }
}

bool LayoutBlock::paintsContinuationOutline(LayoutInline* flow)
{
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    if (table->isEmpty())
        return false;

    ListHashSet<LayoutInline*>* continuations = table->get(this);
    if (!continuations)
        return false;

    return continuations->contains(flow);
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
