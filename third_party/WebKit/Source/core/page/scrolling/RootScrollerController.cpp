// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/TopControls.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class RootFrameViewport;

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
    visitor->trace(m_currentViewportApplyScrollHost);
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

void RootScrollerController::updateEffectiveRootScroller()
{
    bool rootScrollerValid =
        m_rootScroller && isValidRootScroller(*m_rootScroller);

    Element* newEffectiveRootScroller = rootScrollerValid
        ? m_rootScroller.get()
        : defaultEffectiveRootScroller();

    if (m_effectiveRootScroller == newEffectiveRootScroller)
        return;

    m_effectiveRootScroller = newEffectiveRootScroller;

    if (m_document->isInMainFrame())
        setViewportApplyScrollOnRootScroller();
}

void RootScrollerController::setViewportApplyScrollOnRootScroller()
{
    DCHECK(m_document->isInMainFrame());

    if (!m_viewportApplyScroll || !m_effectiveRootScroller)
        return;

    ScrollableArea* targetScroller =
        scrollableAreaFor(*m_effectiveRootScroller);

    if (!targetScroller)
        return;

    if (m_currentViewportApplyScrollHost)
        m_currentViewportApplyScrollHost->removeApplyScroll();

    // Use disable-native-scroll since the ViewportScrollCallback needs to
    // apply scroll actions both before (TopControls) and after (overscroll)
    // scrolling the element so it will apply scroll to the element itself.
    m_effectiveRootScroller->setApplyScroll(
        m_viewportApplyScroll, "disable-native-scroll");

    m_currentViewportApplyScrollHost = m_effectiveRootScroller;

    // Ideally, scroll customization would pass the current element to scroll to
    // the apply scroll callback but this doesn't happen today so we set it
    // through a back door here. This is also needed by the
    // ViewportScrollCallback to swap the target into the layout viewport
    // in RootFrameViewport.
    m_viewportApplyScroll->setScroller(targetScroller);
}

void RootScrollerController::didUpdateCompositing()
{
    FrameHost* frameHost = m_document->frameHost();

    // Let the compositor-side counterpart know about this change.
    if (frameHost && m_document->isInMainFrame())
        frameHost->chromeClient().registerViewportLayers();
}

void RootScrollerController::didAttachDocument()
{
    if (!m_document->isInMainFrame())
        return;

    FrameHost* frameHost = m_document->frameHost();
    FrameView* frameView = m_document->view();

    if (!frameHost || !frameView)
        return;

    RootFrameViewport* rootFrameViewport = frameView->getRootFrameViewport();
    DCHECK(rootFrameViewport);

    m_viewportApplyScroll = ViewportScrollCallback::create(
        &frameHost->topControls(),
        &frameHost->overscrollController(),
        *rootFrameViewport);

    updateEffectiveRootScroller();
}

GraphicsLayer* RootScrollerController::rootScrollerLayer()
{
    if (!m_effectiveRootScroller)
        return nullptr;

    ScrollableArea* area = scrollableAreaFor(*m_effectiveRootScroller);

    if (!area)
        return nullptr;

    GraphicsLayer* graphicsLayer = area->layerForScrolling();

    // TODO(bokan): We should assert graphicsLayer here and
    // RootScrollerController should do whatever needs to happen to ensure
    // the root scroller gets composited.

    return graphicsLayer;
}

bool RootScrollerController::isViewportScrollCallback(
    const ScrollStateCallback* callback) const
{
    if (!callback)
        return false;

    return callback == m_viewportApplyScroll.get();
}

Element* RootScrollerController::defaultEffectiveRootScroller()
{
    DCHECK(m_document);
    return m_document->documentElement();
}

} // namespace blink
