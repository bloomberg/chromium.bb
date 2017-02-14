// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/GestureManager.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/editing/SelectionController.h"
#include "core/events/GestureEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

GestureManager::GestureManager(LocalFrame& frame,
                               ScrollManager& scrollManager,
                               MouseEventManager& mouseEventManager,
                               PointerEventManager& pointerEventManager,
                               SelectionController& selectionController)
    : m_frame(frame),
      m_scrollManager(scrollManager),
      m_mouseEventManager(mouseEventManager),
      m_pointerEventManager(pointerEventManager),
      m_selectionController(selectionController) {
  clear();
}

void GestureManager::clear() {
  m_suppressMouseEventsFromGestures = false;
  m_longTapShouldInvokeContextMenu = false;
  m_lastShowPressTimestamp.reset();
}

DEFINE_TRACE(GestureManager) {
  visitor->trace(m_frame);
  visitor->trace(m_scrollManager);
  visitor->trace(m_mouseEventManager);
  visitor->trace(m_pointerEventManager);
  visitor->trace(m_selectionController);
}

HitTestRequest::HitTestRequestType GestureManager::getHitTypeForGestureType(
    WebInputEvent::Type type) {
  HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent;
  switch (type) {
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapUnconfirmed:
      return hitType | HitTestRequest::Active;
    case WebInputEvent::GestureTapCancel:
      // A TapDownCancel received when no element is active shouldn't really be
      // changing hover state.
      if (!m_frame->document()->activeHoverElement())
        hitType |= HitTestRequest::ReadOnly;
      return hitType | HitTestRequest::Release;
    case WebInputEvent::GestureTap:
      return hitType | HitTestRequest::Release;
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
    case WebInputEvent::GestureTwoFingerTap:
      // FIXME: Shouldn't LongTap and TwoFingerTap clear the Active state?
      return hitType | HitTestRequest::Active | HitTestRequest::ReadOnly;
    default:
      NOTREACHED();
      return hitType | HitTestRequest::Active | HitTestRequest::ReadOnly;
  }
}

WebInputEventResult GestureManager::handleGestureEventInFrame(
    const GestureEventWithHitTestResults& targetedEvent) {
  DCHECK(!targetedEvent.event().isScrollEvent());

  Node* eventTarget = targetedEvent.hitTestResult().innerNode();
  const WebGestureEvent& gestureEvent = targetedEvent.event();

  if (m_scrollManager->canHandleGestureEvent(targetedEvent))
    return WebInputEventResult::HandledSuppressed;

  if (eventTarget) {
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
    case WebInputEvent::GestureTapDown:
      return handleGestureTapDown(targetedEvent);
    case WebInputEvent::GestureTap:
      return handleGestureTap(targetedEvent);
    case WebInputEvent::GestureShowPress:
      return handleGestureShowPress();
    case WebInputEvent::GestureLongPress:
      return handleGestureLongPress(targetedEvent);
    case WebInputEvent::GestureLongTap:
      return handleGestureLongTap(targetedEvent);
    case WebInputEvent::GestureTwoFingerTap:
      return handleGestureTwoFingerTap(targetedEvent);
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed:
      break;
    default:
      NOTREACHED();
  }

  return WebInputEventResult::NotHandled;
}

WebInputEventResult GestureManager::handleGestureTapDown(
    const GestureEventWithHitTestResults& targetedEvent) {
  m_suppressMouseEventsFromGestures =
      m_pointerEventManager->primaryPointerdownCanceled(
          targetedEvent.event().uniqueTouchEventId);
  return WebInputEventResult::NotHandled;
}

