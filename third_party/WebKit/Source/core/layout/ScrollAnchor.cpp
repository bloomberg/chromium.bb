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

static LayoutBox* scrollerLayoutBox(const ScrollableArea* scroller)
{
    LayoutBox* box = scroller->isFrameView()
        ? toFrameView(scroller)->layoutView()
        : &toPaintLayerScrollableArea(scroller)->box();
    ASSERT(box);
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
    if (scroller->isFrameView())
        relativeBounds.moveBy(-LayoutPoint(scroller->scrollPositionDouble()));
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
    LayoutRect visibleRect = scrollerLayoutBox(m_scroller)->overflowClipRect(LayoutPoint());

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

    LayoutSize adjustment = computeRelativeOffset(m_anchorObject, m_scroller, m_corner) - m_savedRelativeOffset;
    if (!adjustment.isZero()) {
        ScrollAnimatorBase* animator = m_scroller->existingScrollAnimator();
        if (!animator || !animator->hasRunningAnimation()) {
            m_scroller->setScrollPosition(
                m_scroller->scrollPositionDouble() + DoubleSize(adjustment),
                AnchoringScroll);
        } else {
            // If in the middle of a scroll animation, stop the animation, make
            // the adjustment, and continue the animation on the pending delta.
            FloatSize pendingDelta = animator->desiredTargetPosition() - FloatPoint(m_scroller->scrollPositionDouble());
            animator->cancelAnimation();
            m_scroller->setScrollPosition(
                m_scroller->scrollPositionDouble() + DoubleSize(adjustment),
                AnchoringScroll);
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
