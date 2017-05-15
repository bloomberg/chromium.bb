// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/PointerEventManager.h"

#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/MouseEventManager.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/wtf/AutoReset.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

namespace {

size_t ToPointerTypeIndex(WebPointerProperties::PointerType t) {
  return static_cast<size_t>(t);
}

Vector<std::pair<WebTouchPoint, TimeTicks>> GetCoalescedPoints(
    const Vector<WebTouchEvent>& coalesced_events,
    int id) {
  Vector<std::pair<WebTouchPoint, TimeTicks>> related_points;
  for (const auto& touch_event : coalesced_events) {
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].id == id &&
          touch_event.touches[i].state != WebTouchPoint::kStateStationary) {
        related_points.push_back(std::pair<WebTouchPoint, TimeTicks>(
            touch_event.TouchPointInRootFrame(i),
            TimeTicks::FromSeconds(touch_event.TimeStampSeconds())));
      }
    }
  }
  return related_points;
}

}  // namespace

PointerEventManager::PointerEventManager(LocalFrame& frame,
                                         MouseEventManager& mouse_event_manager)
    : frame_(frame),
      touch_event_manager_(new TouchEventManager(frame)),
      mouse_event_manager_(mouse_event_manager) {
  Clear();
}

void PointerEventManager::Clear() {
  for (auto& entry : prevent_mouse_event_for_pointer_type_)
    entry = false;
  touch_event_manager_->Clear();
  in_canceled_state_for_pointer_type_touch_ = false;
  pointer_event_factory_.Clear();
  touch_ids_for_canceled_pointerdowns_.clear();
  node_under_pointer_.clear();
  pointer_capture_target_.clear();
  pending_pointer_capture_target_.clear();
  dispatching_pointer_id_ = 0;
}

DEFINE_TRACE(PointerEventManager) {
  visitor->Trace(frame_);
  visitor->Trace(node_under_pointer_);
  visitor->Trace(pointer_capture_target_);
  visitor->Trace(pending_pointer_capture_target_);
  visitor->Trace(touch_event_manager_);
  visitor->Trace(mouse_event_manager_);
}

PointerEventManager::PointerEventBoundaryEventDispatcher::
    PointerEventBoundaryEventDispatcher(
        PointerEventManager* pointer_event_manager,
        PointerEvent* pointer_event)
    : pointer_event_manager_(pointer_event_manager),
      pointer_event_(pointer_event) {}

