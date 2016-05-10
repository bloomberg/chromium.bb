// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutView.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/Histogram.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

ScrollAnchor::ScrollAnchor(ScrollableArea* scroller)
    : m_scroller(scroller)
    , m_anchorObject(nullptr)
    , m_corner(Corner::TopLeft)
{
    ASSERT(m_scroller);
    ASSERT(m_scroller->isFrameView() || m_scroller->isPaintLayerScrollableArea());
}

ScrollAnchor::~ScrollAnchor()
{
}

// TODO(pilgrim) replace all instances of scrollerLayoutBox with scrollerLayoutBoxItem
// https://crbug.com/499321
static LayoutBox* scrollerLayoutBox(const ScrollableArea* scroller)
{
    LayoutBox* box = scroller->isFrameView()
        ? toFrameView(scroller)->layoutView()
        : &toPaintLayerScrollableArea(scroller)->box();
    ASSERT(box);
    return box;
}

static LayoutBoxItem scrollerLayoutBoxItem(const ScrollableArea* scroller)
{
    LayoutBoxItem box = scroller->isFrameView()
        ? toFrameView(scroller)->layoutViewItem()
        : LayoutBoxItem(&toPaintLayerScrollableArea(scroller)->box());
    ASSERT(!box.isNull());
    return box;
}

static Corner cornerFromCandidateRect(LayoutRect candidateRect, LayoutRect visibleRect)
{
    if (visibleRect.contains(candidateRect.minXMinYCorner()))
        return Corner::TopLeft;
    if (visibleRect.contains(candidateRect.maxXMinYCorner()))
        return Corner::TopRight;
    if (visibleRect.contains(candidateRect.minXMaxYCorner()))
        return Corner::BottomLeft;
    if (visibleRect.contains(candidateRect.maxXMaxYCorner()))
        return Corner::BottomRight;
    return Corner::TopLeft;
}

static LayoutPoint cornerPointOfRect(LayoutRect rect, Corner whichCorner)
{
    switch (whichCorner) {
    case Corner::TopLeft: return rect.minXMinYCorner();
    case Corner::TopRight: return rect.maxXMinYCorner();
    case Corner::BottomLeft: return rect.minXMaxYCorner();
    case Corner::BottomRight: return rect.maxXMaxYCorner();
    }
    ASSERT_NOT_REACHED();
    return LayoutPoint();
}

// Bounds of the LayoutObject relative to the scroller's visible content rect.
static LayoutRect relativeBounds(const LayoutObject* layoutObject, const ScrollableArea* scroller)
{
    LayoutRect localBounds;
    if (layoutObject->isBox()) {
        localBounds = toLayoutBox(layoutObject)->borderBoxRect();
    } else if (layoutObject->isText()) {
        // TODO(skobes): Use first and last InlineTextBox only?
        for (InlineTextBox* box = toLayoutText(layoutObject)->firstTextBox(); box; box = box->nextTextBox())
            localBounds.unite(box->calculateBoundaries());
    } else {
        // Only LayoutBox and LayoutText are supported.
        ASSERT_NOT_REACHED();
    }
    LayoutRect relativeBounds = LayoutRect(layoutObject->localToAncestorQuad(
        FloatRect(localBounds), scrollerLayoutBox(scroller)).boundingBox());
    // When the scroller is the FrameView, localToAncestorQuad returns document coords,
    // so we must subtract scroll offset to get viewport coords. We discard the fractional
    // part of the scroll offset so that the rounding in restore() matches the snapping of
    // the anchor node to the pixel grid of the layer it paints into. For non-FrameView
    // scrollers, we rely on the flooring behavior of LayoutBox::scrolledContentOffset.
    if (scroller->isFrameView())
        relativeBounds.moveBy(-flooredIntPoint(scroller->scrollPositionDouble()));
    return relativeBounds;
}

static LayoutPoint computeRelativeOffset(const LayoutObject* layoutObject, const ScrollableArea* scroller, Corner corner)
{
    return cornerPointOfRect(relativeBounds(layoutObject, scroller), corner);
}

static bool candidateMovesWithScroller(const LayoutObject* candidate, const ScrollableArea* scroller)
{
    if (candidate->style() && candidate->style()->hasViewportConstrainedPosition())
        return false;

    bool skippedByContainerLookup = false;
    candidate->container(scrollerLayoutBox(scroller), &skippedByContainerLookup);
    return !skippedByContainerLookup;
}

