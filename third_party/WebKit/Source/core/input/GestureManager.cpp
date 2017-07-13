// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/GestureManager.h"

#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/SelectionController.h"
#include "core/events/GestureEvent.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

GestureManager::GestureManager(LocalFrame& frame,
                               ScrollManager& scroll_manager,
                               MouseEventManager& mouse_event_manager,
                               PointerEventManager& pointer_event_manager,
                               SelectionController& selection_controller)
    : frame_(frame),
      scroll_manager_(scroll_manager),
      mouse_event_manager_(mouse_event_manager),
      pointer_event_manager_(pointer_event_manager),
      selection_controller_(selection_controller) {
  Clear();
}

void GestureManager::Clear() {
  suppress_mouse_events_from_gestures_ = false;
  long_tap_should_invoke_context_menu_ = false;
  last_show_press_timestamp_.reset();
}

DEFINE_TRACE(GestureManager) {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(mouse_event_manager_);
  visitor->Trace(pointer_event_manager_);
  visitor->Trace(selection_controller_);
}

HitTestRequest::HitTestRequestType GestureManager::GetHitTypeForGestureType(
    WebInputEvent::Type type) {
  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kTouchEvent;
  switch (type) {
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureTapUnconfirmed:
      return hit_type | HitTestRequest::kActive;
    case WebInputEvent::kGestureTapCancel:
      // A TapDownCancel received when no element is active shouldn't really be
      // changing hover state.
      if (!frame_->GetDocument()->GetActiveElement())
        hit_type |= HitTestRequest::kReadOnly;
      return hit_type | HitTestRequest::kRelease;
    case WebInputEvent::kGestureTap:
      return hit_type | HitTestRequest::kRelease;
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
    case WebInputEvent::kGestureTwoFingerTap:
      // FIXME: Shouldn't LongTap and TwoFingerTap clear the Active state?
      return hit_type | HitTestRequest::kActive | HitTestRequest::kReadOnly;
    default:
      NOTREACHED();
      return hit_type | HitTestRequest::kActive | HitTestRequest::kReadOnly;
  }
}

