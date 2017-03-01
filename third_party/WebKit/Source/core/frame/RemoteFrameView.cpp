// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteFrameView.h"

#include "core/dom/IntersectionObserverEntry.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameClient.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutPartItem.h"

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remoteFrame)
    : m_remoteFrame(remoteFrame) {
  ASSERT(remoteFrame);
}

RemoteFrameView::~RemoteFrameView() {}

void RemoteFrameView::setParent(FrameViewBase* parent) {
  FrameViewBase::setParent(parent);
  frameRectsChanged();
}

RemoteFrameView* RemoteFrameView::create(RemoteFrame* remoteFrame) {
  RemoteFrameView* view = new RemoteFrameView(remoteFrame);
  view->show();
  return view;
}

void RemoteFrameView::updateRemoteViewportIntersection() {
  if (!m_remoteFrame->ownerLayoutObject())
    return;

  FrameView* localRootView =
      toLocalFrame(m_remoteFrame->tree().parent())->localFrameRoot()->view();
  if (!localRootView)
    return;

  // Start with rect in remote frame's coordinate space. Then
  // mapToVisualRectInAncestorSpace will move it to the local root's coordinate
  // space and account for any clip from containing elements such as a
  // scrollable div. Passing nullptr as an argument to
  // mapToVisualRectInAncestorSpace causes it to be clipped to the viewport,
  // even if there are RemoteFrame ancestors in the frame tree.
  LayoutRect rect(0, 0, frameRect().width(), frameRect().height());
  rect.move(m_remoteFrame->ownerLayoutObject()->contentBoxOffset());
  if (!m_remoteFrame->ownerLayoutObject()->mapToVisualRectInAncestorSpace(
          nullptr, rect))
    return;
  IntRect rootVisibleRect = localRootView->visibleContentRect();
  IntRect viewportIntersection(rect);
  viewportIntersection.intersect(rootVisibleRect);
  viewportIntersection.move(-localRootView->scrollOffsetInt());

  // Translate the intersection rect from the root frame's coordinate space
  // to the remote frame's coordinate space.
  viewportIntersection = convertFromRootFrame(viewportIntersection);
  if (viewportIntersection != m_lastViewportIntersection) {
    m_remoteFrame->client()->updateRemoteViewportIntersection(
        viewportIntersection);
  }
  m_lastViewportIntersection = viewportIntersection;
}

void RemoteFrameView::dispose() {
  HTMLFrameOwnerElement* ownerElement = m_remoteFrame->deprecatedLocalOwner();
  // ownerElement can be null during frame swaps, because the
  // RemoteFrameView is disconnected before detachment.
  if (ownerElement && ownerElement->ownedWidget() == this)
    ownerElement->setWidget(nullptr);
  FrameViewBase::dispose();
}

void RemoteFrameView::invalidateRect(const IntRect& rect) {
  LayoutPartItem layoutItem = m_remoteFrame->ownerLayoutItem();
  if (layoutItem.isNull())
    return;

  LayoutRect repaintRect(rect);
  repaintRect.move(layoutItem.borderLeft() + layoutItem.paddingLeft(),
                   layoutItem.borderTop() + layoutItem.paddingTop());
  layoutItem.invalidatePaintRectangle(repaintRect);
}

void RemoteFrameView::setFrameRect(const IntRect& newRect) {
  IntRect oldRect = frameRect();

  if (newRect == oldRect)
    return;

  FrameViewBase::setFrameRect(newRect);

  frameRectsChanged();
}

void RemoteFrameView::frameRectsChanged() {
  // Update the rect to reflect the position of the frame relative to the
  // containing local frame root. The position of the local root within
  // any remote frames, if any, is accounted for by the embedder.
  IntRect newRect = frameRect();
  if (parent() && parent()->isFrameView())
    newRect = parent()->convertToRootFrame(
        toFrameView(parent())->contentsToFrame(newRect));
  m_remoteFrame->client()->frameRectsChanged(newRect);

  updateRemoteViewportIntersection();
}

void RemoteFrameView::hide() {
  setSelfVisible(false);

  FrameViewBase::hide();

  m_remoteFrame->client()->visibilityChanged(false);
}

void RemoteFrameView::show() {
  setSelfVisible(true);

  FrameViewBase::show();

  m_remoteFrame->client()->visibilityChanged(true);
}

void RemoteFrameView::setParentVisible(bool visible) {
  if (isParentVisible() == visible)
    return;

  FrameViewBase::setParentVisible(visible);
  if (!isSelfVisible())
    return;

  m_remoteFrame->client()->visibilityChanged(isVisible());
}

DEFINE_TRACE(RemoteFrameView) {
  visitor->trace(m_remoteFrame);
  FrameViewBase::trace(visitor);
}

}  // namespace blink