void PointerEventManager::PointerEventBoundaryEventDispatcher::DispatchOut(
    EventTarget* target,
    EventTarget* related_target) {
  Dispatch(target, related_target, EventTypeNames::pointerout, false);
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::DispatchOver(
    EventTarget* target,
    EventTarget* related_target) {
  Dispatch(target, related_target, EventTypeNames::pointerover, false);
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::DispatchLeave(
    EventTarget* target,
    EventTarget* related_target,
    bool check_for_listener) {
  Dispatch(target, related_target, EventTypeNames::pointerleave,
           check_for_listener);
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::DispatchEnter(
    EventTarget* target,
    EventTarget* related_target,
    bool check_for_listener) {
  Dispatch(target, related_target, EventTypeNames::pointerenter,
           check_for_listener);
}

AtomicString
PointerEventManager::PointerEventBoundaryEventDispatcher::GetLeaveEvent() {
  return EventTypeNames::pointerleave;
}

AtomicString
PointerEventManager::PointerEventBoundaryEventDispatcher::GetEnterEvent() {
  return EventTypeNames::pointerenter;
}

void PointerEventManager::PointerEventBoundaryEventDispatcher::Dispatch(
    EventTarget* target,
    EventTarget* related_target,
    const AtomicString& type,
    bool check_for_listener) {
  pointer_event_manager_->DispatchPointerEvent(
      target,
      pointer_event_manager_->pointer_event_factory_.CreatePointerBoundaryEvent(
          pointer_event_, type, related_target),
      check_for_listener);
}

WebInputEventResult PointerEventManager::DispatchPointerEvent(
    EventTarget* target,
    PointerEvent* pointer_event,
    bool check_for_listener) {
  if (!target)
    return WebInputEventResult::kNotHandled;

  // Set whether node under pointer has received pointerover or not.
  const int pointer_id = pointer_event->pointerId();

  const AtomicString& event_type = pointer_event->type();
  if ((event_type == EventTypeNames::pointerout ||
       event_type == EventTypeNames::pointerover) &&
      node_under_pointer_.Contains(pointer_id)) {
    EventTarget* target_under_pointer =
        node_under_pointer_.at(pointer_id).target;
    if (target_under_pointer == target) {
      node_under_pointer_.Set(
          pointer_id,
          EventTargetAttributes(target_under_pointer,
                                event_type == EventTypeNames::pointerover));
    }
  }

  if (!RuntimeEnabledFeatures::pointerEventEnabled())
    return WebInputEventResult::kNotHandled;
  if (!check_for_listener || target->HasEventListeners(event_type)) {
    UseCounter::Count(frame_, UseCounter::kPointerEventDispatch);
    if (event_type == EventTypeNames::pointerdown)
      UseCounter::Count(frame_, UseCounter::kPointerEventDispatchPointerDown);

    DCHECK(!dispatching_pointer_id_);
    AutoReset<int> dispatch_holder(&dispatching_pointer_id_, pointer_id);
    DispatchEventResult dispatch_result = target->DispatchEvent(pointer_event);
    return EventHandlingUtil::ToWebInputEventResult(dispatch_result);
  }
  return WebInputEventResult::kNotHandled;
}

EventTarget* PointerEventManager::GetEffectiveTargetForPointerEvent(
    EventTarget* target,
    int pointer_id) {
  if (EventTarget* capturing_target = GetCapturingNode(pointer_id))
    return capturing_target;
  return target;
}

void PointerEventManager::SendMouseAndPointerBoundaryEvents(
    Node* entered_node,
    const String& canvas_region_id,
    const WebMouseEvent& mouse_event) {
  // Mouse event type does not matter as this pointerevent will only be used
  // to create boundary pointer events and its type will be overridden in
  // |sendBoundaryEvents| function.
  PointerEvent* dummy_pointer_event = pointer_event_factory_.Create(
      EventTypeNames::mousedown, mouse_event, Vector<WebMouseEvent>(),
      frame_->GetDocument()->domWindow());

  // TODO(crbug/545647): This state should reset with pointercancel too.
  // This function also gets called for compat mouse events of touch at this
  // stage. So if the event is not frame boundary transition it is only a
  // compatibility mouse event and we do not need to change pointer event
  // behavior regarding preventMouseEvent state in that case.
  if (dummy_pointer_event->buttons() == 0 && dummy_pointer_event->isPrimary()) {
    prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
        mouse_event.pointer_type)] = false;
  }

  ProcessCaptureAndPositionOfPointerEvent(dummy_pointer_event, entered_node,
                                          canvas_region_id, mouse_event, true);
}

void PointerEventManager::SendBoundaryEvents(EventTarget* exited_target,
                                             EventTarget* entered_target,
                                             PointerEvent* pointer_event) {
  PointerEventBoundaryEventDispatcher boundary_event_dispatcher(this,
                                                                pointer_event);
  boundary_event_dispatcher.SendBoundaryEvents(exited_target, entered_target);
}

void PointerEventManager::SetNodeUnderPointer(PointerEvent* pointer_event,
                                              EventTarget* target) {
  if (node_under_pointer_.Contains(pointer_event->pointerId())) {
    EventTargetAttributes node =
        node_under_pointer_.at(pointer_event->pointerId());
    if (!target) {
      node_under_pointer_.erase(pointer_event->pointerId());
    } else if (target !=
               node_under_pointer_.at(pointer_event->pointerId()).target) {
      node_under_pointer_.Set(pointer_event->pointerId(),
                              EventTargetAttributes(target, false));
    }
    SendBoundaryEvents(node.target, target, pointer_event);
  } else if (target) {
    node_under_pointer_.insert(pointer_event->pointerId(),
                               EventTargetAttributes(target, false));
    SendBoundaryEvents(nullptr, target, pointer_event);
  }
}

void PointerEventManager::BlockTouchPointers(TimeTicks platform_time_stamp) {
  if (in_canceled_state_for_pointer_type_touch_)
    return;
  in_canceled_state_for_pointer_type_touch_ = true;

  Vector<int> touch_pointer_ids = pointer_event_factory_.GetPointerIdsOfType(
      WebPointerProperties::PointerType::kTouch);

  for (int pointer_id : touch_pointer_ids) {
    PointerEvent* pointer_event =
        pointer_event_factory_.CreatePointerCancelEvent(
            pointer_id, WebPointerProperties::PointerType::kTouch,
            platform_time_stamp);

    DCHECK(node_under_pointer_.Contains(pointer_id));
    EventTarget* target = node_under_pointer_.at(pointer_id).target;

    ProcessCaptureAndPositionOfPointerEvent(pointer_event, target);

    // TODO(nzolghadr): This event follows implicit TE capture. The actual
    // target would depend on PE capturing. Perhaps need to split TE/PE event
    // path upstream?  crbug.com/579553.
    DispatchPointerEvent(
        GetEffectiveTargetForPointerEvent(target, pointer_event->pointerId()),
        pointer_event);

    ReleasePointerCapture(pointer_event->pointerId());

    // Sending the leave/out events and lostpointercapture
    // because the next touch event will have a different id. So delayed
    // sending of lostpointercapture won't work here.
    ProcessCaptureAndPositionOfPointerEvent(pointer_event, nullptr);

    RemovePointer(pointer_event);
  }
}

void PointerEventManager::UnblockTouchPointers() {
  in_canceled_state_for_pointer_type_touch_ = false;
}

WebInputEventResult PointerEventManager::HandleTouchEvents(
    const WebTouchEvent& event,
    const Vector<WebTouchEvent>& coalesced_events) {
  if (event.GetType() == WebInputEvent::kTouchScrollStarted) {
    BlockTouchPointers(TimeTicks::FromSeconds(event.TimeStampSeconds()));
    return WebInputEventResult::kHandledSystem;
  }

  bool new_touch_sequence = true;
  for (unsigned i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state != WebTouchPoint::kStatePressed) {
      new_touch_sequence = false;
      break;
    }
  }
  if (new_touch_sequence)
    UnblockTouchPointers();

  // Do any necessary hit-tests and compute the event targets for all pointers
  // in the event.
  HeapVector<TouchEventManager::TouchInfo> touch_infos;
  ComputeTouchTargets(event, touch_infos);

  // Any finger lifting is a user gesture only when it wasn't associated with a
  // scroll.
  // https://docs.google.com/document/d/1oF1T3O7_E4t1PYHV6gyCwHxOi3ystm0eSL5xZu7nvOg/edit#
  // Re-use the same UserGesture for touchend and pointerup (but not for the
  // mouse events generated by GestureTap).
  // For the rare case of multi-finger scenarios spanning documents, it
  // seems extremely unlikely to matter which document the gesture is
  // associated with so just pick the first finger.
  RefPtr<UserGestureToken> possible_gesture_token;
  if (event.GetType() == WebInputEvent::kTouchEnd &&
      !in_canceled_state_for_pointer_type_touch_ && !touch_infos.IsEmpty() &&
      touch_infos[0].target_frame) {
    possible_gesture_token = DocumentUserGestureToken::Create(
        touch_infos[0].target_frame->GetDocument());
  }
  UserGestureIndicator holder(possible_gesture_token);

  DispatchTouchPointerEvents(event, coalesced_events, touch_infos);

  return touch_event_manager_->HandleTouchEvent(event, coalesced_events,
                                                touch_infos);
}