WebInputEventResult GestureManager::HandleGestureEventInFrame(
    const GestureEventWithHitTestResults& targeted_event) {
  DCHECK(!targeted_event.Event().IsScrollEvent());

  Node* event_target = targeted_event.GetHitTestResult().InnerNode();
  const WebGestureEvent& gesture_event = targeted_event.Event();

  if (scroll_manager_->CanHandleGestureEvent(targeted_event))
    return WebInputEventResult::kHandledSuppressed;

  if (event_target) {
    GestureEvent* gesture_dom_event = GestureEvent::Create(
        event_target->GetDocument().domWindow(), gesture_event);
    if (gesture_dom_event) {
      DispatchEventResult gesture_dom_event_result =
          event_target->DispatchEvent(gesture_dom_event);
      if (gesture_dom_event_result != DispatchEventResult::kNotCanceled) {
        DCHECK(gesture_dom_event_result !=
               DispatchEventResult::kCanceledByEventHandler);
        return EventHandlingUtil::ToWebInputEventResult(
            gesture_dom_event_result);
      }
    }
  }

  switch (gesture_event.GetType()) {
    case WebInputEvent::kGestureTapDown:
      return HandleGestureTapDown(targeted_event);
    case WebInputEvent::kGestureTap:
      return HandleGestureTap(targeted_event);
    case WebInputEvent::kGestureShowPress:
      return HandleGestureShowPress();
    case WebInputEvent::kGestureLongPress:
      return HandleGestureLongPress(targeted_event);
    case WebInputEvent::kGestureLongTap:
      return HandleGestureLongTap(targeted_event);
    case WebInputEvent::kGestureTwoFingerTap:
      return HandleGestureTwoFingerTap(targeted_event);
    case WebInputEvent::kGesturePinchBegin:
    case WebInputEvent::kGesturePinchEnd:
    case WebInputEvent::kGesturePinchUpdate:
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureTapUnconfirmed:
      break;
    default:
      NOTREACHED();
  }

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult GestureManager::HandleGestureTapDown(
    const GestureEventWithHitTestResults& targeted_event) {
  suppress_mouse_events_from_gestures_ =
      pointer_event_manager_->PrimaryPointerdownCanceled(
          targeted_event.Event().unique_touch_event_id);
  return WebInputEventResult::kNotHandled;
}

WebInputEventResult GestureManager::HandleGestureTap(
    const GestureEventWithHitTestResults& targeted_event) {
  LocalFrameView* frame_view(frame_->View());
  const WebGestureEvent& gesture_event = targeted_event.Event();
  HitTestRequest::HitTestRequestType hit_type =
      GetHitTypeForGestureType(gesture_event.GetType());
  uint64_t pre_dispatch_dom_tree_version =
      frame_->GetDocument()->DomTreeVersion();
  uint64_t pre_dispatch_style_version = frame_->GetDocument()->StyleVersion();

  HitTestResult current_hit_test = targeted_event.GetHitTestResult();

  // We use the adjusted position so the application isn't surprised to see a
  // event with co-ordinates outside the target's bounds.
  IntPoint adjusted_point = frame_view->RootFrameToContents(
      FlooredIntPoint(gesture_event.PositionInRootFrame()));

  const unsigned modifiers = gesture_event.GetModifiers();

  if (!suppress_mouse_events_from_gestures_) {
    WebMouseEvent fake_mouse_move(
        WebInputEvent::kMouseMove, gesture_event,
        WebPointerProperties::Button::kNoButton,
        /* clickCount */ 0,
        static_cast<WebInputEvent::Modifiers>(
            modifiers |
            WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
        gesture_event.TimeStampSeconds());
    mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
        current_hit_test.InnerNode(), current_hit_test.CanvasRegionId(),
        EventTypeNames::mousemove, fake_mouse_move);
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
  if (current_hit_test.InnerNode()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (main_frame.View())
      main_frame.View()->UpdateLifecycleToCompositingCleanPlusScrolling();
    adjusted_point = frame_view->RootFrameToContents(
        FlooredIntPoint(gesture_event.PositionInRootFrame()));
    current_hit_test = EventHandlingUtil::HitTestResultInFrame(
        frame_, adjusted_point, hit_type);
  }

  // Capture data for showUnhandledTapUIIfNeeded.
  IntPoint tapped_position =
      FlooredIntPoint(gesture_event.PositionInRootFrame());
  Node* tapped_node = current_hit_test.InnerNode();
  Element* tapped_element = current_hit_test.InnerElement();
  UserGestureIndicator gesture_indicator(UserGestureToken::Create(
      tapped_node ? &tapped_node->GetDocument() : nullptr));

  mouse_event_manager_->SetClickElement(tapped_element);

  WebMouseEvent fake_mouse_down(
      WebInputEvent::kMouseDown, gesture_event,
      WebPointerProperties::Button::kLeft, gesture_event.TapCount(),
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
      gesture_event.TimeStampSeconds());

  // TODO(mustaq): We suppress MEs plus all it's side effects. What would that
  // mean for for TEs?  What's the right balance here? crbug.com/617255
  WebInputEventResult mouse_down_event_result =
      WebInputEventResult::kHandledSuppressed;
  if (!suppress_mouse_events_from_gestures_) {
    mouse_event_manager_->SetClickCount(gesture_event.TapCount());

    mouse_down_event_result =
        mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
            current_hit_test.InnerNode(), current_hit_test.CanvasRegionId(),
            EventTypeNames::mousedown, fake_mouse_down);
    selection_controller_->InitializeSelectionState();
    if (mouse_down_event_result == WebInputEventResult::kNotHandled) {
      mouse_down_event_result = mouse_event_manager_->HandleMouseFocus(
          current_hit_test, frame_->GetDocument()
                                ->domWindow()
                                ->GetInputDeviceCapabilities()
                                ->FiresTouchEvents(true));
    }
    if (mouse_down_event_result == WebInputEventResult::kNotHandled) {
      mouse_down_event_result = mouse_event_manager_->HandleMousePressEvent(
          MouseEventWithHitTestResults(fake_mouse_down, current_hit_test));
    }
  }

  if (current_hit_test.InnerNode()) {
    DCHECK(gesture_event.GetType() == WebInputEvent::kGestureTap);
    HitTestResult result = current_hit_test;
    result.SetToShadowHostIfInRestrictedShadowRoot();
    frame_->GetChromeClient().OnMouseDown(*result.InnerNode());
  }

  // FIXME: Use a hit-test cache to avoid unnecessary hit tests.
  // http://crbug.com/398920
  if (current_hit_test.InnerNode()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (main_frame.View())
      main_frame.View()->UpdateAllLifecyclePhases();
    adjusted_point = frame_view->RootFrameToContents(tapped_position);
    current_hit_test = EventHandlingUtil::HitTestResultInFrame(
        frame_, adjusted_point, hit_type);
  }

  WebMouseEvent fake_mouse_up(
      WebInputEvent::kMouseUp, gesture_event,
      WebPointerProperties::Button::kLeft, gesture_event.TapCount(),
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
      gesture_event.TimeStampSeconds());
  WebInputEventResult mouse_up_event_result =
      suppress_mouse_events_from_gestures_
          ? WebInputEventResult::kHandledSuppressed
          : mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
                current_hit_test.InnerNode(), current_hit_test.CanvasRegionId(),
                EventTypeNames::mouseup, fake_mouse_up);

  WebInputEventResult click_event_result = WebInputEventResult::kNotHandled;
  if (tapped_element) {
    if (current_hit_test.InnerNode()) {
      // Updates distribution because a mouseup (or mousedown) event listener
      // can make the tree dirty at dispatchMouseEvent() invocation above.
      // Unless distribution is updated, commonAncestor would hit DCHECK.  Both
      // tappedNonTextNode and currentHitTest.innerNode()) don't need to be
      // updated because commonAncestor() will exit early if their documents are
      // different.
      tapped_element->UpdateDistribution();
      Node* click_target_node = current_hit_test.InnerNode()->CommonAncestor(
          *tapped_element, EventHandlingUtil::ParentForClickEvent);
      click_event_result =
          mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
              click_target_node, String(), EventTypeNames::click,
              fake_mouse_up);
    }
    mouse_event_manager_->SetClickElement(nullptr);
  }

  if (mouse_up_event_result == WebInputEventResult::kNotHandled)
    mouse_up_event_result = mouse_event_manager_->HandleMouseReleaseEvent(
        MouseEventWithHitTestResults(fake_mouse_up, current_hit_test));
  mouse_event_manager_->ClearDragHeuristicState();

  WebInputEventResult event_result = EventHandlingUtil::MergeEventResult(
      EventHandlingUtil::MergeEventResult(mouse_down_event_result,
                                          mouse_up_event_result),
      click_event_result);
  if (event_result == WebInputEventResult::kNotHandled && tapped_node &&
      frame_->GetPage()) {
    bool dom_tree_changed = pre_dispatch_dom_tree_version !=
                            frame_->GetDocument()->DomTreeVersion();
    bool style_changed =
        pre_dispatch_style_version != frame_->GetDocument()->StyleVersion();

    IntPoint tapped_position_in_viewport =
        frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
            tapped_position);
    frame_->GetChromeClient().ShowUnhandledTapUIIfNeeded(
        tapped_position_in_viewport, tapped_node,
        dom_tree_changed || style_changed);
  }
  return event_result;
}