WebInputEventResult GestureManager::handleGestureTap(
    const GestureEventWithHitTestResults& targetedEvent) {
  FrameView* frameView(m_frame->view());
  const WebGestureEvent& gestureEvent = targetedEvent.event();
  HitTestRequest::HitTestRequestType hitType =
      getHitTypeForGestureType(gestureEvent.type());
  uint64_t preDispatchDomTreeVersion = m_frame->document()->domTreeVersion();
  uint64_t preDispatchStyleVersion = m_frame->document()->styleVersion();

  HitTestResult currentHitTest = targetedEvent.hitTestResult();

  // We use the adjusted position so the application isn't surprised to see a
  // event with co-ordinates outside the target's bounds.
  IntPoint adjustedPoint = frameView->rootFrameToContents(
      flooredIntPoint(gestureEvent.positionInRootFrame()));

  const unsigned modifiers = gestureEvent.modifiers();

  if (!m_suppressMouseEventsFromGestures) {
    WebMouseEvent fakeMouseMove(
        WebInputEvent::MouseMove, gestureEvent,
        WebPointerProperties::Button::NoButton,
        /* clickCount */ 0,
        static_cast<WebInputEvent::Modifiers>(
            modifiers | WebInputEvent::Modifiers::IsCompatibilityEventForTouch),
        gestureEvent.timeStampSeconds());
    m_mouseEventManager->setMousePositionAndDispatchMouseEvent(
        currentHitTest.innerNode(), currentHitTest.canvasRegionId(),
        EventTypeNames::mousemove, fakeMouseMove);
  }

  // Do a new hit-test in case the mousemove event changed the DOM.
  // Note that if the original hit test wasn't over an element (eg. was over a
  // scrollbar) we don't want to re-hit-test because it may be in the wrong
  // frame (and there's no way the page could have seen the event anyway).  Also
  // note that the position of the frame may have changed, so we need to
  // recompute the content co-ordinates (updating layout/style as
  // hitTestResultAtPoint normally would).
  // FIXME: Use a hit-test cache to avoid unnecessary hit tests.
  // http://crbug.com/398920
  if (currentHitTest.innerNode()) {
    LocalFrame* mainFrame = m_frame->localFrameRoot();
    if (mainFrame && mainFrame->view())
      mainFrame->view()->updateLifecycleToCompositingCleanPlusScrolling();
    adjustedPoint = frameView->rootFrameToContents(
        flooredIntPoint(gestureEvent.positionInRootFrame()));
    currentHitTest = EventHandlingUtil::hitTestResultInFrame(
        m_frame, adjustedPoint, hitType);
  }

  // Capture data for showUnhandledTapUIIfNeeded.
  Node* tappedNode = currentHitTest.innerNode();
  IntPoint tappedPosition = flooredIntPoint(gestureEvent.positionInRootFrame());
  Node* tappedNonTextNode = tappedNode;
  UserGestureIndicator gestureIndicator(DocumentUserGestureToken::create(
      tappedNode ? &tappedNode->document() : nullptr));

  if (tappedNonTextNode && tappedNonTextNode->isTextNode())
    tappedNonTextNode = FlatTreeTraversal::parent(*tappedNonTextNode);

  m_mouseEventManager->setClickNode(tappedNonTextNode);

  WebMouseEvent fakeMouseDown(
      WebInputEvent::MouseDown, gestureEvent,
      WebPointerProperties::Button::Left, gestureEvent.tapCount(),
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::LeftButtonDown |
          WebInputEvent::Modifiers::IsCompatibilityEventForTouch),
      gestureEvent.timeStampSeconds());

  // TODO(mustaq): We suppress MEs plus all it's side effects. What would that
  // mean for for TEs?  What's the right balance here? crbug.com/617255
  WebInputEventResult mouseDownEventResult =
      WebInputEventResult::HandledSuppressed;
  if (!m_suppressMouseEventsFromGestures) {
    m_mouseEventManager->setClickCount(gestureEvent.tapCount());

    mouseDownEventResult =
        m_mouseEventManager->setMousePositionAndDispatchMouseEvent(
            currentHitTest.innerNode(), currentHitTest.canvasRegionId(),
            EventTypeNames::mousedown, fakeMouseDown);
    m_selectionController->initializeSelectionState();
    if (mouseDownEventResult == WebInputEventResult::NotHandled) {
      mouseDownEventResult = m_mouseEventManager->handleMouseFocus(
          currentHitTest, m_frame->document()
                              ->domWindow()
                              ->getInputDeviceCapabilities()
                              ->firesTouchEvents(true));
    }
    if (mouseDownEventResult == WebInputEventResult::NotHandled) {
      mouseDownEventResult = m_mouseEventManager->handleMousePressEvent(
          MouseEventWithHitTestResults(fakeMouseDown, currentHitTest));
    }
  }

  if (currentHitTest.innerNode()) {
    DCHECK(gestureEvent.type() == WebInputEvent::GestureTap);
    HitTestResult result = currentHitTest;
    result.setToShadowHostIfInUserAgentShadowRoot();
    m_frame->chromeClient().onMouseDown(result.innerNode());
  }

  // FIXME: Use a hit-test cache to avoid unnecessary hit tests.
  // http://crbug.com/398920
  if (currentHitTest.innerNode()) {
    LocalFrame* mainFrame = m_frame->localFrameRoot();
    if (mainFrame && mainFrame->view())
      mainFrame->view()->updateAllLifecyclePhases();
    adjustedPoint = frameView->rootFrameToContents(tappedPosition);
    currentHitTest = EventHandlingUtil::hitTestResultInFrame(
        m_frame, adjustedPoint, hitType);
  }

  WebMouseEvent fakeMouseUp(
      WebInputEvent::MouseUp, gestureEvent, WebPointerProperties::Button::Left,
      gestureEvent.tapCount(),
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::IsCompatibilityEventForTouch),
      gestureEvent.timeStampSeconds());
  WebInputEventResult mouseUpEventResult =
      m_suppressMouseEventsFromGestures
          ? WebInputEventResult::HandledSuppressed
          : m_mouseEventManager->setMousePositionAndDispatchMouseEvent(
                currentHitTest.innerNode(), currentHitTest.canvasRegionId(),
                EventTypeNames::mouseup, fakeMouseUp);

  WebInputEventResult clickEventResult = WebInputEventResult::NotHandled;
  if (tappedNonTextNode) {
    if (currentHitTest.innerNode()) {
      // Updates distribution because a mouseup (or mousedown) event listener
      // can make the tree dirty at dispatchMouseEvent() invocation above.
      // Unless distribution is updated, commonAncestor would hit DCHECK.  Both
      // tappedNonTextNode and currentHitTest.innerNode()) don't need to be
      // updated because commonAncestor() will exit early if their documents are
      // different.
      tappedNonTextNode->updateDistribution();
      Node* clickTargetNode = currentHitTest.innerNode()->commonAncestor(
          *tappedNonTextNode, EventHandlingUtil::parentForClickEvent);
      clickEventResult =
          m_mouseEventManager->setMousePositionAndDispatchMouseEvent(
              clickTargetNode, String(), EventTypeNames::click, fakeMouseUp);
    }
    m_mouseEventManager->setClickNode(nullptr);
  }

  if (mouseUpEventResult == WebInputEventResult::NotHandled)
    mouseUpEventResult = m_mouseEventManager->handleMouseReleaseEvent(
        MouseEventWithHitTestResults(fakeMouseUp, currentHitTest));
  m_mouseEventManager->clearDragHeuristicState();

  WebInputEventResult eventResult = EventHandlingUtil::mergeEventResult(
      EventHandlingUtil::mergeEventResult(mouseDownEventResult,
                                          mouseUpEventResult),
      clickEventResult);
  if (eventResult == WebInputEventResult::NotHandled && tappedNode &&
      m_frame->page()) {
    bool domTreeChanged =
        preDispatchDomTreeVersion != m_frame->document()->domTreeVersion();
    bool styleChanged =
        preDispatchStyleVersion != m_frame->document()->styleVersion();

    IntPoint tappedPositionInViewport =
        frameHost()->visualViewport().rootFrameToViewport(tappedPosition);
    m_frame->chromeClient().showUnhandledTapUIIfNeeded(
        tappedPositionInViewport, tappedNode, domTreeChanged || styleChanged);
  }
  return eventResult;
}

