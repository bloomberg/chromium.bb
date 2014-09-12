/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/rendering/RenderBoxModelObject.h"

#include "core/page/scrolling/ScrollingConstraints.h"
#include "core/rendering/ImageQualityController.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObjectInlines.h"
#include "core/rendering/RenderRegion.h"
#include "core/rendering/RenderTextFragment.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/rendering/style/BorderEdge.h"
#include "core/rendering/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/Path.h"
#include "wtf/CurrentTime.h"

namespace blink {

// The HashMap for storing continuation pointers.
// An inline can be split with blocks occuring in between the inline content.
// When this occurs we need a pointer to the next object. We can basically be
// split into a sequence of inlines and blocks. The continuation will either be
// an anonymous block (that houses other blocks) or it will be an inline flow.
// <b><i><p>Hello</p></i></b>. In this example the <i> will have a block as
// its continuation but the <b> will just have an inline as its continuation.
typedef WillBeHeapHashMap<RawPtrWillBeMember<const RenderBoxModelObject>, RawPtrWillBeMember<RenderBoxModelObject> > ContinuationMap;
static OwnPtrWillBePersistent<ContinuationMap>* continuationMap = 0;

// This HashMap is similar to the continuation map, but connects first-letter
// renderers to their remaining text fragments.
typedef WillBeHeapHashMap<RawPtrWillBeMember<const RenderBoxModelObject>, RawPtrWillBeMember<RenderTextFragment> > FirstLetterRemainingTextMap;
static OwnPtrWillBePersistent<FirstLetterRemainingTextMap>* firstLetterRemainingTextMap = 0;

void RenderBoxModelObject::setSelectionState(SelectionState state)
{
    if (state == SelectionInside && selectionState() != SelectionNone)
        return;

    if ((state == SelectionStart && selectionState() == SelectionEnd)
        || (state == SelectionEnd && selectionState() == SelectionStart))
        RenderObject::setSelectionState(SelectionBoth);
    else
        RenderObject::setSelectionState(state);

    // FIXME: We should consider whether it is OK propagating to ancestor RenderInlines.
    // This is a workaround for http://webkit.org/b/32123
    // The containing block can be null in case of an orphaned tree.
    RenderBlock* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isRenderView())
        containingBlock->setSelectionState(state);
}

void RenderBoxModelObject::contentChanged(ContentChangeType changeType)
{
    if (!hasLayer())
        return;

    layer()->contentChanged(changeType);
}

bool RenderBoxModelObject::hasAcceleratedCompositing() const
{
    return view()->compositor()->hasAcceleratedCompositing();
}

RenderBoxModelObject::RenderBoxModelObject(ContainerNode* node)
    : RenderLayerModelObject(node)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
}

void RenderBoxModelObject::willBeDestroyed()
{
    ImageQualityController::remove(this);

    // A continuation of this RenderObject should be destroyed at subclasses.
    ASSERT(!continuation());

    // If this is a first-letter object with a remaining text fragment then the
    // entry needs to be cleared from the map.
    if (firstLetterRemainingText())
        setFirstLetterRemainingText(0);

    RenderLayerModelObject::willBeDestroyed();
}

bool RenderBoxModelObject::calculateHasBoxDecorations() const
{
    RenderStyle* styleToUse = style();
    ASSERT(styleToUse);
    return hasBackground() || styleToUse->hasBorder() || styleToUse->hasAppearance() || styleToUse->boxShadow();
}

void RenderBoxModelObject::updateFromStyle()
{
    RenderLayerModelObject::updateFromStyle();

    RenderStyle* styleToUse = style();
    setHasBoxDecorationBackground(calculateHasBoxDecorations());
    setInline(styleToUse->isDisplayInlineType());
    setPositionState(styleToUse->position());
    setHorizontalWritingMode(styleToUse->isHorizontalWritingMode());
}

static LayoutSize accumulateInFlowPositionOffsets(const RenderObject* child)
{
    if (!child->isAnonymousBlock() || !child->isRelPositioned())
        return LayoutSize();
    LayoutSize offset;
    RenderObject* p = toRenderBlock(child)->inlineElementContinuation();
    while (p && p->isRenderInline()) {
        if (p->isRelPositioned()) {
            RenderInline* renderInline = toRenderInline(p);
            offset += renderInline->offsetForInFlowPosition();
        }
        p = p->parent();
    }
    return offset;
}

