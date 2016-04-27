// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScroller.h"

#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

namespace {

Document* topDocument(FrameHost* frameHost)
{
    DCHECK(frameHost);
    if (!frameHost->page().mainFrame())
        return nullptr;

    DCHECK(frameHost->page().mainFrame()->isLocalFrame());
    return toLocalFrame(frameHost->page().mainFrame())->document();
}

ScrollableArea* scrollableAreaFor(Element& element)
{
    if (!element.layoutObject() || !element.layoutObject()->isBox())
        return nullptr;

    LayoutBox* box = toLayoutBox(element.layoutObject());

    if (box->isDocumentElement() && element.document().view())
        return element.document().view()->getScrollableArea();

    return static_cast<PaintInvalidationCapableScrollableArea*>(
        box->getScrollableArea());
}

} // namespace

RootScroller::RootScroller(FrameHost& frameHost)
    : m_frameHost(&frameHost)
{
}

DEFINE_TRACE(RootScroller)
{
    visitor->trace(m_frameHost);
    visitor->trace(m_viewportApplyScroll);
    visitor->trace(m_rootScroller);
}

bool RootScroller::set(Element& newRootScroller)
{
    if (!isValid(newRootScroller))
        return false;

    DCHECK(m_frameHost);

    Document* document = topDocument(m_frameHost);
    if (!document)
        return false;

    ScrollableArea* newRootScrollableArea = scrollableAreaFor(newRootScroller);
    if (!newRootScrollableArea)
        return false;

    if (m_rootScroller)
        m_rootScroller->removeApplyScroll();

    m_rootScroller = &newRootScroller;

    createApplyScrollIfNeeded();

    // Ideally, the scrolling infrastructure would pass this to the callback but
    // we don't get that today so we set it manually.
    m_viewportApplyScroll->setScroller(*newRootScrollableArea);

    // Installs the viewport scrolling callback (the "applyScroll" in Scroll
    // Customization lingo) on the given element. This callback is
    // responsible for viewport related scroll actions like top controls
    // movement and overscroll glow as well as actually scrolling the element.
    // Use disable-native-scroll since the ViewportScrollCallback needs to
    // apply scroll actions before (TopControls) and after (overscroll)
    // scrolling the element so it applies scroll to the element itself.
    m_rootScroller->setApplyScroll(
        m_viewportApplyScroll,
        "disable-native-scroll");

    return true;
}

Element* RootScroller::get() const
{
    if (!m_frameHost)
        return nullptr;

    return m_rootScroller;
}

void RootScroller::resetToDefault()
{
    if (!m_frameHost)
        return;

    Document* document = topDocument(m_frameHost);
    if (!document)
        return;

    if (Element* defaultRootElement = document->documentElement())
        set(*defaultRootElement);
}

void RootScroller::didUpdateTopDocumentLayout()
{
    if (m_rootScroller && isValid(*m_rootScroller))
        return;

    resetToDefault();
}

bool RootScroller::isValid(Element& element) const
{
    if (!m_frameHost)
        return false;

    if (element.document() != topDocument(m_frameHost))
        return false;

    if (!element.isInTreeScope())
        return false;

    if (!element.layoutObject())
        return false;

    if (!element.layoutObject()->isLayoutBlockFlow()
        && !element.layoutObject()->isLayoutIFrame())
        return false;

    return true;
}

void RootScroller::createApplyScrollIfNeeded()
{
    if (!m_viewportApplyScroll) {
        TopControls& topControls = m_frameHost->topControls();
        OverscrollController& overscrollController =
            m_frameHost->overscrollController();
        m_viewportApplyScroll =
            new ViewportScrollCallback(topControls, overscrollController);
    }
}

} // namespace blink