WebInputEventResult GestureManager::handleGestureLongPress(
    const GestureEventWithHitTestResults& targetedEvent) {
  const WebGestureEvent& gestureEvent = targetedEvent.event();

  // FIXME: Ideally we should try to remove the extra mouse-specific hit-tests
  // here (re-using the supplied HitTestResult), but that will require some
  // overhaul of the touch drag-and-drop code and LongPress is such a special
  // scenario that it's unlikely to matter much in practice.

  IntPoint hitTestPoint = m_frame->view()->rootFrameToContents(
      flooredIntPoint(gestureEvent.positionInRootFrame()));
  HitTestResult hitTestResult =
      m_frame->eventHandler().hitTestResultAtPoint(hitTestPoint);

  m_longTapShouldInvokeContextMenu = false;
  bool hitTestContainsLinks = hitTestResult.URLElement() ||
                              !hitTestResult.absoluteImageURL().isNull() ||
                              !hitTestResult.absoluteMediaURL().isNull();

  if (!hitTestContainsLinks &&
      m_mouseEventManager->handleDragDropIfPossible(targetedEvent)) {
    m_longTapShouldInvokeContextMenu = true;
    return WebInputEventResult::HandledSystem;
  }

  Node* innerNode = hitTestResult.innerNode();
  if (innerNode && innerNode->layoutObject() &&
      m_selectionController->handleGestureLongPress(gestureEvent,
                                                    hitTestResult)) {
    m_mouseEventManager->focusDocumentView();
    return WebInputEventResult::HandledSystem;
  }

  return sendContextMenuEventForGesture(targetedEvent);
}