ScrollAnchor::ExamineResult ScrollAnchor::examine(const LayoutObject* candidate) const
{
    if (candidate->isLayoutInline())
        return ExamineResult(Continue);

    if (!candidate->isText() && !candidate->isBox())
        return ExamineResult(Skip);

    if (!candidateMovesWithScroller(candidate, m_scroller))
        return ExamineResult(Skip);

    LayoutRect candidateRect = relativeBounds(candidate, m_scroller);
    LayoutRect visibleRect = scrollerLayoutBoxItem(m_scroller).overflowClipRect(LayoutPoint());

    bool occupiesSpace = candidateRect.width() > 0 && candidateRect.height() > 0;
    if (occupiesSpace && visibleRect.intersects(candidateRect)) {
        return ExamineResult(
            visibleRect.contains(candidateRect) ? Return : Constrain,
            cornerFromCandidateRect(candidateRect, visibleRect));
    } else {
        return ExamineResult(Skip);
    }
}

void ScrollAnchor::findAnchor()
{
    LayoutObject* stayWithin = scrollerLayoutBox(m_scroller);
    LayoutObject* candidate = stayWithin->nextInPreOrder(stayWithin);
    while (candidate) {
        ExamineResult result = examine(candidate);
        if (result.viable) {
            m_anchorObject = candidate;
            m_corner = result.corner;
        }
        switch (result.status) {
        case Skip:
            candidate = candidate->nextInPreOrderAfterChildren(stayWithin);
            break;
        case Constrain:
            stayWithin = candidate;
            // fall through
        case Continue:
            candidate = candidate->nextInPreOrder(stayWithin);
            break;
        case Return:
            return;
        }
    }
}

void ScrollAnchor::save()
{
    if (m_scroller->scrollPosition() == IntPoint::zero()) {
        clear();
        return;
    }
    if (m_anchorObject)
        return;

    findAnchor();
    if (!m_anchorObject)
        return;

    m_anchorObject->setIsScrollAnchorObject();
    m_savedRelativeOffset = computeRelativeOffset(m_anchorObject, m_scroller, m_corner);
}

void ScrollAnchor::restore()
{
    if (!m_anchorObject)
        return;

    // The anchor node can report fractional positions, but it is DIP-snapped when
    // painting (crbug.com/610805), so we must round the offsets to determine the
    // visual delta. If we scroll by the delta in LayoutUnits, the snapping of the
    // anchor node may round differently from the snapping of the scroll position.
    // (For example, anchor moving from 2.4px -> 2.6px is really 2px -> 3px, so we
    // should scroll by 1px instead of 0.2px.) This is true regardless of whether
    // the ScrollableArea actually uses fractional scroll positions.
    IntSize adjustment =
        roundedIntSize(computeRelativeOffset(m_anchorObject, m_scroller, m_corner)) -
        roundedIntSize(m_savedRelativeOffset);
    if (!adjustment.isZero()) {
        DoublePoint desiredPos = m_scroller->scrollPositionDouble() + adjustment;
        ScrollAnimatorBase* animator = m_scroller->existingScrollAnimator();
        if (!animator || !animator->hasRunningAnimation()) {
            m_scroller->setScrollPosition(desiredPos, AnchoringScroll);
        } else {
            // If in the middle of a scroll animation, stop the animation, make
            // the adjustment, and continue the animation on the pending delta.
            FloatSize pendingDelta = animator->desiredTargetPosition() - FloatPoint(m_scroller->scrollPositionDouble());
            animator->cancelAnimation();
            m_scroller->setScrollPosition(desiredPos, AnchoringScroll);
            animator->userScroll(ScrollByPixel, pendingDelta);
        }
        // Update UMA metric.
        DEFINE_STATIC_LOCAL(EnumerationHistogram, adjustedOffsetHistogram,
            ("Layout.ScrollAnchor.AdjustedScrollOffset", 2));
        adjustedOffsetHistogram.count(1);
        UseCounter::count(scrollerLayoutBox(m_scroller)->document(), UseCounter::ScrollAnchored);
    }
}

void ScrollAnchor::clear()
{
    LayoutObject* anchorObject = m_anchorObject;
    m_anchorObject = nullptr;

    if (anchorObject)
        anchorObject->maybeClearIsScrollAnchorObject();
}

} // namespace blink