void PointerEventManager::ComputeTouchTargets(
    const WebTouchEvent& event,
    HeapVector<TouchEventManager::TouchInfo>& touch_infos) {
  for (unsigned touch_point = 0; touch_point < event.touches_length;
       ++touch_point) {
    TouchEventManager::TouchInfo touch_info;
    touch_info.point = event.TouchPointInRootFrame(touch_point);

    int pointer_id = pointer_event_factory_.GetPointerEventId(touch_info.point);
    // Do the hit test either when the touch first starts or when the touch
    // is not captured. |m_pendingPointerCaptureTarget| indicates the target
    // that will be capturing this event. |m_pointerCaptureTarget| may not
    // have this target yet since the processing of that will be done right
    // before firing the event.
    if (touch_info.point.state == WebTouchPoint::kStatePressed ||
        !pending_pointer_capture_target_.Contains(pointer_id)) {
      HitTestRequest::HitTestRequestType hit_type =
          HitTestRequest::kTouchEvent | HitTestRequest::kReadOnly |
          HitTestRequest::kActive;
      LayoutPoint page_point = LayoutPoint(
          frame_->View()->RootFrameToContents(touch_info.point.position));
      HitTestResult hit_test_tesult =
          frame_->GetEventHandler().HitTestResultAtPoint(page_point, hit_type);
      Node* node = hit_test_tesult.InnerNode();
      if (node) {
        touch_info.target_frame = node->GetDocument().GetFrame();
        if (isHTMLCanvasElement(node)) {
          HitTestCanvasResult* hit_test_canvas_result =
              toHTMLCanvasElement(node)->GetControlAndIdIfHitRegionExists(
                  hit_test_tesult.PointInInnerNodeFrame());
          if (hit_test_canvas_result->GetControl())
            node = hit_test_canvas_result->GetControl();
          touch_info.region = hit_test_canvas_result->GetId();
        }
        // TODO(crbug.com/612456): We need to investigate whether pointer
        // events should go to text nodes or not. If so we need to
        // update the mouse code as well. Also this logic looks similar
        // to the one in TouchEventManager. We should be able to
        // refactor it better after this investigation.
        if (node->IsTextNode())
          node = FlatTreeTraversal::Parent(*node);
        touch_info.touch_node = node;
      }
    } else {
      // Set the target of pointer event to the captured node as this
      // pointer is captured otherwise it would have gone to the if block
      // and perform a hit-test.
      touch_info.touch_node =
          pending_pointer_capture_target_.at(pointer_id)->ToNode();
      touch_info.target_frame = touch_info.touch_node->GetDocument().GetFrame();
    }

    touch_infos.push_back(touch_info);
  }
}

