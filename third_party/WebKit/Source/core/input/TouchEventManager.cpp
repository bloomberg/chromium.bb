// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchEventManager.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/events/TouchEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

namespace {

bool HasTouchHandlers(const EventHandlerRegistry& registry) {
  return registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventBlocking) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventPassive) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchEndOrCancelEventBlocking) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchEndOrCancelEventPassive);
}

const AtomicString& TouchEventNameForPointerEventType(
    WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kPointerUp:
      return EventTypeNames::touchend;
    case WebInputEvent::kPointerCancel:
      return EventTypeNames::touchcancel;
    case WebInputEvent::kPointerDown:
      return EventTypeNames::touchstart;
    case WebInputEvent::kPointerMove:
      return EventTypeNames::touchmove;
    default:
      NOTREACHED();
      return g_empty_atom;
  }
}

enum TouchEventDispatchResultType {
  kUnhandledTouches,  // Unhandled touch events.
  kHandledTouches,    // Handled touch events.
  kTouchEventDispatchResultTypeMax,
};

WebTouchPoint::State TouchPointStateFromPointerEventType(
    WebInputEvent::Type type,
    bool stale) {
  if (stale)
    return WebTouchPoint::kStateStationary;
  switch (type) {
    case WebInputEvent::Type::kPointerUp:
      return WebTouchPoint::kStateReleased;
    case WebInputEvent::Type::kPointerCancel:
      return WebTouchPoint::kStateCancelled;
    case WebInputEvent::Type::kPointerDown:
      return WebTouchPoint::kStatePressed;
    case WebInputEvent::Type::kPointerMove:
      return WebTouchPoint::kStateMoved;
    default:
      NOTREACHED();
      return WebTouchPoint::kStateUndefined;
  }
}

WebTouchPoint CreateWebTouchPointFromWebPointerEvent(
    const WebPointerEvent& web_pointer_event,
    bool stale) {
  WebTouchPoint web_touch_point(web_pointer_event);
  web_touch_point.state =
      TouchPointStateFromPointerEventType(web_pointer_event.GetType(), stale);
  // TODO(crbug.com/731725): This mapping needs a division by 2.
  web_touch_point.radius_x = web_pointer_event.width;
  web_touch_point.radius_y = web_pointer_event.height;
  web_touch_point.rotation_angle = web_pointer_event.rotation_angle;
  return web_touch_point;
}

void SetWebTouchEventAttributesFromWebPointerEvent(
    WebTouchEvent* web_touch_event,
    const WebPointerEvent& web_pointer_event) {
  web_touch_event->dispatch_type = web_pointer_event.dispatch_type;
  web_touch_event->touch_start_or_first_touch_move =
      web_pointer_event.touch_start_or_first_touch_move;
  web_touch_event->moved_beyond_slop_region =
      web_pointer_event.moved_beyond_slop_region;
  web_touch_event->SetFrameScale(web_pointer_event.FrameScale());
  web_touch_event->SetFrameTranslate(web_pointer_event.FrameTranslate());
  web_touch_event->SetTimeStampSeconds(web_pointer_event.TimeStampSeconds());
  web_touch_event->SetModifiers(web_pointer_event.GetModifiers());
}

// Defining this class type local to
// DispatchTouchEventFromAccumulatdTouchPoints() and annotating
// it with STACK_ALLOCATED(), runs into MSVC(VS 2013)'s C4822 warning
// that the local class doesn't provide a local definition for 'operator new'.
// Which it intentionally doesn't and shouldn't.
//
// Work around such toolchain bugginess by lifting out the type, thereby
// taking it out of C4822's reach.
class ChangedTouches final {
  STACK_ALLOCATED();

 public:
  // The touches corresponding to the particular change state this struct
  // instance represents.
  Member<TouchList> touches_;

  using EventTargetSet = HeapHashSet<Member<EventTarget>>;
  // Set of targets involved in m_touches.
  EventTargetSet targets_;
};

