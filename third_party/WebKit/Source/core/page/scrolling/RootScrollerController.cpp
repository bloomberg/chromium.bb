// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class RootFrameViewport;

namespace {

bool fillsViewport(const Element& element) {
  DCHECK(element.layoutObject());
  DCHECK(element.layoutObject()->isBox());

  LayoutObject* layoutObject = element.layoutObject();

  // TODO(bokan): Broken for OOPIF. crbug.com/642378.
  Document& topDocument = element.document().topDocument();

  Vector<FloatQuad> quads;
  layoutObject->absoluteQuads(quads);
  DCHECK_EQ(quads.size(), 1u);

  if (!quads[0].isRectilinear())
    return false;

  LayoutRect boundingBox(quads[0].boundingBox());

  return boundingBox.location() == LayoutPoint::zero() &&
         boundingBox.size() == topDocument.layoutViewItem().size();
}

}  // namespace

// static
RootScrollerController* RootScrollerController::create(Document& document) {
  return new RootScrollerController(document);
}

RootScrollerController::RootScrollerController(Document& document)
    : m_document(&document),
      m_effectiveRootScroller(&document),
      m_documentHasDocumentElement(false) {}

DEFINE_TRACE(RootScrollerController) {
  visitor->trace(m_document);
  visitor->trace(m_rootScroller);
  visitor->trace(m_effectiveRootScroller);
}

void RootScrollerController::set(Element* newRootScroller) {
  m_rootScroller = newRootScroller;
  recomputeEffectiveRootScroller();
}

Element* RootScrollerController::get() const {
  return m_rootScroller;
}

Node& RootScrollerController::effectiveRootScroller() const {
  DCHECK(m_effectiveRootScroller);
  return *m_effectiveRootScroller;
}

void RootScrollerController::didUpdateLayout() {
  recomputeEffectiveRootScroller();
}

void RootScrollerController::recomputeEffectiveRootScroller() {
  bool rootScrollerValid =
      m_rootScroller && isValidRootScroller(*m_rootScroller);

  Node* newEffectiveRootScroller = m_document;
  if (rootScrollerValid)
    newEffectiveRootScroller = m_rootScroller;

  // TODO(bokan): This is a terrible hack but required because the viewport
  // apply scroll works on Elements rather than Nodes. If we're going from
  // !documentElement to documentElement, we can't early out even if the root
  // scroller didn't change since the global root scroller didn't have an
  // Element previously to put it's ViewportScrollCallback onto. We need this
  // to kick the global root scroller to recompute itself. We can remove this
  // if ScrollCustomization is moved to the Node rather than Element.
  bool oldHasDocumentElement = m_documentHasDocumentElement;
  m_documentHasDocumentElement = m_document->documentElement();

  if (oldHasDocumentElement || !m_documentHasDocumentElement) {
    if (m_effectiveRootScroller == newEffectiveRootScroller)
      return;
  }

  PaintLayer* oldRootScrollerLayer = rootScrollerPaintLayer();

  m_effectiveRootScroller = newEffectiveRootScroller;

  // This change affects both the old and new layers.
  if (oldRootScrollerLayer)
    oldRootScrollerLayer->setNeedsCompositingInputsUpdate();
  if (rootScrollerPaintLayer())
    rootScrollerPaintLayer()->setNeedsCompositingInputsUpdate();

  // The above may not be enough as we need to update existing ancestor
  // GraphicsLayers. This will force us to rebuild the GraphicsLayer tree.
  if (LayoutView* layoutView = m_document->layoutView()) {
    layoutView->compositor()->setNeedsCompositingUpdate(
        CompositingUpdateRebuildTree);
  }

  if (FrameHost* frameHost = m_document->frameHost())
    frameHost->globalRootScrollerController().didChangeRootScroller();
}

bool RootScrollerController::isValidRootScroller(const Element& element) const {
  if (!element.layoutObject())
    return false;

  if (!RootScrollerUtil::scrollableAreaForRootScroller(&element))
    return false;

  if (!fillsViewport(element))
    return false;

  return true;
}

PaintLayer* RootScrollerController::rootScrollerPaintLayer() const {
  return RootScrollerUtil::paintLayerForRootScroller(m_effectiveRootScroller);
}

bool RootScrollerController::scrollsViewport(const Element& element) const {
  if (m_effectiveRootScroller->isDocumentNode())
    return element.isSameNode(m_document->documentElement());

  return element.isSameNode(m_effectiveRootScroller);
}

}  // namespace blink