void PointerEventManager::DispatchTouchPointerEvents(
    const WebTouchEvent& event,
    const Vector<WebTouchEvent>& coalesced_events,
    HeapVector<TouchEventManager::TouchInfo>& touch_infos) {
  // Iterate through the touch points, sending PointerEvents to the targets as
  // required.
  for (auto touch_info : touch_infos) {
    const WebTouchPoint& touch_point = touch_info.point;
    // Do not send pointer events for stationary touches or null targetFrame
    if (touch_info.touch_node && touch_info.target_frame &&
        touch_point.state != WebTouchPoint::kStateStationary &&
        !in_canceled_state_for_pointer_type_touch_) {
      PointerEvent* pointer_event = pointer_event_factory_.Create(
          touch_point, GetCoalescedPoints(coalesced_events, touch_point.id),
          static_cast<WebInputEvent::Modifiers>(event.GetModifiers()),
          TimeTicks::FromSeconds(event.TimeStampSeconds()),
          touch_info.target_frame,
          touch_info.touch_node
              ? touch_info.touch_node->GetDocument().domWindow()
              : nullptr);

      WebInputEventResult result =
          SendTouchPointerEvent(touch_info.touch_node, pointer_event);

      // If a pointerdown has been canceled, queue the unique id to allow
      // suppressing mouse events from gesture events. For mouse events
      // fired from GestureTap & GestureLongPress (which are triggered by
      // single touches only), it is enough to queue the ids only for
      // primary pointers.
      // TODO(mustaq): What about other cases (e.g. GestureTwoFingerTap)?
      if (result != WebInputEventResult::kNotHandled &&
          pointer_event->type() == EventTypeNames::pointerdown &&
          pointer_event->isPrimary()) {
        touch_ids_for_canceled_pointerdowns_.push_back(
            event.unique_touch_event_id);
      }
    }
  }
}

