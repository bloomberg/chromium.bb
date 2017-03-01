// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/ScrollManager.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/events/GestureEvent.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/PtrUtil.h"

namespace blink {

ScrollManager::ScrollManager(LocalFrame& frame) : m_frame(frame) {
  clear();
}

void ScrollManager::clear() {
  m_lastGestureScrollOverWidget = false;
  m_scrollbarHandlingScrollGesture = nullptr;
  m_resizeScrollableArea = nullptr;
  m_offsetFromResizeCorner = LayoutSize();
  clearGestureScrollState();
}

DEFINE_TRACE(ScrollManager) {
  visitor->trace(m_frame);
  visitor->trace(m_scrollGestureHandlingNode);
  visitor->trace(m_previousGestureScrolledElement);
  visitor->trace(m_scrollbarHandlingScrollGesture);
  visitor->trace(m_resizeScrollableArea);
}

void ScrollManager::clearGestureScrollState() {
  m_scrollGestureHandlingNode = nullptr;
  m_previousGestureScrolledElement = nullptr;
  m_deltaConsumedForScrollSequence = false;
  m_currentScrollChain.clear();

  if (FrameHost* host = frameHost()) {
    bool resetX = true;
    bool resetY = true;
    host->overscrollController().resetAccumulated(resetX, resetY);
  }
}

void ScrollManager::stopAutoscroll() {
  if (AutoscrollController* controller = autoscrollController())
    controller->stopAutoscroll();
}

bool ScrollManager::middleClickAutoscrollInProgress() const {
  return autoscrollController() &&
         autoscrollController()->middleClickAutoscrollInProgress();
}

AutoscrollController* ScrollManager::autoscrollController() const {
  if (Page* page = m_frame->page())
    return &page->autoscrollController();
  return nullptr;
}

void ScrollManager::recomputeScrollChain(const Node& startNode,
                                         std::deque<int>& scrollChain) {
  scrollChain.clear();

  DCHECK(startNode.layoutObject());
  LayoutBox* curBox = startNode.layoutObject()->enclosingBox();
  Element* documentElement = m_frame->document()->documentElement();

  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller element. The RootScroller element will have a custom
  // applyScroll callback that scrolls the frame or element.
  while (curBox) {
    Node* curNode = curBox->node();
    Element* curElement = nullptr;

    // FIXME: this should reject more elements, as part of crbug.com/410974.
    if (curNode && curNode->isElementNode()) {
      curElement = toElement(curNode);
    } else if (curNode && curNode->isDocumentNode()) {
      // In normal circumastances, the documentElement will be the root
      // scroller but the documentElement itself isn't a containing block,
      // that'll be the document node rather than the element.
      curElement = documentElement;
    }

    if (curElement) {
      scrollChain.push_front(DOMNodeIds::idForNode(curElement));
      if (isViewportScrollingElement(*curElement) ||
          curElement->isSameNode(documentElement))
        break;
    }

    curBox = curBox->containingBlock();
  }
}

bool ScrollManager::logicalScroll(ScrollDirection direction,
                                  ScrollGranularity granularity,
                                  Node* startNode,
                                  Node* mousePressNode) {
  Node* node = startNode;

  if (!node)
    node = m_frame->document()->focusedElement();

  if (!node)
    node = mousePressNode;

  if ((!node || !node->layoutObject()) && m_frame->view() &&
      !m_frame->view()->layoutViewItem().isNull())
    node = m_frame->view()->layoutViewItem().node();

  if (!node)
    return false;

  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  LayoutBox* curBox = node->layoutObject()->enclosingBox();
  while (curBox) {
    ScrollDirectionPhysical physicalDirection =
        toPhysicalDirection(direction, curBox->isHorizontalWritingMode(),
                            curBox->style()->isFlippedBlocksWritingMode());

    ScrollResult result =
        curBox->scroll(granularity, toScrollDelta(physicalDirection, 1));

    if (result.didScroll()) {
      setFrameWasScrolledByUser();
      return true;
    }

    curBox = curBox->containingBlock();
  }

  return false;
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool ScrollManager::bubblingScroll(ScrollDirection direction,
                                   ScrollGranularity granularity,
                                   Node* startingNode,
                                   Node* mousePressNode) {
  // The layout needs to be up to date to determine if we can scroll. We may be
  // here because of an onLoad event, in which case the final layout hasn't been
  // performed yet.
  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();
  // FIXME: enable scroll customization in this case. See crbug.com/410974.
  if (logicalScroll(direction, granularity, startingNode, mousePressNode))
    return true;

  Frame* parentFrame = m_frame->tree().parent();
  if (!parentFrame || !parentFrame->isLocalFrame())
    return false;
  // FIXME: Broken for OOPI.
  return toLocalFrame(parentFrame)
      ->eventHandler()
      .bubblingScroll(direction, granularity, m_frame->deprecatedLocalOwner());
}

void ScrollManager::setFrameWasScrolledByUser() {
  if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
    documentLoader->initialScrollState().wasScrolledByUser = true;
}

void ScrollManager::customizedScroll(const Node& startNode,
                                     ScrollState& scrollState) {
  if (scrollState.fullyConsumed())
    return;

  if (scrollState.deltaX() || scrollState.deltaY())
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  if (m_currentScrollChain.empty())
    recomputeScrollChain(startNode, m_currentScrollChain);
  scrollState.setScrollChain(m_currentScrollChain);

  scrollState.distributeToScrollChainDescendant();
}

WebInputEventResult ScrollManager::handleGestureScrollBegin(
    const WebGestureEvent& gestureEvent) {
  Document* document = m_frame->document();

  if (document->layoutViewItem().isNull())
    return WebInputEventResult::NotHandled;

  // If there's no layoutObject on the node, send the event to the nearest
  // ancestor with a layoutObject.  Needed for <option> and <optgroup> elements
  // so we can touch scroll <select>s
  while (m_scrollGestureHandlingNode &&
         !m_scrollGestureHandlingNode->layoutObject())
    m_scrollGestureHandlingNode =
        m_scrollGestureHandlingNode->parentOrShadowHostNode();

  if (!m_scrollGestureHandlingNode)
    m_scrollGestureHandlingNode = m_frame->document()->documentElement();

  if (!m_scrollGestureHandlingNode ||
      !m_scrollGestureHandlingNode->layoutObject())
    return WebInputEventResult::NotHandled;

  passScrollGestureEventToWidget(gestureEvent,
                                 m_scrollGestureHandlingNode->layoutObject());

  m_currentScrollChain.clear();
  std::unique_ptr<ScrollStateData> scrollStateData =
      WTF::makeUnique<ScrollStateData>();
  IntPoint position = flooredIntPoint(gestureEvent.positionInRootFrame());
  scrollStateData->position_x = position.x();
  scrollStateData->position_y = position.y();
  scrollStateData->is_beginning = true;
  scrollStateData->from_user_input = true;
  scrollStateData->is_direct_manipulation =
      gestureEvent.sourceDevice == WebGestureDeviceTouchscreen;
  scrollStateData->delta_consumed_for_scroll_sequence =
      m_deltaConsumedForScrollSequence;
  ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
  customizedScroll(*m_scrollGestureHandlingNode.get(), *scrollState);
  return WebInputEventResult::HandledSystem;
}

WebInputEventResult ScrollManager::handleGestureScrollUpdate(
    const WebGestureEvent& gestureEvent) {
  DCHECK_EQ(gestureEvent.type(), WebInputEvent::GestureScrollUpdate);

  Node* node = m_scrollGestureHandlingNode.get();
  if (!node || !node->layoutObject())
    return WebInputEventResult::NotHandled;

  // Negate the deltas since the gesture event stores finger movement and
  // scrolling occurs in the direction opposite the finger's movement
  // direction. e.g. Finger moving up has negative event delta but causes the
  // page to scroll down causing positive scroll delta.
  FloatSize delta(-gestureEvent.deltaXInRootFrame(),
                  -gestureEvent.deltaYInRootFrame());
  FloatSize velocity(-gestureEvent.velocityX(), -gestureEvent.velocityY());
  FloatPoint position(gestureEvent.positionInRootFrame());

  if (delta.isZero())
    return WebInputEventResult::NotHandled;

  LayoutObject* layoutObject = node->layoutObject();

  // Try to send the event to the correct view.
  WebInputEventResult result =
      passScrollGestureEventToWidget(gestureEvent, layoutObject);
  if (result != WebInputEventResult::NotHandled) {
    // FIXME: we should allow simultaneous scrolling of nested
    // iframes along perpendicular axes. See crbug.com/466991.
    m_deltaConsumedForScrollSequence = true;
    return result;
  }

  std::unique_ptr<ScrollStateData> scrollStateData =
      WTF::makeUnique<ScrollStateData>();
  scrollStateData->delta_x = delta.width();
  scrollStateData->delta_y = delta.height();
  scrollStateData->delta_granularity = static_cast<double>(
      toPlatformScrollGranularity(gestureEvent.deltaUnits()));
  scrollStateData->velocity_x = velocity.width();
  scrollStateData->velocity_y = velocity.height();
  scrollStateData->position_x = position.x();
  scrollStateData->position_y = position.y();
  scrollStateData->should_propagate = !gestureEvent.preventPropagation();
  scrollStateData->is_in_inertial_phase =
      gestureEvent.inertialPhase() == WebGestureEvent::MomentumPhase;
  scrollStateData->is_direct_manipulation =
      gestureEvent.sourceDevice == WebGestureDeviceTouchscreen;
  scrollStateData->from_user_input = true;
  scrollStateData->delta_consumed_for_scroll_sequence =
      m_deltaConsumedForScrollSequence;
  ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
  if (m_previousGestureScrolledElement) {
    // The ScrollState needs to know what the current
    // native scrolling element is, so that for an
    // inertial scroll that shouldn't propagate, only the
    // currently scrolling element responds.
    scrollState->setCurrentNativeScrollingElement(
        m_previousGestureScrolledElement);
  }
  customizedScroll(*node, *scrollState);
  m_previousGestureScrolledElement =
      scrollState->currentNativeScrollingElement();
  m_deltaConsumedForScrollSequence =
      scrollState->deltaConsumedForScrollSequence();

  bool didScrollX = scrollState->deltaX() != delta.width();
  bool didScrollY = scrollState->deltaY() != delta.height();

  if ((!m_previousGestureScrolledElement ||
       !isViewportScrollingElement(*m_previousGestureScrolledElement)) &&
      frameHost())
    frameHost()->overscrollController().resetAccumulated(didScrollX,
                                                         didScrollY);

  if (didScrollX || didScrollY) {
    setFrameWasScrolledByUser();
    return WebInputEventResult::HandledSystem;
  }

  return WebInputEventResult::NotHandled;
}

WebInputEventResult ScrollManager::handleGestureScrollEnd(
    const WebGestureEvent& gestureEvent) {
  Node* node = m_scrollGestureHandlingNode;

  if (node && node->layoutObject()) {
    passScrollGestureEventToWidget(gestureEvent, node->layoutObject());
    std::unique_ptr<ScrollStateData> scrollStateData =
        WTF::makeUnique<ScrollStateData>();
    scrollStateData->is_ending = true;
    scrollStateData->is_in_inertial_phase =
        gestureEvent.inertialPhase() == WebGestureEvent::MomentumPhase;
    scrollStateData->from_user_input = true;
    scrollStateData->is_direct_manipulation =
        gestureEvent.sourceDevice == WebGestureDeviceTouchscreen;
    scrollStateData->delta_consumed_for_scroll_sequence =
        m_deltaConsumedForScrollSequence;
    ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
    customizedScroll(*node, *scrollState);
  }

  clearGestureScrollState();
  return WebInputEventResult::NotHandled;
}

FrameHost* ScrollManager::frameHost() const {
  if (!m_frame->page())
    return nullptr;

  return &m_frame->page()->frameHost();
}

WebInputEventResult ScrollManager::passScrollGestureEventToWidget(
    const WebGestureEvent& gestureEvent,
    LayoutObject* layoutObject) {
  DCHECK(gestureEvent.isScrollEvent());

  if (!m_lastGestureScrollOverWidget || !layoutObject ||
      !layoutObject->isLayoutPart())
    return WebInputEventResult::NotHandled;

  FrameViewBase* frameViewBase = toLayoutPart(layoutObject)->widget();

  if (!frameViewBase || !frameViewBase->isFrameView())
    return WebInputEventResult::NotHandled;

  return toFrameView(frameViewBase)
      ->frame()
      .eventHandler()
      .handleGestureScrollEvent(gestureEvent);
}

bool ScrollManager::isViewportScrollingElement(const Element& element) const {
  // The root scroller is the one Element on the page designated to perform
  // "viewport actions" like browser controls movement and overscroll glow.
  if (!m_frame->document())
    return false;

  return m_frame->document()->rootScrollerController().scrollsViewport(element);
}

WebInputEventResult ScrollManager::handleGestureScrollEvent(
    const WebGestureEvent& gestureEvent) {
  if (!m_frame->view())
    return WebInputEventResult::NotHandled;

  bool enableTouchpadScrollLatching =
      RuntimeEnabledFeatures::touchpadAndWheelScrollLatchingEnabled();

  Node* eventTarget = nullptr;
  Scrollbar* scrollbar = nullptr;
  if (gestureEvent.type() != WebInputEvent::GestureScrollBegin) {
    scrollbar = m_scrollbarHandlingScrollGesture.get();
    eventTarget = m_scrollGestureHandlingNode.get();
  }

  if (!eventTarget) {
    Document* document = m_frame->document();
    if (document->layoutViewItem().isNull())
      return WebInputEventResult::NotHandled;

    FrameView* view = m_frame->view();
    LayoutPoint viewPoint = view->rootFrameToContents(
        flooredIntPoint(gestureEvent.positionInRootFrame()));
    HitTestRequest request(HitTestRequest::ReadOnly);
    HitTestResult result(request, viewPoint);
    document->layoutViewItem().hitTest(result);

    eventTarget = result.innerNode();

    m_lastGestureScrollOverWidget = result.isOverWidget();
    m_scrollGestureHandlingNode = eventTarget;
    m_previousGestureScrolledElement = nullptr;
    m_deltaConsumedForScrollSequence = false;

    if (!scrollbar)
      scrollbar = result.scrollbar();
  }

  if (scrollbar) {
    bool shouldUpdateCapture = false;
    // scrollbar->gestureEvent always returns true for touchpad based GSB
    // events. Therefore, while mouse is over a fully scrolled scrollbar, GSB
    // won't propagate to the next scrollable layer.
    if (scrollbar->gestureEvent(gestureEvent, &shouldUpdateCapture)) {
      if (shouldUpdateCapture)
        m_scrollbarHandlingScrollGesture = scrollbar;
      return WebInputEventResult::HandledSuppressed;
    }

    // When touchpad scroll latching is enabled and mouse is over a scrollbar,
    // GSU events will always latch to the scrollbar even when it hits the
    // scroll content.
    if (enableTouchpadScrollLatching &&
        gestureEvent.type() == WebInputEvent::GestureScrollUpdate) {
      return WebInputEventResult::NotHandled;
    }

    m_scrollbarHandlingScrollGesture = nullptr;
  }

  if (eventTarget) {
    if (handleScrollGestureOnResizer(eventTarget, gestureEvent))
      return WebInputEventResult::HandledSuppressed;

    GestureEvent* gestureDomEvent =
        GestureEvent::create(eventTarget->document().domWindow(), gestureEvent);
    if (gestureDomEvent) {
      DispatchEventResult gestureDomEventResult =
          eventTarget->dispatchEvent(gestureDomEvent);
      if (gestureDomEventResult != DispatchEventResult::NotCanceled) {
        DCHECK(gestureDomEventResult !=
               DispatchEventResult::CanceledByEventHandler);
        return EventHandlingUtil::toWebInputEventResult(gestureDomEventResult);
      }
    }
  }

  switch (gestureEvent.type()) {
    case WebInputEvent::GestureScrollBegin:
      return handleGestureScrollBegin(gestureEvent);
    case WebInputEvent::GestureScrollUpdate:
      return handleGestureScrollUpdate(gestureEvent);
    case WebInputEvent::GestureScrollEnd:
      return handleGestureScrollEnd(gestureEvent);
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
      return WebInputEventResult::NotHandled;
    default:
      NOTREACHED();
      return WebInputEventResult::NotHandled;
  }
}

bool ScrollManager::isScrollbarHandlingGestures() const {
  return m_scrollbarHandlingScrollGesture.get();
}

bool ScrollManager::handleScrollGestureOnResizer(
    Node* eventTarget,
    const WebGestureEvent& gestureEvent) {
  if (gestureEvent.sourceDevice != WebGestureDeviceTouchscreen)
    return false;

  if (gestureEvent.type() == WebInputEvent::GestureScrollBegin) {
    PaintLayer* layer = eventTarget->layoutObject()
                            ? eventTarget->layoutObject()->enclosingLayer()
                            : nullptr;
    IntPoint p = m_frame->view()->rootFrameToContents(
        flooredIntPoint(gestureEvent.positionInRootFrame()));
    if (layer && layer->getScrollableArea() &&
        layer->getScrollableArea()->isPointInResizeControl(p,
                                                           ResizerForTouch)) {
      m_resizeScrollableArea = layer->getScrollableArea();
      m_resizeScrollableArea->setInResizeMode(true);
      m_offsetFromResizeCorner =
          LayoutSize(m_resizeScrollableArea->offsetFromResizeCorner(p));
      return true;
    }
  } else if (gestureEvent.type() == WebInputEvent::GestureScrollUpdate) {
    if (m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode()) {
      IntPoint pos = roundedIntPoint(gestureEvent.positionInRootFrame());
      pos.move(gestureEvent.deltaXInRootFrame(),
               gestureEvent.deltaYInRootFrame());
      m_resizeScrollableArea->resize(pos, m_offsetFromResizeCorner);
      return true;
    }
  } else if (gestureEvent.type() == WebInputEvent::GestureScrollEnd) {
    if (m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode()) {
      m_resizeScrollableArea->setInResizeMode(false);
      m_resizeScrollableArea = nullptr;
      return false;
    }
  }

  return false;
}

bool ScrollManager::inResizeMode() const {
  return m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode();
}

void ScrollManager::resize(const WebMouseEvent& evt) {
  if (evt.type() == WebInputEvent::MouseMove) {
    if (!m_frame->eventHandler().mousePressed())
      return;
    m_resizeScrollableArea->resize(flooredIntPoint(evt.positionInRootFrame()),
                                   m_offsetFromResizeCorner);
  }
}

void ScrollManager::clearResizeScrollableArea(bool shouldNotBeNull) {
  if (shouldNotBeNull)
    DCHECK(m_resizeScrollableArea);

  if (m_resizeScrollableArea)
    m_resizeScrollableArea->setInResizeMode(false);
  m_resizeScrollableArea = nullptr;
}

void ScrollManager::setResizeScrollableArea(PaintLayer* layer, IntPoint p) {
  m_resizeScrollableArea = layer->getScrollableArea();
  m_resizeScrollableArea->setInResizeMode(true);
  m_offsetFromResizeCorner =
      LayoutSize(m_resizeScrollableArea->offsetFromResizeCorner(p));
}

bool ScrollManager::canHandleGestureEvent(
    const GestureEventWithHitTestResults& targetedEvent) {
  Scrollbar* scrollbar = targetedEvent.hitTestResult().scrollbar();

  if (scrollbar) {
    bool shouldUpdateCapture = false;
    if (scrollbar->gestureEvent(targetedEvent.event(), &shouldUpdateCapture)) {
      if (shouldUpdateCapture)
        m_scrollbarHandlingScrollGesture = scrollbar;
      return true;
    }
  }
  return false;
}

}  // namespace blink