bool RenderBoxModelObject::hasAutoHeightOrContainingBlockWithAutoHeight() const
{
    Length logicalHeightLength = style()->logicalHeight();
    if (logicalHeightLength.isAuto())
        return true;

    // For percentage heights: The percentage is calculated with respect to the height of the generated box's
    // containing block. If the height of the containing block is not specified explicitly (i.e., it depends
    // on content height), and this element is not absolutely positioned, the value computes to 'auto'.
    if (!logicalHeightLength.isPercent() || isOutOfFlowPositioned() || document().inQuirksMode())
        return false;

    // Anonymous block boxes are ignored when resolving percentage values that would refer to it:
    // the closest non-anonymous ancestor box is used instead.
    RenderBlock* cb = containingBlock();
    while (cb->isAnonymous())
        cb = cb->containingBlock();

    // Matching RenderBox::percentageLogicalHeightIsResolvableFromBlock() by
    // ignoring table cell's attribute value, where it says that table cells violate
    // what the CSS spec says to do with heights. Basically we
    // don't care if the cell specified a height or not.
    if (cb->isTableCell())
        return false;

    // Match RenderBox::availableLogicalHeightUsing by special casing
    // the render view. The available height is taken from the frame.
    if (cb->isRenderView())
        return false;

    if (cb->isOutOfFlowPositioned() && !cb->style()->logicalTop().isAuto() && !cb->style()->logicalBottom().isAuto())
        return false;

    // If the height of the containing block computes to 'auto', then it hasn't been 'specified explicitly'.
    return cb->hasAutoHeightOrContainingBlockWithAutoHeight();
}

LayoutSize RenderBoxModelObject::relativePositionOffset() const
{
    LayoutSize offset = accumulateInFlowPositionOffsets(this);

    RenderBlock* containingBlock = this->containingBlock();

    // Objects that shrink to avoid floats normally use available line width when computing containing block width.  However
    // in the case of relative positioning using percentages, we can't do this.  The offset should always be resolved using the
    // available width of the containing block.  Therefore we don't use containingBlockLogicalWidthForContent() here, but instead explicitly
    // call availableWidth on our containing block.
    if (!style()->left().isAuto()) {
        if (!style()->right().isAuto() && !containingBlock->style()->isLeftToRightDirection())
            offset.setWidth(-valueForLength(style()->right(), containingBlock->availableWidth()));
        else
            offset.expand(valueForLength(style()->left(), containingBlock->availableWidth()), 0);
    } else if (!style()->right().isAuto()) {
        offset.expand(-valueForLength(style()->right(), containingBlock->availableWidth()), 0);
    }

    // If the containing block of a relatively positioned element does not
    // specify a height, a percentage top or bottom offset should be resolved as
    // auto. An exception to this is if the containing block has the WinIE quirk
    // where <html> and <body> assume the size of the viewport. In this case,
    // calculate the percent offset based on this height.
    // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
    if (!style()->top().isAuto()
        && (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight()
            || !style()->top().isPercent()
            || containingBlock->stretchesToViewport()))
        offset.expand(0, valueForLength(style()->top(), containingBlock->availableHeight()));

    else if (!style()->bottom().isAuto()
        && (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight()
            || !style()->bottom().isPercent()
            || containingBlock->stretchesToViewport()))
        offset.expand(0, -valueForLength(style()->bottom(), containingBlock->availableHeight()));

    return offset;
}

