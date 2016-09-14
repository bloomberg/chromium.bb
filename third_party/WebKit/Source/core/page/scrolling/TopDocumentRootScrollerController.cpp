// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/TopDocumentRootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ChromeClient.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

// static
TopDocumentRootScrollerController* TopDocumentRootScrollerController::create(
    Document& document)
{
    return new TopDocumentRootScrollerController(document);
}

TopDocumentRootScrollerController::TopDocumentRootScrollerController(
    Document& document)
    : RootScrollerController(document)
{
}

DEFINE_TRACE(TopDocumentRootScrollerController)
{
    visitor->trace(m_viewportApplyScroll);
    visitor->trace(m_globalRootScroller);
    RootScrollerController::trace(visitor);
}

void TopDocumentRootScrollerController::globalRootScrollerMayHaveChanged()
{
    updateGlobalRootScroller();
}

Element* TopDocumentRootScrollerController::findGlobalRootScrollerElement()
{
    Element* element = effectiveRootScroller();

    while (element && element->isFrameOwnerElement()) {
        HTMLFrameOwnerElement* frameOwner = toHTMLFrameOwnerElement(element);
        DCHECK(frameOwner);

        Document* iframeDocument = frameOwner->contentDocument();
        if (!iframeDocument)
            return element;

        DCHECK(iframeDocument->rootScrollerController());
        element =
            iframeDocument->rootScrollerController()->effectiveRootScroller();
    }

    return element;
}

void TopDocumentRootScrollerController::updateGlobalRootScroller()
{
    Element* target = findGlobalRootScrollerElement();

    if (!m_viewportApplyScroll || !target)
        return;

    ScrollableArea* targetScroller =
        scrollableAreaFor(*target);

    if (!targetScroller)
        return;

    if (m_globalRootScroller)
        m_globalRootScroller->removeApplyScroll();

    // Use disable-native-scroll since the ViewportScrollCallback needs to
    // apply scroll actions both before (TopControls) and after (overscroll)
    // scrolling the element so it will apply scroll to the element itself.
    target->setApplyScroll(m_viewportApplyScroll, "disable-native-scroll");

    // A change in global root scroller requires a compositing inputs update to
    // the new and old global root scroller since it might change how the
    // ancestor layers are clipped. e.g. An iframe that's the global root
    // scroller clips its layers like the root frame.  Normally this is set
    // when the local effective root scroller changes but the global root
    // scroller can change because the parent's effective root scroller
    // changes.
    setNeedsCompositingInputsUpdateOnGlobalRootScroller();

    m_globalRootScroller = target;

    setNeedsCompositingInputsUpdateOnGlobalRootScroller();

    // Ideally, scroll customization would pass the current element to scroll to
    // the apply scroll callback but this doesn't happen today so we set it
    // through a back door here. This is also needed by the
    // ViewportScrollCallback to swap the target into the layout viewport
    // in RootFrameViewport.
    m_viewportApplyScroll->setScroller(targetScroller);
}

void TopDocumentRootScrollerController
    ::setNeedsCompositingInputsUpdateOnGlobalRootScroller()
{
    if (!m_globalRootScroller)
        return;

    PaintLayer* layer = m_globalRootScroller->document()
        .rootScrollerController()->rootScrollerPaintLayer();

    if (layer)
        layer->setNeedsCompositingInputsUpdate();

    if (LayoutView* view = m_globalRootScroller->document().layoutView()) {
        view->compositor()
            ->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
    }
}

void TopDocumentRootScrollerController::didUpdateCompositing()
{
    RootScrollerController::didUpdateCompositing();

    // Let the compositor-side counterpart know about this change.
    if (FrameHost* frameHost = m_document->frameHost())
        frameHost->chromeClient().registerViewportLayers();
}

void TopDocumentRootScrollerController::didAttachDocument()
{
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

    updateGlobalRootScroller();
}

bool TopDocumentRootScrollerController::isViewportScrollCallback(
    const ScrollStateCallback* callback) const
{
    if (!callback)
        return false;

    return callback == m_viewportApplyScroll.get();
}

GraphicsLayer* TopDocumentRootScrollerController::rootScrollerLayer()
{
    if (!m_globalRootScroller)
        return nullptr;

    ScrollableArea* area = scrollableAreaFor(*m_globalRootScroller);

    if (!area)
        return nullptr;

    GraphicsLayer* graphicsLayer = area->layerForScrolling();

    // TODO(bokan): We should assert graphicsLayer here and
    // RootScrollerController should do whatever needs to happen to ensure
    // the root scroller gets composited.

    return graphicsLayer;
}

Element* TopDocumentRootScrollerController::globalRootScroller() const
{
    return m_globalRootScroller.get();
}

} // namespace blink