WebInputEventResult PointerEventManager::SendTouchPointerEvent(
    EventTarget* target,
    PointerEvent* pointer_event) {
  if (in_canceled_state_for_pointer_type_touch_)
    return WebInputEventResult::kNotHandled;

  ProcessCaptureAndPositionOfPointerEvent(pointer_event, target);

  // Setting the implicit capture for touch
  if (pointer_event->type() == EventTypeNames::pointerdown)
    SetPointerCapture(pointer_event->pointerId(), target);

  WebInputEventResult result = DispatchPointerEvent(
      GetEffectiveTargetForPointerEvent(target, pointer_event->pointerId()),
      pointer_event);

  if (pointer_event->type() == EventTypeNames::pointerup ||
      pointer_event->type() == EventTypeNames::pointercancel) {
    ReleasePointerCapture(pointer_event->pointerId());

    // Sending the leave/out events and lostpointercapture because the next
    // touch event will have a different id.
    ProcessCaptureAndPositionOfPointerEvent(pointer_event, nullptr);

    RemovePointer(pointer_event);
  }

  return result;
}

WebInputEventResult PointerEventManager::SendMousePointerEvent(
    Node* target,
    const String& canvas_region_id,
    const AtomicString& mouse_event_type,
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events) {
  PointerEvent* pointer_event = pointer_event_factory_.Create(
      mouse_event_type, mouse_event, coalesced_events,
      frame_->GetDocument()->domWindow());

  // This is for when the mouse is released outside of the page.
  if (pointer_event->type() == EventTypeNames::pointermove &&
      !pointer_event->buttons()) {
    ReleasePointerCapture(pointer_event->pointerId());
    // Send got/lostpointercapture rightaway if necessary.
    ProcessPendingPointerCapture(pointer_event);

    if (pointer_event->isPrimary()) {
      prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
          mouse_event.pointer_type)] = false;
    }
  }

  EventTarget* pointer_event_target = ProcessCaptureAndPositionOfPointerEvent(
      pointer_event, target, canvas_region_id, mouse_event, true);

  EventTarget* effective_target = GetEffectiveTargetForPointerEvent(
      pointer_event_target, pointer_event->pointerId());

  WebInputEventResult result =
      DispatchPointerEvent(effective_target, pointer_event);

  if (result != WebInputEventResult::kNotHandled &&
      pointer_event->type() == EventTypeNames::pointerdown &&
      pointer_event->isPrimary()) {
    prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
        mouse_event.pointer_type)] = true;
  }

  if (pointer_event->isPrimary() &&
      !prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
          mouse_event.pointer_type)]) {
    EventTarget* mouse_target = effective_target;
    // Event path could be null if pointer event is not dispatched and
    // that happens for example when pointer event feature is not enabled.
    if (!EventHandlingUtil::IsInDocument(mouse_target) &&
        pointer_event->HasEventPath()) {
      for (const auto& context :
           pointer_event->GetEventPath().NodeEventContexts()) {
        if (EventHandlingUtil::IsInDocument(context.GetNode())) {
          mouse_target = context.GetNode();
          break;
        }
      }
    }
    result = EventHandlingUtil::MergeEventResult(
        result, mouse_event_manager_->DispatchMouseEvent(
                    mouse_target, mouse_event_type, mouse_event,
                    canvas_region_id, nullptr));
  }

  if (pointer_event->type() == EventTypeNames::pointerup ||
      pointer_event->type() == EventTypeNames::pointercancel) {
    ReleasePointerCapture(pointer_event->pointerId());
    // Send got/lostpointercapture rightaway if necessary.
    ProcessPendingPointerCapture(pointer_event);

    if (pointer_event->isPrimary()) {
      prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
          mouse_event.pointer_type)] = false;
    }
  }

  if (mouse_event.GetType() == WebInputEvent::kMouseLeave &&
      mouse_event.pointer_type == WebPointerProperties::PointerType::kPen) {
    pointer_event_factory_.Remove(pointer_event->pointerId());
  }
  return result;
}