WebInputEventResult GestureManager::HandleGestureLongPress(
    const GestureEventWithHitTestResults& targeted_event) {
  const WebGestureEvent& gesture_event = targeted_event.Event();

  // FIXME: Ideally we should try to remove the extra mouse-specific hit-tests
  // here (re-using the supplied HitTestResult), but that will require some
  // overhaul of the touch drag-and-drop code and LongPress is such a special
  // scenario that it's unlikely to matter much in practice.

  IntPoint hit_test_point = frame_->View()->RootFrameToContents(
      FlooredIntPoint(gesture_event.PositionInRootFrame()));
  HitTestResult hit_test_result =
      frame_->GetEventHandler().HitTestResultAtPoint(hit_test_point);

  long_tap_should_invoke_context_menu_ = false;
  bool hit_test_contains_links = hit_test_result.URLElement() ||
                                 !hit_test_result.AbsoluteImageURL().IsNull() ||
                                 !hit_test_result.AbsoluteMediaURL().IsNull();

  if (!hit_test_contains_links &&
      mouse_event_manager_->HandleDragDropIfPossible(targeted_event)) {
    long_tap_should_invoke_context_menu_ = true;
    return WebInputEventResult::kHandledSystem;
  }

  Node* inner_node = hit_test_result.InnerNode();
  if (inner_node && inner_node->GetLayoutObject() &&
      selection_controller_->HandleGestureLongPress(hit_test_result)) {
    mouse_event_manager_->FocusDocumentView();
  }

  return SendContextMenuEventForGesture(targeted_event);
}