LayoutPoint RenderBoxModelObject::adjustedPositionRelativeToOffsetParent(const LayoutPoint& startPoint) const
{
    // If the element is the HTML body element or doesn't have a parent
    // return 0 and stop this algorithm.
    if (isBody() || !parent())
        return LayoutPoint();

    LayoutPoint referencePoint = startPoint;
    referencePoint.move(parent()->columnOffset(referencePoint));

    // If the offsetParent of the element is null, or is the HTML body element,
    // return the distance between the canvas origin and the left border edge
    // of the element and stop this algorithm.
    Element* element = offsetParent();
    if (!element)
        return referencePoint;

    if (const RenderBoxModelObject* offsetParent = element->renderBoxModelObject()) {
        if (offsetParent->isBox() && !offsetParent->isBody())
            referencePoint.move(-toRenderBox(offsetParent)->borderLeft(), -toRenderBox(offsetParent)->borderTop());
        if (!isOutOfFlowPositioned() || flowThreadContainingBlock()) {
            if (isRelPositioned())
                referencePoint.move(relativePositionOffset());

            RenderObject* current;
            for (current = parent(); current != offsetParent && current->parent(); current = current->parent()) {
                // FIXME: What are we supposed to do inside SVG content?
                if (!isOutOfFlowPositioned()) {
                    if (current->isBox() && !current->isTableRow())
                        referencePoint.moveBy(toRenderBox(current)->topLeftLocation());
                    referencePoint.move(current->parent()->columnOffset(referencePoint));
                }
            }

            if (offsetParent->isBox() && offsetParent->isBody() && !offsetParent->isPositioned())
                referencePoint.moveBy(toRenderBox(offsetParent)->topLeftLocation());
        }
    }

    return referencePoint;
}

LayoutSize RenderBoxModelObject::offsetForInFlowPosition() const
{
    return isRelPositioned() ? relativePositionOffset() : LayoutSize();
}

LayoutUnit RenderBoxModelObject::offsetLeft() const
{
    // Note that RenderInline and RenderBox override this to pass a different
    // startPoint to adjustedPositionRelativeToOffsetParent.
    return adjustedPositionRelativeToOffsetParent(LayoutPoint()).x();
}

LayoutUnit RenderBoxModelObject::offsetTop() const
{
    // Note that RenderInline and RenderBox override this to pass a different
    // startPoint to adjustedPositionRelativeToOffsetParent.
    return adjustedPositionRelativeToOffsetParent(LayoutPoint()).y();
}

int RenderBoxModelObject::pixelSnappedOffsetWidth() const
{
    return snapSizeToPixel(offsetWidth(), offsetLeft());
}

int RenderBoxModelObject::pixelSnappedOffsetHeight() const
{
    return snapSizeToPixel(offsetHeight(), offsetTop());
}

LayoutUnit RenderBoxModelObject::computedCSSPadding(const Length& padding) const
{
    LayoutUnit w = 0;
    if (padding.isPercent())
        w = containingBlockLogicalWidthForContent();
    return minimumValueForLength(padding, w);
}

static inline int resolveWidthForRatio(int height, const FloatSize& intrinsicRatio)
{
    return ceilf(height * intrinsicRatio.width() / intrinsicRatio.height());
}

static inline int resolveHeightForRatio(int width, const FloatSize& intrinsicRatio)
{
    return ceilf(width * intrinsicRatio.height() / intrinsicRatio.width());
}

static inline IntSize resolveAgainstIntrinsicWidthOrHeightAndRatio(const IntSize& size, const FloatSize& intrinsicRatio, int useWidth, int useHeight)
{
    if (intrinsicRatio.isEmpty()) {
        if (useWidth)
            return IntSize(useWidth, size.height());
        return IntSize(size.width(), useHeight);
    }

    if (useWidth)
        return IntSize(useWidth, resolveHeightForRatio(useWidth, intrinsicRatio));
    return IntSize(resolveWidthForRatio(useHeight, intrinsicRatio), useHeight);
}

static inline IntSize resolveAgainstIntrinsicRatio(const IntSize& size, const FloatSize& intrinsicRatio)
{
    // Two possible solutions: (size.width(), solutionHeight) or (solutionWidth, size.height())
    // "... must be assumed to be the largest dimensions..." = easiest answer: the rect with the largest surface area.

    int solutionWidth = resolveWidthForRatio(size.height(), intrinsicRatio);
    int solutionHeight = resolveHeightForRatio(size.width(), intrinsicRatio);
    if (solutionWidth <= size.width()) {
        if (solutionHeight <= size.height()) {
            // If both solutions fit, choose the one covering the larger area.
            int areaOne = solutionWidth * size.height();
            int areaTwo = size.width() * solutionHeight;
            if (areaOne < areaTwo)
                return IntSize(size.width(), solutionHeight);
            return IntSize(solutionWidth, size.height());
        }

        // Only the first solution fits.
        return IntSize(solutionWidth, size.height());
    }

    // Only the second solution fits, assert that.
    ASSERT(solutionHeight <= size.height());
    return IntSize(size.width(), solutionHeight);
}