void ReportMetricsForTouch(const WebPointerEvent& event,
                           DispatchEventResult dom_dispatch_result,
                           bool prevent_default_called_on_uncancelable_event,
                           bool is_frame_loaded) {
  int64_t latency_in_micros =
      (TimeTicks::Now() - TimeTicks::FromSeconds(event.TimeStampSeconds()))
          .InMicroseconds();
  if (event.IsCancelable()) {
    if (is_frame_loaded) {
      DEFINE_STATIC_LOCAL(EnumerationHistogram,
                          touch_dispositions_after_page_load_histogram,
                          ("Event.Touch.TouchDispositionsAfterPageLoad",
                           kTouchEventDispatchResultTypeMax));
      touch_dispositions_after_page_load_histogram.Count(
          (dom_dispatch_result != DispatchEventResult::kNotCanceled)
              ? kHandledTouches
              : kUnhandledTouches);

      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, event_latency_after_page_load_histogram,
          ("Event.Touch.TouchLatencyAfterPageLoad", 1, 100000000, 50));
      event_latency_after_page_load_histogram.Count(latency_in_micros);
    } else {
      DEFINE_STATIC_LOCAL(EnumerationHistogram,
                          touch_dispositions_before_page_load_histogram,
                          ("Event.Touch.TouchDispositionsBeforePageLoad",
                           kTouchEventDispatchResultTypeMax));
      touch_dispositions_before_page_load_histogram.Count(
          (dom_dispatch_result != DispatchEventResult::kNotCanceled)
              ? kHandledTouches
              : kUnhandledTouches);

      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, event_latency_before_page_load_histogram,
          ("Event.Touch.TouchLatencyBeforePageLoad", 1, 100000000, 50));
      event_latency_before_page_load_histogram.Count(latency_in_micros);
    }
    // Report the touch disposition there is no active fling animation.
    DEFINE_STATIC_LOCAL(EnumerationHistogram,
                        touch_dispositions_outside_fling_histogram,
                        ("Event.Touch.TouchDispositionsOutsideFling2",
                         kTouchEventDispatchResultTypeMax));
    touch_dispositions_outside_fling_histogram.Count(
        (dom_dispatch_result != DispatchEventResult::kNotCanceled)
            ? kHandledTouches
            : kUnhandledTouches);
  }

  // Report the touch disposition when there is an active fling
  // animation.
  if (event.dispatch_type ==
      WebInputEvent::kListenersForcedNonBlockingDueToFling) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram,
                        touch_dispositions_during_fling_histogram,
                        ("Event.Touch.TouchDispositionsDuringFling2",
                         kTouchEventDispatchResultTypeMax));
    touch_dispositions_during_fling_histogram.Count(
        prevent_default_called_on_uncancelable_event ? kHandledTouches
                                                     : kUnhandledTouches);
  }
}

}  // namespace

TouchEventManager::TouchEventManager(LocalFrame& frame) : frame_(frame) {
  Clear();
}

void TouchEventManager::Clear() {
  touch_sequence_document_.Clear();
  touch_attribute_map_.clear();
  last_coalesced_touch_event_ = WebTouchEvent();
  suppressing_touchmoves_within_slop_ = false;
  current_touch_action_ = TouchAction::kTouchActionAuto;
}

DEFINE_TRACE(TouchEventManager) {
  visitor->Trace(frame_);
  visitor->Trace(touch_sequence_document_);
  visitor->Trace(touch_attribute_map_);
}