WebInputEventResult GestureManager::HandleGestureLongTap(
    const GestureEventWithHitTestResults& targeted_event) {
#if !defined(OS_ANDROID)
  if (long_tap_should_invoke_context_menu_) {
    long_tap_should_invoke_context_menu_ = false;
    Node* inner_node = targeted_event.GetHitTestResult().InnerNode();
    if (inner_node && inner_node->GetLayoutObject())
      selection_controller_->HandleGestureLongTap(targeted_event);
    return SendContextMenuEventForGesture(targeted_event);
  }
#endif
  return WebInputEventResult::kNotHandled;
}

WebInputEventResult GestureManager::HandleGestureTwoFingerTap(
    const GestureEventWithHitTestResults& targeted_event) {
  Node* inner_node = targeted_event.GetHitTestResult().InnerNode();
  if (inner_node && inner_node->GetLayoutObject())
    selection_controller_->HandleGestureTwoFingerTap(targeted_event);
  return SendContextMenuEventForGesture(targeted_event);
}

WebInputEventResult GestureManager::SendContextMenuEventForGesture(
    const GestureEventWithHitTestResults& targeted_event) {
  const WebGestureEvent& gesture_event = targeted_event.Event();
  unsigned modifiers = gesture_event.GetModifiers();

  if (!suppress_mouse_events_from_gestures_) {
    // Send MouseMove event prior to handling (https://crbug.com/485290).
    WebMouseEvent fake_mouse_move(
        WebInputEvent::kMouseMove, gesture_event,
        WebPointerProperties::Button::kNoButton,
        /* clickCount */ 0,
        static_cast<WebInputEvent::Modifiers>(
            modifiers | WebInputEvent::kIsCompatibilityEventForTouch),
        gesture_event.TimeStampSeconds());
    mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
        targeted_event.GetHitTestResult().InnerNode(),
        targeted_event.CanvasRegionId(), EventTypeNames::mousemove,
        fake_mouse_move);
  }

  WebInputEvent::Type event_type = WebInputEvent::kMouseDown;
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetShowContextMenuOnMouseUp())
    event_type = WebInputEvent::kMouseUp;

  WebMouseEvent mouse_event(
      event_type, gesture_event, WebPointerProperties::Button::kNoButton,
      /* clickCount */ 0,
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::kIsCompatibilityEventForTouch),
      gesture_event.TimeStampSeconds());

  if (!suppress_mouse_events_from_gestures_ && frame_->View()) {
    HitTestRequest request(HitTestRequest::kActive);
    LayoutPoint document_point = frame_->View()->RootFrameToContents(
        FlooredIntPoint(targeted_event.Event().PositionInRootFrame()));
    MouseEventWithHitTestResults mev =
        frame_->GetDocument()->PerformMouseEventHitTest(request, document_point,
                                                        mouse_event);
    mouse_event_manager_->HandleMouseFocus(mev.GetHitTestResult(),
                                           frame_->GetDocument()
                                               ->domWindow()
                                               ->GetInputDeviceCapabilities()
                                               ->FiresTouchEvents(true));
  }
  return frame_->GetEventHandler().SendContextMenuEvent(mouse_event);
}

WebInputEventResult GestureManager::HandleGestureShowPress() {
  last_show_press_timestamp_ = TimeTicks::Now();

  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;
  if (ScrollAnimatorBase* scroll_animator = view->ExistingScrollAnimator())
    scroll_animator->CancelAnimation();
  const LocalFrameView::ScrollableAreaSet* areas = view->ScrollableAreas();
  if (!areas)
    return WebInputEventResult::kNotHandled;
  for (const ScrollableArea* scrollable_area : *areas) {
    ScrollAnimatorBase* animator = scrollable_area->ExistingScrollAnimator();
    if (animator)
      animator->CancelAnimation();
  }
  return WebInputEventResult::kNotHandled;
}

WTF::Optional<WTF::TimeTicks> GestureManager::GetLastShowPressTimestamp()
    const {
  return last_show_press_timestamp_;
}

}  // namespace blink