IntSize RenderBoxModelObject::calculateImageIntrinsicDimensions(StyleImage* image, const IntSize& positioningAreaSize, ScaleByEffectiveZoomOrNot shouldScaleOrNot) const
{
    // A generated image without a fixed size, will always return the container size as intrinsic size.
    if (image->isGeneratedImage() && image->usesImageContainerSize())
        return IntSize(positioningAreaSize.width(), positioningAreaSize.height());

    Length intrinsicWidth;
    Length intrinsicHeight;
    FloatSize intrinsicRatio;
    image->computeIntrinsicDimensions(this, intrinsicWidth, intrinsicHeight, intrinsicRatio);

    ASSERT(!intrinsicWidth.isPercent());
    ASSERT(!intrinsicHeight.isPercent());

    IntSize resolvedSize(intrinsicWidth.value(), intrinsicHeight.value());
    IntSize minimumSize(resolvedSize.width() > 0 ? 1 : 0, resolvedSize.height() > 0 ? 1 : 0);
    if (shouldScaleOrNot == ScaleByEffectiveZoom)
        resolvedSize.scale(style()->effectiveZoom());
    resolvedSize.clampToMinimumSize(minimumSize);

    if (!resolvedSize.isEmpty())
        return resolvedSize;

    // If the image has one of either an intrinsic width or an intrinsic height:
    // * and an intrinsic aspect ratio, then the missing dimension is calculated from the given dimension and the ratio.
    // * and no intrinsic aspect ratio, then the missing dimension is assumed to be the size of the rectangle that
    //   establishes the coordinate system for the 'background-position' property.
    if (resolvedSize.width() > 0 || resolvedSize.height() > 0)
        return resolveAgainstIntrinsicWidthOrHeightAndRatio(positioningAreaSize, intrinsicRatio, resolvedSize.width(), resolvedSize.height());

    // If the image has no intrinsic dimensions and has an intrinsic ratio the dimensions must be assumed to be the
    // largest dimensions at that ratio such that neither dimension exceeds the dimensions of the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    if (!intrinsicRatio.isEmpty())
        return resolveAgainstIntrinsicRatio(positioningAreaSize, intrinsicRatio);

    // If the image has no intrinsic ratio either, then the dimensions must be assumed to be the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    return positioningAreaSize;
}

bool RenderBoxModelObject::boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* inlineFlowBox) const
{
    if (bleedAvoidance != BackgroundBleedNone)
        return false;

    if (style()->hasAppearance())
        return false;

    const ShadowList* shadowList = style()->boxShadow();
    if (!shadowList)
        return false;

    bool hasOneNormalBoxShadow = false;
    size_t shadowCount = shadowList->shadows().size();
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& currentShadow = shadowList->shadows()[i];
        if (currentShadow.style() != Normal)
            continue;

        if (hasOneNormalBoxShadow)
            return false;
        hasOneNormalBoxShadow = true;

        if (currentShadow.spread())
            return false;
    }

    if (!hasOneNormalBoxShadow)
        return false;

    Color backgroundColor = resolveColor(CSSPropertyBackgroundColor);
    if (backgroundColor.hasAlpha())
        return false;

    const FillLayer* lastBackgroundLayer = &style()->backgroundLayers();
    for (const FillLayer* next = lastBackgroundLayer->next(); next; next = lastBackgroundLayer->next())
        lastBackgroundLayer = next;

    if (lastBackgroundLayer->clip() != BorderFillBox)
        return false;

    if (lastBackgroundLayer->image() && style()->hasBorderRadius())
        return false;

    if (inlineFlowBox && !inlineFlowBox->boxShadowCanBeAppliedToBackground(*lastBackgroundLayer))
        return false;

    if (hasOverflowClip() && lastBackgroundLayer->attachment() == LocalBackgroundAttachment)
        return false;

    return true;
}



LayoutUnit RenderBoxModelObject::containingBlockLogicalWidthForContent() const
{
    return containingBlock()->availableLogicalWidth();
}

RenderBoxModelObject* RenderBoxModelObject::continuation() const
{
    if (!continuationMap)
        return 0;
    return (*continuationMap)->get(this);
}