Touch* TouchEventManager::CreateDomTouch(
    const TouchEventManager::TouchPointAttributes* point_attr,
    bool* known_target) {
  Node* touch_node = point_attr->target_;
  String region_id = point_attr->region_;
  *known_target = false;
  FloatPoint content_point;
  FloatSize adjusted_radius;

  LocalFrame* target_frame = nullptr;
  if (touch_node) {
    Document& doc = touch_node->GetDocument();
    // If the target node has moved to a new document while it was being
    // touched, we can't send events to the new document because that could
    // leak nodes from one document to another. See http://crbug.com/394339.
    if (&doc == touch_sequence_document_.Get()) {
      target_frame = doc.GetFrame();
      *known_target = true;
    }
  }
  if (!(*known_target)) {
    // If we don't have a target registered for the point it means we've
    // missed our opportunity to do a hit test for it (due to some
    // optimization that prevented blink from ever seeing the
    // touchstart), or that the touch started outside the active touch
    // sequence document. We should still include the touch in the
    // Touches list reported to the application (eg. so it can
    // differentiate between a one and two finger gesture), but we won't
    // actually dispatch any events for it. Set the target to the
    // Document so that there's some valid node here. Perhaps this
    // should really be LocalDOMWindow, but in all other cases the target of
    // a Touch is a Node so using the window could be a breaking change.
    // Since we know there was no handler invoked, the specific target
    // should be completely irrelevant to the application.
    touch_node = touch_sequence_document_;
    target_frame = touch_sequence_document_->GetFrame();
  }
  DCHECK(target_frame);

  WebPointerEvent transformed_event =
      point_attr->event_.WebPointerEventInRootFrame();
  // pagePoint should always be in the target element's document coordinates.
  FloatPoint page_point = target_frame->View()->RootFrameToContents(
      transformed_event.PositionInWidget());
  float scale_factor = 1.0f / target_frame->PageZoomFactor();

  content_point = page_point.ScaledBy(scale_factor);
  adjusted_radius = FloatSize(transformed_event.width, transformed_event.height)
                        .ScaledBy(scale_factor);

  return Touch::Create(target_frame, touch_node, point_attr->event_.id,
                       transformed_event.PositionInScreen(), content_point,
                       adjusted_radius, transformed_event.rotation_angle,
                       transformed_event.force, region_id);
}

WebCoalescedInputEvent TouchEventManager::GenerateWebCoalescedInputEvent() {
  DCHECK(!touch_attribute_map_.IsEmpty());

  WebTouchEvent event;

  const auto& first_touch_pointer_event =
      touch_attribute_map_.begin()->value->event_;

  SetWebTouchEventAttributesFromWebPointerEvent(&event,
                                                first_touch_pointer_event);
  SetWebTouchEventAttributesFromWebPointerEvent(&last_coalesced_touch_event_,
                                                first_touch_pointer_event);
  WebInputEvent::Type touch_event_type = WebInputEvent::kTouchMove;
  Vector<WebPointerEvent> all_coalesced_events;
  Vector<int> available_ids;
  for (const auto& id : touch_attribute_map_.Keys())
    available_ids.push_back(id);
  std::sort(available_ids.begin(), available_ids.end());
  for (const int& touch_point_id : available_ids) {
    const auto& touch_point_attribute = touch_attribute_map_.at(touch_point_id);
    const WebPointerEvent& touch_pointer_event = touch_point_attribute->event_;
    event.touches[event.touches_length++] =
        CreateWebTouchPointFromWebPointerEvent(touch_pointer_event,
                                               touch_point_attribute->stale_);

    // Only change the touch event type from move. So if we have two pointers
    // in up and down state we just set the touch event type to the first one
    // we see.
    // TODO(crbug.com/732842): Note that event sender API allows sending any
    // mix of input and as long as we don't crash or anything we should be good
    // for now.
    if (touch_event_type == WebInputEvent::kTouchMove) {
      if (touch_pointer_event.GetType() == WebInputEvent::kPointerDown)
        touch_event_type = WebInputEvent::kTouchStart;
      else if (touch_pointer_event.GetType() == WebInputEvent::kPointerCancel)
        touch_event_type = WebInputEvent::kTouchCancel;
      else if (touch_pointer_event.GetType() == WebInputEvent::kPointerUp)
        touch_event_type = WebInputEvent::kTouchEnd;
    }

    for (const WebPointerEvent& coalesced_event :
         touch_point_attribute->coalesced_events_) {
      all_coalesced_events.push_back(coalesced_event);
    }
  }
  event.SetType(touch_event_type);
  last_coalesced_touch_event_.SetType(touch_event_type);

  // Create all coalesced touch events based on pointerevents
  struct {
    bool operator()(const WebPointerEvent& a, const WebPointerEvent& b) {
      return a.TimeStampSeconds() < b.TimeStampSeconds();
    }
  } timestamp_based_event_comparison;
  std::sort(all_coalesced_events.begin(), all_coalesced_events.end(),
            timestamp_based_event_comparison);
  WebCoalescedInputEvent result(event, std::vector<const WebInputEvent*>());
  for (const auto& web_pointer_event : all_coalesced_events) {
    if (web_pointer_event.GetType() == WebInputEvent::kPointerDown) {
      // TODO(crbug.com/732842): Technically we should never receive the
      // pointerdown twice for the same touch point. But event sender API allows
      // that. So we should handle it gracefully.
      WebTouchPoint web_touch_point(web_pointer_event);
      bool found_existing_id = false;
      for (unsigned i = 0; i < last_coalesced_touch_event_.touches_length;
           ++i) {
        if (last_coalesced_touch_event_.touches[i].id == web_pointer_event.id) {
          last_coalesced_touch_event_.touches[i] =
              CreateWebTouchPointFromWebPointerEvent(web_pointer_event, false);
          found_existing_id = true;
          break;
        }
      }
      // If the pointerdown point didn't exist add a new point to the array.
      if (!found_existing_id) {
        last_coalesced_touch_event_
            .touches[last_coalesced_touch_event_.touches_length++] =
            CreateWebTouchPointFromWebPointerEvent(web_pointer_event, false);
      }
      struct {
        bool operator()(const WebTouchPoint& a, const WebTouchPoint& b) {
          return a.id < b.id;
        }
      } id_based_event_comparison;
      std::sort(last_coalesced_touch_event_.touches,
                last_coalesced_touch_event_.touches +
                    last_coalesced_touch_event_.touches_length,
                id_based_event_comparison);
      result.AddCoalescedEvent(last_coalesced_touch_event_);
    } else {
      for (unsigned i = 0; i < last_coalesced_touch_event_.touches_length;
           ++i) {
        if (last_coalesced_touch_event_.touches[i].id == web_pointer_event.id) {
          last_coalesced_touch_event_.touches[i] =
              CreateWebTouchPointFromWebPointerEvent(web_pointer_event, false);
          result.AddCoalescedEvent(last_coalesced_touch_event_);

          // Remove up and canceled points.
          unsigned result_size = 0;
          for (unsigned j = 0; j < last_coalesced_touch_event_.touches_length;
               j++) {
            if (last_coalesced_touch_event_.touches[j].state !=
                    WebTouchPoint::kStateCancelled &&
                last_coalesced_touch_event_.touches[j].state !=
                    WebTouchPoint::kStateReleased) {
              last_coalesced_touch_event_.touches[result_size++] =
                  last_coalesced_touch_event_.touches[j];
            }
          }
          last_coalesced_touch_event_.touches_length = result_size;
          break;
        }
      }
    }

    for (unsigned i = 0; i < event.touches_length; ++i) {
      event.touches[i].state = blink::WebTouchPoint::kStateStationary;
      event.touches[i].movement_x = 0;
      event.touches[i].movement_y = 0;
    }
  }

  return result;
}