bool PointerEventManager::GetPointerCaptureState(
    int pointer_id,
    EventTarget** pointer_capture_target,
    EventTarget** pending_pointer_capture_target) {
  PointerCapturingMap::const_iterator it;

  it = pointer_capture_target_.find(pointer_id);
  EventTarget* pointer_capture_target_temp =
      (it != pointer_capture_target_.end()) ? it->value : nullptr;
  it = pending_pointer_capture_target_.find(pointer_id);
  EventTarget* pending_pointercapture_target_temp =
      (it != pending_pointer_capture_target_.end()) ? it->value : nullptr;

  if (pointer_capture_target)
    *pointer_capture_target = pointer_capture_target_temp;
  if (pending_pointer_capture_target)
    *pending_pointer_capture_target = pending_pointercapture_target_temp;

  return pointer_capture_target_temp != pending_pointercapture_target_temp;
}

EventTarget* PointerEventManager::ProcessCaptureAndPositionOfPointerEvent(
    PointerEvent* pointer_event,
    EventTarget* hit_test_target,
    const String& canvas_region_id,
    const WebMouseEvent& mouse_event,
    bool send_mouse_event) {
  ProcessPendingPointerCapture(pointer_event);

  PointerCapturingMap::const_iterator it =
      pointer_capture_target_.find(pointer_event->pointerId());
  if (EventTarget* pointercapture_target =
          (it != pointer_capture_target_.end()) ? it->value : nullptr)
    hit_test_target = pointercapture_target;

  SetNodeUnderPointer(pointer_event, hit_test_target);
  if (send_mouse_event) {
    mouse_event_manager_->SetNodeUnderMouse(
        hit_test_target ? hit_test_target->ToNode() : nullptr, canvas_region_id,
        mouse_event);
  }
  return hit_test_target;
}

void PointerEventManager::ProcessPendingPointerCapture(
    PointerEvent* pointer_event) {
  EventTarget* pointer_capture_target;
  EventTarget* pending_pointer_capture_target;
  const int pointer_id = pointer_event->pointerId();
  const bool is_capture_changed = GetPointerCaptureState(
      pointer_id, &pointer_capture_target, &pending_pointer_capture_target);

  if (!is_capture_changed)
    return;

  // We have to check whether the pointerCaptureTarget is null or not because
  // we are checking whether it is still connected to its document or not.
  if (pointer_capture_target) {
    // Re-target lostpointercapture to the document when the element is
    // no longer participating in the tree.
    EventTarget* target = pointer_capture_target;
    if (target->ToNode() && !target->ToNode()->isConnected()) {
      target = target->ToNode()->ownerDocument();
    }
    DispatchPointerEvent(
        target, pointer_event_factory_.CreatePointerCaptureEvent(
                    pointer_event, EventTypeNames::lostpointercapture));
  }

  if (pending_pointer_capture_target) {
    SetNodeUnderPointer(pointer_event, pending_pointer_capture_target);
    DispatchPointerEvent(pending_pointer_capture_target,
                         pointer_event_factory_.CreatePointerCaptureEvent(
                             pointer_event, EventTypeNames::gotpointercapture));
    pointer_capture_target_.Set(pointer_id, pending_pointer_capture_target);
  } else {
    pointer_capture_target_.erase(pointer_id);
  }
}

void PointerEventManager::RemoveTargetFromPointerCapturingMapping(
    PointerCapturingMap& map,
    const EventTarget* target) {
  // We could have kept a reverse mapping to make this deletion possibly
  // faster but it adds some code complication which might not be worth of
  // the performance improvement considering there might not be a lot of
  // active pointer or pointer captures at the same time.
  PointerCapturingMap tmp = map;
  for (PointerCapturingMap::iterator it = tmp.begin(); it != tmp.end(); ++it) {
    if (it->value == target)
      map.erase(it->key);
  }
}

EventTarget* PointerEventManager::GetCapturingNode(int pointer_id) {
  if (pointer_capture_target_.Contains(pointer_id))
    return pointer_capture_target_.at(pointer_id);
  return nullptr;
}

