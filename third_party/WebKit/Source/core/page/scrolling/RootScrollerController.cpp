// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class RootFrameViewport;

namespace {

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

} // namespace

// static
RootScrollerController* RootScrollerController::create(Document& document)
{
    return new RootScrollerController(document);
}

RootScrollerController::RootScrollerController(Document& document)
    : m_document(&document)
{
}

DEFINE_TRACE(RootScrollerController)
{
    visitor->trace(m_document);
    visitor->trace(m_rootScroller);
    visitor->trace(m_effectiveRootScroller);
}

void RootScrollerController::set(Element* newRootScroller)
{
    m_rootScroller = newRootScroller;
    recomputeEffectiveRootScroller();
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
    recomputeEffectiveRootScroller();
}

void RootScrollerController::globalRootScrollerMayHaveChanged()
{
}

void RootScrollerController::recomputeEffectiveRootScroller()
{
    bool rootScrollerValid =
        m_rootScroller && isValidRootScroller(*m_rootScroller);

    Element* newEffectiveRootScroller = rootScrollerValid
        ? m_rootScroller.get()
        : defaultEffectiveRootScroller();

    if (m_effectiveRootScroller == newEffectiveRootScroller)
        return;

    m_effectiveRootScroller = newEffectiveRootScroller;

    m_document->topDocument().rootScrollerController()
        ->globalRootScrollerMayHaveChanged();
}

ScrollableArea* RootScrollerController::scrollableAreaFor(
    const Element& element) const
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

bool RootScrollerController::isValidRootScroller(const Element& element) const
{
    if (!element.layoutObject())
        return false;

    if (!scrollableAreaFor(element))
        return false;

    if (!fillsViewport(element))
        return false;

    return true;
}

void RootScrollerController::didUpdateCompositing()
{
}

void RootScrollerController::didAttachDocument()
{
}

GraphicsLayer* RootScrollerController::rootScrollerLayer()
{
    NOTREACHED();
    return nullptr;
}

bool RootScrollerController::isViewportScrollCallback(
    const ScrollStateCallback* callback) const
{
    // TopDocumentRootScrollerController must override this method to actually
    // do the comparison.
    DCHECK(!m_document->isInMainFrame());

    RootScrollerController* topDocumentController =
        m_document->topDocument().rootScrollerController();
    return topDocumentController->isViewportScrollCallback(callback);
}

Element* RootScrollerController::defaultEffectiveRootScroller()
{
    DCHECK(m_document);
    return m_document->documentElement();
}

} // namespace blink
