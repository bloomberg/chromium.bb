// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/TopControls.h"
#include "core/layout/LayoutBox.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

namespace {

ScrollableArea* scrollableAreaFor(const Element& element)
{
    if (!element.layoutObject() || !element.layoutObject()->isBox())
        return nullptr;

    LayoutBox* box = toLayoutBox(element.layoutObject());

    // For a FrameView, we use the layoutViewport rather than the
    // getScrollableArea() since that could be the RootFrameViewport. The
    // rootScroller's ScrollableArea will be swapped in as the layout viewport
    // in RootFrameViewport so we need to ensure we get the layout viewport.
    if (box->isDocumentElement())
        return element.document().view()->layoutViewportScrollableArea();

    return static_cast<PaintInvalidationCapableScrollableArea*>(
        box->getScrollableArea());
}

bool fillsViewport(const Element& element)
{
    DCHECK(element.layoutObject());
    DCHECK(element.layoutObject()->isBox());

    LayoutObject* layoutObject = element.layoutObject();

    // TODO(bokan): Broken for OOPIF.
    Document& topDocument = element.document().topDocument();

    Vector<FloatQuad> quads;
    layoutObject->absoluteQuads(quads);
    DCHECK_EQ(quads.size(), 1u);

    if (!quads[0].isRectilinear())
        return false;

    LayoutRect boundingBox(quads[0].boundingBox());

    return boundingBox.location() == LayoutPoint::zero()
        && boundingBox.size() == topDocument.layoutViewItem().size();
}

bool isValidRootScroller(const Element& element)
{
    if (!element.layoutObject())
        return false;

    if (!scrollableAreaFor(element))
        return false;

    if (!fillsViewport(element))
        return false;

    return true;
}

} // namespace

RootScrollerController::RootScrollerController(Document& document)
    : m_document(&document)
{
}

DEFINE_TRACE(RootScrollerController)
{
    visitor->trace(m_document);
    visitor->trace(m_viewportApplyScroll);
    visitor->trace(m_rootScroller);
    visitor->trace(m_effectiveRootScroller);
}

void RootScrollerController::set(Element* newRootScroller)
{
    m_rootScroller = newRootScroller;
    updateEffectiveRootScroller();
}

Element* RootScrollerController::get() const
{
    return m_rootScroller;
}

Element* RootScrollerController::effectiveRootScroller() const
{
    return m_effectiveRootScroller;
}

void RootScrollerController::didUpdateLayout()
{
    updateEffectiveRootScroller();
}

void RootScrollerController::setViewportScrollCallback(ViewportScrollCallback* callback)
{
    m_viewportApplyScroll = callback;
    moveViewportApplyScroll(m_effectiveRootScroller);
}

void RootScrollerController::updateEffectiveRootScroller()
{
    bool rootScrollerValid =
        m_rootScroller && isValidRootScroller(*m_rootScroller);

    Element* newEffectiveRootScroller = rootScrollerValid
        ? m_rootScroller.get()
        : defaultEffectiveRootScroller();

    if (m_effectiveRootScroller == newEffectiveRootScroller)
        return;

    if (moveViewportApplyScroll(newEffectiveRootScroller))
        m_effectiveRootScroller = newEffectiveRootScroller;
}

bool RootScrollerController::moveViewportApplyScroll(Element* target)
{
    if (!m_viewportApplyScroll || !target)
        return false;

    ScrollableArea* targetScroller = scrollableAreaFor(*target);
    if (!targetScroller)
        return false;

    if (m_effectiveRootScroller)
        m_effectiveRootScroller->removeApplyScroll();

    // Use disable-native-scroll since the ViewportScrollCallback needs to
    // apply scroll actions both before (TopControls) and after (overscroll)
    // scrolling the element so it will apply scroll to the element itself.
    target->setApplyScroll(m_viewportApplyScroll, "disable-native-scroll");

    // Ideally, scroll customization would pass the current element to scroll to
    // the apply scroll callback but this doesn't happen today so we set it
    // through a back door here. This is also needed by the
    // RootViewportScrollCallback to swap the target into the layout viewport
    // in RootFrameViewport.
    m_viewportApplyScroll->setScroller(targetScroller);

    return true;
}

Element* RootScrollerController::defaultEffectiveRootScroller()
{
    DCHECK(m_document);
    return m_document->documentElement();
}

} // namespace blink