void PointerEventManager::RemovePointer(PointerEvent* pointer_event) {
  int pointer_id = pointer_event->pointerId();
  if (pointer_event_factory_.Remove(pointer_id)) {
    pending_pointer_capture_target_.erase(pointer_id);
    pointer_capture_target_.erase(pointer_id);
    node_under_pointer_.erase(pointer_id);
  }
}

void PointerEventManager::ElementRemoved(EventTarget* target) {
  RemoveTargetFromPointerCapturingMapping(pending_pointer_capture_target_,
                                          target);
}

void PointerEventManager::SetPointerCapture(int pointer_id,
                                            EventTarget* target) {
  UseCounter::Count(frame_, UseCounter::kPointerEventSetCapture);
  if (pointer_event_factory_.IsActiveButtonsState(pointer_id)) {
    if (pointer_id != dispatching_pointer_id_) {
      UseCounter::Count(frame_,
                        UseCounter::kPointerEventSetCaptureOutsideDispatch);
    }
    pending_pointer_capture_target_.Set(pointer_id, target);
  }
}

void PointerEventManager::ReleasePointerCapture(int pointer_id,
                                                EventTarget* target) {
  // Only the element that is going to get the next pointer event can release
  // the capture. Note that this might be different from
  // |m_pointercaptureTarget|. |m_pointercaptureTarget| holds the element
  // that had the capture until now and has been receiving the pointerevents
  // but |m_pendingPointerCaptureTarget| indicated the element that gets the
  // very next pointer event. They will be the same if there was no change in
  // capturing of a particular |pointerId|. See crbug.com/614481.
  if (pending_pointer_capture_target_.at(pointer_id) == target)
    ReleasePointerCapture(pointer_id);
}

bool PointerEventManager::HasPointerCapture(int pointer_id,
                                            const EventTarget* target) const {
  return pending_pointer_capture_target_.at(pointer_id) == target;
}

bool PointerEventManager::HasProcessedPointerCapture(
    int pointer_id,
    const EventTarget* target) const {
  return pointer_capture_target_.at(pointer_id) == target;
}

void PointerEventManager::ReleasePointerCapture(int pointer_id) {
  pending_pointer_capture_target_.erase(pointer_id);
}

bool PointerEventManager::IsActive(const int pointer_id) const {
  return pointer_event_factory_.IsActive(pointer_id);
}

// This function checks the type of the pointer event to be touch as touch
// pointer events are the only ones that are directly dispatched from the main
// page managers to their target (event if target is in an iframe) and only
// those managers will keep track of these pointer events.
bool PointerEventManager::IsTouchPointerIdActiveOnFrame(
    int pointer_id,
    LocalFrame* frame) const {
  if (pointer_event_factory_.GetPointerType(pointer_id) !=
      WebPointerProperties::PointerType::kTouch)
    return false;
  Node* last_node_receiving_event =
      node_under_pointer_.Contains(pointer_id)
          ? node_under_pointer_.at(pointer_id).target->ToNode()
          : nullptr;
  return last_node_receiving_event &&
         last_node_receiving_event->GetDocument().GetFrame() == frame;
}

bool PointerEventManager::IsAnyTouchActive() const {
  return touch_event_manager_->IsAnyTouchActive();
}

bool PointerEventManager::PrimaryPointerdownCanceled(
    uint32_t unique_touch_event_id) {
  // It's safe to assume that uniqueTouchEventIds won't wrap back to 0 from
  // 2^32-1 (>4.2 billion): even with a generous 100 unique ids per touch
  // sequence & one sequence per 10 second, it takes 13+ years to wrap back.
  while (!touch_ids_for_canceled_pointerdowns_.IsEmpty()) {
    uint32_t first_id = touch_ids_for_canceled_pointerdowns_.front();
    if (first_id > unique_touch_event_id)
      return false;
    touch_ids_for_canceled_pointerdowns_.TakeFirst();
    if (first_id == unique_touch_event_id)
      return true;
  }
  return false;
}

}  // namespace blink