WebInputEventResult GestureManager::handleGestureLongTap(
    const GestureEventWithHitTestResults& targetedEvent) {
#if !OS(ANDROID)
  if (m_longTapShouldInvokeContextMenu) {
    m_longTapShouldInvokeContextMenu = false;
    Node* innerNode = targetedEvent.hitTestResult().innerNode();
    if (innerNode && innerNode->layoutObject())
      m_selectionController->handleGestureLongTap(targetedEvent);
    return sendContextMenuEventForGesture(targetedEvent);
  }
#endif
  return WebInputEventResult::NotHandled;
}

WebInputEventResult GestureManager::handleGestureTwoFingerTap(
    const GestureEventWithHitTestResults& targetedEvent) {
  Node* innerNode = targetedEvent.hitTestResult().innerNode();
  if (innerNode && innerNode->layoutObject())
    m_selectionController->handleGestureTwoFingerTap(targetedEvent);
  return sendContextMenuEventForGesture(targetedEvent);
}

WebInputEventResult GestureManager::sendContextMenuEventForGesture(
    const GestureEventWithHitTestResults& targetedEvent) {
  const WebGestureEvent& gestureEvent = targetedEvent.event();
  unsigned modifiers = gestureEvent.modifiers();

  if (!m_suppressMouseEventsFromGestures) {
    // Send MouseMove event prior to handling (https://crbug.com/485290).
    WebMouseEvent fakeMouseMove(
        WebInputEvent::MouseMove, gestureEvent,
        WebPointerProperties::Button::NoButton,
        /* clickCount */ 0,
        static_cast<WebInputEvent::Modifiers>(
            modifiers | WebInputEvent::IsCompatibilityEventForTouch),
        gestureEvent.timeStampSeconds());
    m_mouseEventManager->setMousePositionAndDispatchMouseEvent(
        targetedEvent.hitTestResult().innerNode(),
        targetedEvent.canvasRegionId(), EventTypeNames::mousemove,
        fakeMouseMove);
  }

  WebInputEvent::Type eventType = WebInputEvent::MouseDown;
  if (m_frame->settings() && m_frame->settings()->getShowContextMenuOnMouseUp())
    eventType = WebInputEvent::MouseUp;

  WebMouseEvent mouseEvent(
      eventType, gestureEvent, WebPointerProperties::Button::Right,
      /* clickCount */ 1,
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::RightButtonDown |
          WebInputEvent::IsCompatibilityEventForTouch),
      gestureEvent.timeStampSeconds());

  if (!m_suppressMouseEventsFromGestures && m_frame->view()) {
    HitTestRequest request(HitTestRequest::Active);
    LayoutPoint documentPoint = m_frame->view()->rootFrameToContents(
        flooredIntPoint(targetedEvent.event().positionInRootFrame()));
    MouseEventWithHitTestResults mev =
        m_frame->document()->performMouseEventHitTest(request, documentPoint,
                                                      mouseEvent);
    m_mouseEventManager->handleMouseFocus(mev.hitTestResult(),
                                          m_frame->document()
                                              ->domWindow()
                                              ->getInputDeviceCapabilities()
                                              ->firesTouchEvents(true));
  }
  return m_frame->eventHandler().sendContextMenuEvent(mouseEvent);
}

WebInputEventResult GestureManager::handleGestureShowPress() {
  m_lastShowPressTimestamp = TimeTicks::Now();

  FrameView* view = m_frame->view();
  if (!view)
    return WebInputEventResult::NotHandled;
  if (ScrollAnimatorBase* scrollAnimator = view->existingScrollAnimator())
    scrollAnimator->cancelAnimation();
  const FrameView::ScrollableAreaSet* areas = view->scrollableAreas();
  if (!areas)
    return WebInputEventResult::NotHandled;
  for (const ScrollableArea* scrollableArea : *areas) {
    ScrollAnimatorBase* animator = scrollableArea->existingScrollAnimator();
    if (animator)
      animator->cancelAnimation();
  }
  return WebInputEventResult::NotHandled;
}

FrameHost* GestureManager::frameHost() const {
  if (!m_frame->page())
    return nullptr;

  return &m_frame->page()->frameHost();
}

WTF::Optional<WTF::TimeTicks> GestureManager::getLastShowPressTimestamp()
    const {
  return m_lastShowPressTimestamp;
}

}  // namespace blink
