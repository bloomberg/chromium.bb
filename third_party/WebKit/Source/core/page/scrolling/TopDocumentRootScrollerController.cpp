// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/TopDocumentRootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

// static
TopDocumentRootScrollerController* TopDocumentRootScrollerController::create(
    Page& page) {
  return new TopDocumentRootScrollerController(page);
}

TopDocumentRootScrollerController::TopDocumentRootScrollerController(Page& page)
    : m_page(&page) {}

DEFINE_TRACE(TopDocumentRootScrollerController) {
  visitor->trace(m_viewportApplyScroll);
  visitor->trace(m_globalRootScroller);
  visitor->trace(m_page);
}

void TopDocumentRootScrollerController::didChangeRootScroller() {
  recomputeGlobalRootScroller();
}

ScrollableArea* TopDocumentRootScrollerController::rootScrollerArea() const {
  return RootScrollerUtil::scrollableAreaForRootScroller(globalRootScroller());
}

IntSize TopDocumentRootScrollerController::rootScrollerVisibleArea() const {
  if (!topDocument() || !topDocument()->view())
    return IntSize();

  float minimumPageScale =
      m_page->pageScaleConstraintsSet().finalConstraints().minimumScale;
  int browserControlsAdjustment = ceilf(
      m_page->visualViewport().browserControlsAdjustment() / minimumPageScale);

  return topDocument()->view()->visibleContentSize(ExcludeScrollbars) +
         IntSize(0, browserControlsAdjustment);
}

Element* TopDocumentRootScrollerController::findGlobalRootScrollerElement() {
  if (!topDocument())
    return nullptr;

  Node* effectiveRootScroller =
      &topDocument()->rootScrollerController().effectiveRootScroller();

  if (effectiveRootScroller->isDocumentNode())
    return topDocument()->documentElement();

  DCHECK(effectiveRootScroller->isElementNode());
  Element* element = toElement(effectiveRootScroller);

  while (element && element->isFrameOwnerElement()) {
    HTMLFrameOwnerElement* frameOwner = toHTMLFrameOwnerElement(element);
    DCHECK(frameOwner);

    Document* iframeDocument = frameOwner->contentDocument();
    if (!iframeDocument)
      return element;

    effectiveRootScroller =
        &iframeDocument->rootScrollerController().effectiveRootScroller();
    if (effectiveRootScroller->isDocumentNode())
      return iframeDocument->documentElement();

    element = toElement(effectiveRootScroller);
  }

  return element;
}

void TopDocumentRootScrollerController::recomputeGlobalRootScroller() {
  if (!m_viewportApplyScroll)
    return;

  Element* target = findGlobalRootScrollerElement();
  if (target == m_globalRootScroller)
    return;

  ScrollableArea* targetScroller =
      RootScrollerUtil::scrollableAreaForRootScroller(target);

  if (!targetScroller)
    return;

  if (m_globalRootScroller)
    m_globalRootScroller->removeApplyScroll();

  // Use disable-native-scroll since the ViewportScrollCallback needs to
  // apply scroll actions both before (BrowserControls) and after (overscroll)
  // scrolling the element so it will apply scroll to the element itself.
  target->setApplyScroll(m_viewportApplyScroll, "disable-native-scroll");

  ScrollableArea* oldRootScrollerArea =
      RootScrollerUtil::scrollableAreaForRootScroller(m_globalRootScroller);

  m_globalRootScroller = target;

  // Ideally, scroll customization would pass the current element to scroll to
  // the apply scroll callback but this doesn't happen today so we set it
  // through a back door here. This is also needed by the
  // ViewportScrollCallback to swap the target into the layout viewport
  // in RootFrameViewport.
  m_viewportApplyScroll->setScroller(targetScroller);

  // The scrollers may need to stop using their own scrollbars as Android
  // Chrome's VisualViewport provides the scrollbars for the root scroller.
  if (oldRootScrollerArea)
    oldRootScrollerArea->didChangeGlobalRootScroller();

  targetScroller->didChangeGlobalRootScroller();
}

Document* TopDocumentRootScrollerController::topDocument() const {
  if (!m_page || !m_page->mainFrame() || !m_page->mainFrame()->isLocalFrame())
    return nullptr;

  return toLocalFrame(m_page->mainFrame())->document();
}

void TopDocumentRootScrollerController::didUpdateCompositing() {
  if (!m_page)
    return;

  // Let the compositor-side counterpart know about this change.
  m_page->chromeClient().registerViewportLayers();
}

void TopDocumentRootScrollerController::didDisposeScrollableArea(
    ScrollableArea& area) {
  if (!topDocument() || !topDocument()->view())
    return;

  // If the document is tearing down, we may no longer have a layoutViewport to
  // fallback to.
  if (topDocument()->lifecycle().state() >= DocumentLifecycle::Stopping)
    return;

  FrameView* frameView = topDocument()->view();

  RootFrameViewport* rfv = frameView->getRootFrameViewport();

  if (&area == &rfv->layoutViewport()) {
    DCHECK(frameView->layoutViewportScrollableArea());
    rfv->setLayoutViewport(*frameView->layoutViewportScrollableArea());
  }
}

void TopDocumentRootScrollerController::initializeViewportScrollCallback(
    RootFrameViewport& rootFrameViewport) {
  DCHECK(m_page);
  m_viewportApplyScroll = ViewportScrollCallback::create(
      &m_page->browserControls(), &m_page->overscrollController(),
      rootFrameViewport);

  recomputeGlobalRootScroller();
}

bool TopDocumentRootScrollerController::isViewportScrollCallback(
    const ScrollStateCallback* callback) const {
  if (!callback)
    return false;

  return callback == m_viewportApplyScroll.get();
}

GraphicsLayer* TopDocumentRootScrollerController::rootScrollerLayer() const {
  ScrollableArea* area =
      RootScrollerUtil::scrollableAreaForRootScroller(m_globalRootScroller);

  if (!area)
    return nullptr;

  GraphicsLayer* graphicsLayer = area->layerForScrolling();

  // TODO(bokan): We should assert graphicsLayer here and
  // RootScrollerController should do whatever needs to happen to ensure
  // the root scroller gets composited.

  return graphicsLayer;
}

PaintLayer* TopDocumentRootScrollerController::rootScrollerPaintLayer() const {
  return RootScrollerUtil::paintLayerForRootScroller(m_globalRootScroller);
}

Element* TopDocumentRootScrollerController::globalRootScroller() const {
  return m_globalRootScroller.get();
}

}  // namespace blink