void RenderBoxModelObject::setContinuation(RenderBoxModelObject* continuation)
{
    if (continuation) {
        if (!continuationMap)
            continuationMap = new OwnPtrWillBePersistent<ContinuationMap>(adoptPtrWillBeNoop(new ContinuationMap));
        (*continuationMap)->set(this, continuation);
    } else {
        if (continuationMap)
            (*continuationMap)->remove(this);
    }
}

void RenderBoxModelObject::computeLayerHitTestRects(LayerHitTestRects& rects) const
{
    RenderLayerModelObject::computeLayerHitTestRects(rects);

    // If there is a continuation then we need to consult it here, since this is
    // the root of the tree walk and it wouldn't otherwise get picked up.
    // Continuations should always be siblings in the tree, so any others should
    // get picked up already by the tree walk.
    if (continuation())
        continuation()->computeLayerHitTestRects(rects);
}

RenderTextFragment* RenderBoxModelObject::firstLetterRemainingText() const
{
    if (!firstLetterRemainingTextMap)
        return 0;
    return (*firstLetterRemainingTextMap)->get(this);
}

void RenderBoxModelObject::setFirstLetterRemainingText(RenderTextFragment* remainingText)
{
    if (remainingText) {
        if (!firstLetterRemainingTextMap)
            firstLetterRemainingTextMap = new OwnPtrWillBePersistent<FirstLetterRemainingTextMap>(adoptPtrWillBeNoop(new FirstLetterRemainingTextMap));
        (*firstLetterRemainingTextMap)->set(this, remainingText);
    } else if (firstLetterRemainingTextMap) {
        (*firstLetterRemainingTextMap)->remove(this);
    }
}

LayoutRect RenderBoxModelObject::localCaretRectForEmptyElement(LayoutUnit width, LayoutUnit textIndentOffset)
{
    ASSERT(!slowFirstChild());

    // FIXME: This does not take into account either :first-line or :first-letter
    // However, as soon as some content is entered, the line boxes will be
    // constructed and this kludge is not called any more. So only the caret size
    // of an empty :first-line'd block is wrong. I think we can live with that.
    RenderStyle* currentStyle = firstLineStyle();

    enum CaretAlignment { alignLeft, alignRight, alignCenter };

    CaretAlignment alignment = alignLeft;

    switch (currentStyle->textAlign()) {
    case LEFT:
    case WEBKIT_LEFT:
        break;
    case CENTER:
    case WEBKIT_CENTER:
        alignment = alignCenter;
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        alignment = alignRight;
        break;
    case JUSTIFY:
    case TASTART:
        if (!currentStyle->isLeftToRightDirection())
            alignment = alignRight;
        break;
    case TAEND:
        if (currentStyle->isLeftToRightDirection())
            alignment = alignRight;
        break;
    }

    LayoutUnit x = borderLeft() + paddingLeft();
    LayoutUnit maxX = width - borderRight() - paddingRight();

    switch (alignment) {
    case alignLeft:
        if (currentStyle->isLeftToRightDirection())
            x += textIndentOffset;
        break;
    case alignCenter:
        x = (x + maxX) / 2;
        if (currentStyle->isLeftToRightDirection())
            x += textIndentOffset / 2;
        else
            x -= textIndentOffset / 2;
        break;
    case alignRight:
        x = maxX - caretWidth;
        if (!currentStyle->isLeftToRightDirection())
            x -= textIndentOffset;
        break;
    }
    x = std::min(x, std::max<LayoutUnit>(maxX - caretWidth, 0));

    LayoutUnit height = style()->fontMetrics().height();
    LayoutUnit verticalSpace = lineHeight(true, currentStyle->isHorizontalWritingMode() ? HorizontalLine : VerticalLine,  PositionOfInteriorLineBoxes) - height;
    LayoutUnit y = paddingTop() + borderTop() + (verticalSpace / 2);
    return currentStyle->isHorizontalWritingMode() ? LayoutRect(x, y, caretWidth, height) : LayoutRect(y, x, height, caretWidth);
}