WebInputEventResult
TouchEventManager::DispatchTouchEventFromAccumulatdTouchPoints() {
  // Build up the lists to use for the |touches|, |targetTouches| and
  // |changedTouches| attributes in the JS event. See
  // http://www.w3.org/TR/touch-events/#touchevent-interface for how these
  // lists fit together.

  bool new_touch_point_since_last_dispatch = false;
  bool any_touch_canceled_or_ended = false;
  bool all_touch_points_pressed = true;

  for (const auto& attr : touch_attribute_map_.Values()) {
    if (!attr->stale_)
      new_touch_point_since_last_dispatch = true;
    if (attr->event_.GetType() == WebInputEvent::kPointerUp ||
        attr->event_.GetType() == WebInputEvent::kPointerCancel)
      any_touch_canceled_or_ended = true;
    if (attr->event_.GetType() != WebInputEvent::kPointerDown)
      all_touch_points_pressed = false;
  }

  if (!new_touch_point_since_last_dispatch)
    return WebInputEventResult::kNotHandled;

  if (any_touch_canceled_or_ended || touch_attribute_map_.size() > 1)
    suppressing_touchmoves_within_slop_ = false;

  if (suppressing_touchmoves_within_slop_) {
    // There is exactly one touch point here otherwise
    // |suppressing_touchmoves_within_slop_| would have been false.
    DCHECK_EQ(1U, touch_attribute_map_.size());
    const auto& touch_point_attribute = touch_attribute_map_.begin()->value;
    if (touch_point_attribute->event_.GetType() ==
        WebInputEvent::kPointerMove) {
      if (!touch_point_attribute->event_.moved_beyond_slop_region)
        return WebInputEventResult::kHandledSuppressed;
      suppressing_touchmoves_within_slop_ = false;
    }
  }

  // Holds the complete set of touches on the screen.
  TouchList* touches = TouchList::Create();

  // A different view on the 'touches' list above, filtered and grouped by
  // event target. Used for the |targetTouches| list in the JS event.
  using TargetTouchesHeapMap = HeapHashMap<EventTarget*, Member<TouchList>>;
  TargetTouchesHeapMap touches_by_target;

  // Array of touches per state, used to assemble the |changedTouches| list.
  ChangedTouches changed_touches[WebInputEvent::kPointerTypeLast -
                                 WebInputEvent::kPointerTypeFirst + 1];

  Vector<int> available_ids;
  for (const auto& id : touch_attribute_map_.Keys())
    available_ids.push_back(id);
  std::sort(available_ids.begin(), available_ids.end());
  for (const int& touch_point_id : available_ids) {
    const auto& touch_point_attribute = touch_attribute_map_.at(touch_point_id);
    WebInputEvent::Type event_type = touch_point_attribute->event_.GetType();
    bool known_target;

    Touch* touch = CreateDomTouch(touch_point_attribute, &known_target);
    EventTarget* touch_target = touch->target();

    // Ensure this target's touch list exists, even if it ends up empty, so
    // it can always be passed to TouchEvent::Create below.
    TargetTouchesHeapMap::iterator target_touches_iterator =
        touches_by_target.find(touch_target);
    if (target_touches_iterator == touches_by_target.end()) {
      touches_by_target.Set(touch_target, TouchList::Create());
      target_touches_iterator = touches_by_target.find(touch_target);
    }

    // |touches| and |targetTouches| should only contain information about
    // touches still on the screen, so if this point is released or
    // cancelled it will only appear in the |changedTouches| list.
    if (event_type != WebInputEvent::kPointerUp &&
        event_type != WebInputEvent::kPointerCancel) {
      touches->Append(touch);
      target_touches_iterator->value->Append(touch);
    }

    // Now build up the correct list for |changedTouches|.
    // Note that  any touches that are in the TouchStationary state (e.g. if
    // the user had several points touched but did not move them all) should
    // never be in the |changedTouches| list so we do not handle them
    // explicitly here. See https://bugs.webkit.org/show_bug.cgi?id=37609
    // for further discussion about the TouchStationary state.
    if (!touch_point_attribute->stale_ && known_target) {
      size_t event_type_idx = event_type - WebInputEvent::kPointerTypeFirst;
      if (!changed_touches[event_type_idx].touches_)
        changed_touches[event_type_idx].touches_ = TouchList::Create();
      changed_touches[event_type_idx].touches_->Append(touch);
      changed_touches[event_type_idx].targets_.insert(touch_target);
    }
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;

  // First we construct the webcoalescedinputevent containing all the coalesced
  // touch event.
  WebCoalescedInputEvent coalesced_event = GenerateWebCoalescedInputEvent();

  // Now iterate through the |changedTouches| list and |m_targets| within it,
  // sending TouchEvents to the targets as required.
  for (unsigned action = WebInputEvent::kPointerTypeFirst;
       action <= WebInputEvent::kPointerTypeLast; ++action) {
    size_t action_idx = action - WebInputEvent::kPointerTypeFirst;
    if (!changed_touches[action_idx].touches_)
      continue;

    const AtomicString& event_name(TouchEventNameForPointerEventType(
        static_cast<WebInputEvent::Type>(action)));

    for (const auto& event_target : changed_touches[action_idx].targets_) {
      EventTarget* touch_event_target = event_target;
      TouchEvent* touch_event = TouchEvent::Create(
          coalesced_event, touches, touches_by_target.at(touch_event_target),
          changed_touches[action_idx].touches_.Get(), event_name,
          touch_event_target->ToNode()->GetDocument().domWindow(),
          current_touch_action_);

      DispatchEventResult dom_dispatch_result =
          touch_event_target->DispatchEvent(touch_event);

      // Only report for top level documents with a single touch on
      // touch-start or the first touch-move.
      if (touch_attribute_map_.size() == 1 && frame_->IsMainFrame()) {
        const auto& event = touch_attribute_map_.begin()->value->event_;
        if (event.touch_start_or_first_touch_move) {
          // Record the disposition and latency of touch starts and first touch
          // moves before and after the page is fully loaded respectively.
          ReportMetricsForTouch(
              event, dom_dispatch_result,
              touch_event->PreventDefaultCalledOnUncancelableEvent(),
              frame_->GetDocument()->IsLoadCompleted());
        }
      }
      event_result = EventHandlingUtil::MergeEventResult(
          event_result,
          EventHandlingUtil::ToWebInputEventResult(dom_dispatch_result));
    }
  }

  // Suppress following touchmoves within the slop region if the touchstart is
  // not consumed.
  if (all_touch_points_pressed &&
      event_result == WebInputEventResult::kNotHandled) {
    suppressing_touchmoves_within_slop_ = true;
  }

  return event_result;
}

void TouchEventManager::UpdateTouchAttributeMapsForPointerDown(
    const WebPointerEvent& event,
    const EventHandlingUtil::PointerEventTarget& pointer_event_target) {
  // Touch events implicitly capture to the touched node, and don't change
  // active/hover states themselves (Gesture events do). So we only need
  // to hit-test on touchstart and when the target could be different than
  // the corresponding pointer event target.
  DCHECK(event.GetType() == WebInputEvent::kPointerDown);
  // Ideally we'd DCHECK(!touch_attribute_map_.Contains(event.id))
  // since we shouldn't get a touchstart for a touch that's already
  // down. However EventSender allows this to be violated and there's
  // some tests that take advantage of it. There may also be edge
  // cases in the browser where this happens.
  // See http://crbug.com/345372.
  touch_attribute_map_.Set(event.id, new TouchPointAttributes(event));

  Node* touch_node = pointer_event_target.target_node;
  String region = pointer_event_target.region;

  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kTouchEvent |
                                                HitTestRequest::kReadOnly |
                                                HitTestRequest::kActive;
  HitTestResult result;
  // For the touchPressed points hit-testing is done in
  // PointerEventManager. If it was the second touch there is a
  // capturing documents for the touch and |m_touchSequenceDocument|
  // is not null. So if PointerEventManager should hit-test again
  // against |m_touchSequenceDocument| if the target set by
  // PointerEventManager was either null or not in
  // |m_touchSequenceDocument|.
  if (touch_sequence_document_ &&
      (!touch_node || &touch_node->GetDocument() != touch_sequence_document_)) {
    if (touch_sequence_document_->GetFrame()) {
      LayoutPoint frame_point = LayoutPoint(
          touch_sequence_document_->GetFrame()->View()->RootFrameToContents(
              event.PositionInWidget()));
      result = EventHandlingUtil::HitTestResultInFrame(
          touch_sequence_document_->GetFrame(), frame_point, hit_type);
      Node* node = result.InnerNode();
      if (!node)
        return;
      if (isHTMLCanvasElement(node)) {
        HitTestCanvasResult* hit_test_canvas_result =
            toHTMLCanvasElement(node)->GetControlAndIdIfHitRegionExists(
                result.PointInInnerNodeFrame());
        if (hit_test_canvas_result->GetControl())
          node = hit_test_canvas_result->GetControl();
        region = hit_test_canvas_result->GetId();
      }
      // Touch events should not go to text nodes.
      if (node->IsTextNode())
        node = FlatTreeTraversal::Parent(*node);
      touch_node = node;
    } else {
      return;
    }
  }
  if (!touch_node)
    return;
  if (!touch_sequence_document_) {
    // Keep track of which document should receive all touch events
    // in the active sequence. This must be a single document to
    // ensure we don't leak Nodes between documents.
    touch_sequence_document_ = &(touch_node->GetDocument());
    DCHECK(touch_sequence_document_->GetFrame()->View());
  }

  TouchPointAttributes* attributes = touch_attribute_map_.at(event.id);
  attributes->target_ = touch_node;
  attributes->region_ = region;

  TouchAction effective_touch_action =
      TouchActionUtil::ComputeEffectiveTouchAction(*touch_node);
  if (effective_touch_action != TouchAction::kTouchActionAuto) {
    frame_->GetPage()->GetChromeClient().SetTouchAction(frame_,
                                                        effective_touch_action);

    // Combine the current touch action sequence with the touch action
    // for the current finger press.
    current_touch_action_ &= effective_touch_action;
  }
}

void TouchEventManager::HandleTouchPoint(
    const WebPointerEvent& event,
    const Vector<WebPointerEvent>& coalesced_events,
    const EventHandlingUtil::PointerEventTarget& pointer_event_target) {
  DCHECK_GE(event.GetType(), WebInputEvent::kPointerTypeFirst);
  DCHECK_LE(event.GetType(), WebInputEvent::kPointerTypeLast);

  if (touch_attribute_map_.IsEmpty()) {
    // Ideally we'd DCHECK(!m_touchSequenceDocument) here since we should
    // have cleared the active document when we saw the last release. But we
    // have some tests that violate this, ClusterFuzz could trigger it, and
    // there may be cases where the browser doesn't reliably release all
    // touches. http://crbug.com/345372 tracks this.
    AllTouchesReleasedCleanup();
  }

  DCHECK(frame_->View());
  if (touch_sequence_document_ &&
      (!touch_sequence_document_->GetFrame() ||
       !touch_sequence_document_->GetFrame()->View())) {
    // If the active touch document has no frame or view, it's probably being
    // destroyed so we can't dispatch events.
    return;
  }

  // In touch event model only touch starts can set the target and after that
  // the touch event always goes to that target.
  if (event.GetType() == WebInputEvent::kPointerDown) {
    UpdateTouchAttributeMapsForPointerDown(event, pointer_event_target);
  }

  // We might not receive the down action for a touch point. In that case we
  // would have never added them to |touch_attribute_map_| or hit-tested
  // them. For those just keep them in the map with a null target. Later they
  // will be targeted at the |touch_sequence_document_|.
  if (!touch_attribute_map_.Contains(event.id)) {
    touch_attribute_map_.insert(event.id, new TouchPointAttributes(event));
  }

  TouchPointAttributes* attributes = touch_attribute_map_.at(event.id);
  attributes->event_ = event;
  attributes->coalesced_events_ = coalesced_events;
  attributes->stale_ = false;
}

WebInputEventResult TouchEventManager::FlushEvents() {
  WebInputEventResult result = WebInputEventResult::kNotHandled;

  // If there's no document receiving touch events, or no handlers on the
  // document set to receive the events, then we can skip all the rest of
  // sending the event.
  if (touch_sequence_document_ && touch_sequence_document_->GetPage() &&
      HasTouchHandlers(
          touch_sequence_document_->GetPage()->GetEventHandlerRegistry()) &&
      touch_sequence_document_->GetFrame() &&
      touch_sequence_document_->GetFrame()->View()) {
    result = DispatchTouchEventFromAccumulatdTouchPoints();
  }

  // Cleanup the |touch_attribute_map_| map from released and canceled
  // touch points.
  Vector<int> released_canceled_points;
  for (auto& attributes : touch_attribute_map_.Values()) {
    if (attributes->event_.GetType() == WebInputEvent::kPointerUp ||
        attributes->event_.GetType() == WebInputEvent::kPointerCancel) {
      released_canceled_points.push_back(attributes->event_.id);
    } else {
      attributes->stale_ = true;
      attributes->event_.movement_x = 0;
      attributes->event_.movement_y = 0;
      attributes->coalesced_events_.clear();
    }
  }
  touch_attribute_map_.RemoveAll(released_canceled_points);

  if (touch_attribute_map_.IsEmpty()) {
    AllTouchesReleasedCleanup();
  }

  return result;
}

void TouchEventManager::AllTouchesReleasedCleanup() {
  touch_sequence_document_.Clear();
  current_touch_action_ = TouchAction::kTouchActionAuto;
  last_coalesced_touch_event_ = WebTouchEvent();
}

bool TouchEventManager::IsAnyTouchActive() const {
  return !touch_attribute_map_.IsEmpty();
}

}  // namespace blink
