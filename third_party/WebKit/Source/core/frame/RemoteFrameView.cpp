// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteFrameView.h"

#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutPart.h"

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remoteFrame)
    : m_remoteFrame(remoteFrame)
{
    ASSERT(remoteFrame);
}

RemoteFrameView::~RemoteFrameView()
{
}

PassRefPtrWillBeRawPtr<RemoteFrameView> RemoteFrameView::create(RemoteFrame* remoteFrame)
{
    RefPtrWillBeRawPtr<RemoteFrameView> view = adoptRefWillBeNoop(new RemoteFrameView(remoteFrame));
    view->show();
    return view.release();
}

void RemoteFrameView::dispose()
{
    HTMLFrameOwnerElement* ownerElement = m_remoteFrame->deprecatedLocalOwner();
    // ownerElement can be null during frame swaps, because the
    // RemoteFrameView is disconnected before detachment.
    if (ownerElement && ownerElement->ownedWidget() == this)
        ownerElement->setWidget(nullptr);
    Widget::dispose();
}

void RemoteFrameView::invalidateRect(const IntRect& rect)
{
    LayoutPart* layoutObject = m_remoteFrame->ownerLayoutObject();
    if (!layoutObject)
        return;

    LayoutRect repaintRect(rect);
    repaintRect.move(layoutObject->borderLeft() + layoutObject->paddingLeft(),
        layoutObject->borderTop() + layoutObject->paddingTop());
    layoutObject->invalidatePaintRectangle(repaintRect);
}

void RemoteFrameView::setFrameRect(const IntRect& newRect)
{
    IntRect oldRect = frameRect();

    if (newRect == oldRect)
        return;

    Widget::setFrameRect(newRect);

    frameRectsChanged();
}

void RemoteFrameView::frameRectsChanged()
{
    m_remoteFrame->frameRectsChanged(frameRect());
}

void RemoteFrameView::hide()
{
    setSelfVisible(false);

    Widget::hide();

    m_remoteFrame->visibilityChanged(false);
}

void RemoteFrameView::show()
{
    setSelfVisible(true);

    Widget::show();

    m_remoteFrame->visibilityChanged(true);
}

void RemoteFrameView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);
    if (!isSelfVisible())
        return;

    m_remoteFrame->visibilityChanged(isVisible());
}

DEFINE_TRACE(RemoteFrameView)
{
    visitor->trace(m_remoteFrame);
    Widget::trace(visitor);
}

} // namespace blink