void RenderBoxModelObject::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    RenderObject* o = container();
    if (!o)
        return;

    if (o->isRenderFlowThread())
        transformState.move(o->columnOffset(LayoutPoint(transformState.mappedPoint())));

    o->mapAbsoluteToLocalPoint(mode, transformState);

    LayoutSize containerOffset = offsetFromContainer(o, LayoutPoint());

    if (!style()->hasOutOfFlowPosition() && o->hasColumns()) {
        RenderBlock* block = toRenderBlock(o);
        LayoutPoint point(roundedLayoutPoint(transformState.mappedPoint()));
        point -= containerOffset;
        block->adjustForColumnRect(containerOffset, point);
    }

    bool preserve3D = mode & UseTransforms && (o->style()->preserves3D() || style()->preserves3D());
    if (mode & UseTransforms && shouldUseTransformFromContainer(o)) {
        TransformationMatrix t;
        getTransformFromContainer(o, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
}

const RenderObject* RenderBoxModelObject::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT(ancestorToStopAt != this);

    bool ancestorSkipped;
    RenderObject* container = this->container(ancestorToStopAt, &ancestorSkipped);
    if (!container)
        return 0;

    bool isInline = isRenderInline();
    bool isFixedPos = !isInline && style()->position() == FixedPosition;
    bool hasTransform = !isInline && hasLayer() && layer()->transform();

    LayoutSize adjustmentForSkippedAncestor;
    if (ancestorSkipped) {
        // There can't be a transform between paintInvalidationContainer and o, because transforms create containers, so it should be safe
        // to just subtract the delta between the ancestor and o.
        adjustmentForSkippedAncestor = -ancestorToStopAt->offsetFromAncestorContainer(container);
    }

    bool offsetDependsOnPoint = false;
    LayoutSize containerOffset = offsetFromContainer(container, LayoutPoint(), &offsetDependsOnPoint);

    bool preserve3D = container->style()->preserves3D() || style()->preserves3D();
    if (shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        t.translateRight(adjustmentForSkippedAncestor.width().toFloat(), adjustmentForSkippedAncestor.height().toFloat());
        geometryMap.push(this, t, preserve3D, offsetDependsOnPoint, isFixedPos, hasTransform);
    } else {
        containerOffset += adjustmentForSkippedAncestor;
        geometryMap.push(this, containerOffset, preserve3D, offsetDependsOnPoint, isFixedPos, hasTransform);
    }

    return ancestorSkipped ? ancestorToStopAt : container;
}

void RenderBoxModelObject::moveChildTo(RenderBoxModelObject* toBoxModelObject, RenderObject* child, RenderObject* beforeChild, bool fullRemoveInsert)
{
    // We assume that callers have cleared their positioned objects list for child moves (!fullRemoveInsert) so the
    // positioned renderer maps don't become stale. It would be too slow to do the map lookup on each call.
    ASSERT(!fullRemoveInsert || !isRenderBlock() || !toRenderBlock(this)->hasPositionedObjects());

    ASSERT(this == child->parent());
    ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());
    if (fullRemoveInsert && (toBoxModelObject->isRenderBlock() || toBoxModelObject->isRenderInline())) {
        // Takes care of adding the new child correctly if toBlock and fromBlock
        // have different kind of children (block vs inline).
        toBoxModelObject->addChild(virtualChildren()->removeChildNode(this, child), beforeChild);
    } else
        toBoxModelObject->virtualChildren()->insertChildNode(toBoxModelObject, virtualChildren()->removeChildNode(this, child, fullRemoveInsert), beforeChild, fullRemoveInsert);
}

void RenderBoxModelObject::moveChildrenTo(RenderBoxModelObject* toBoxModelObject, RenderObject* startChild, RenderObject* endChild, RenderObject* beforeChild, bool fullRemoveInsert)
{
    // This condition is rarely hit since this function is usually called on
    // anonymous blocks which can no longer carry positioned objects (see r120761)
    // or when fullRemoveInsert is false.
    if (fullRemoveInsert && isRenderBlock()) {
        RenderBlock* block = toRenderBlock(this);
        block->removePositionedObjects(0);
        if (block->isRenderBlockFlow())
            toRenderBlockFlow(block)->removeFloatingObjects();
    }

    ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());
    for (RenderObject* child = startChild; child && child != endChild; ) {
        // Save our next sibling as moveChildTo will clear it.
        RenderObject* nextSibling = child->nextSibling();
        moveChildTo(toBoxModelObject, child, beforeChild, fullRemoveInsert);
        child = nextSibling;
    }
}

} // namespace blink
