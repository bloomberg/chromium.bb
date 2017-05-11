// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchEventManager.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/events/TouchEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

namespace {

bool HasTouchHandlers(const EventHandlerRegistry& registry) {
  return registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventBlocking) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventPassive) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchEndOrCancelEventBlocking) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchEndOrCancelEventPassive);
}

const AtomicString& TouchEventNameForTouchPointState(
    WebTouchPoint::State state) {
  switch (state) {
    case WebTouchPoint::kStateReleased:
      return EventTypeNames::touchend;
    case WebTouchPoint::kStateCancelled:
      return EventTypeNames::touchcancel;
    case WebTouchPoint::kStatePressed:
      return EventTypeNames::touchstart;
    case WebTouchPoint::kStateMoved:
      return EventTypeNames::touchmove;
    case WebTouchPoint::kStateStationary:
    // Fall through to default
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

bool IsTouchSequenceStart(const WebTouchEvent& event) {
  if (!event.touches_length)
    return false;
  if (event.GetType() != WebInputEvent::kTouchStart)
    return false;
  for (size_t i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state != blink::WebTouchPoint::kStatePressed)
      return false;
  }
  return true;
}

// Defining this class type local to dispatchTouchEvents() and annotating
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

  WebPointerProperties::PointerType pointer_type_;
};

}  // namespace

TouchEventManager::TouchEventManager(LocalFrame& frame) : frame_(frame) {
  Clear();
}

void TouchEventManager::Clear() {
  touch_sequence_document_.Clear();
  target_for_touch_id_.clear();
  region_for_touch_id_.clear();
  touch_pressed_ = false;
  suppressing_touchmoves_within_slop_ = false;
  current_touch_action_ = TouchAction::kTouchActionAuto;
}

DEFINE_TRACE(TouchEventManager) {
  visitor->Trace(frame_);
  visitor->Trace(touch_sequence_document_);
  visitor->Trace(target_for_touch_id_);
}

WebInputEventResult TouchEventManager::DispatchTouchEvents(
    const WebTouchEvent& event,
    const HeapVector<TouchInfo>& touch_infos,
    bool all_touches_released) {
  // Build up the lists to use for the |touches|, |targetTouches| and
  // |changedTouches| attributes in the JS event. See
  // http://www.w3.org/TR/touch-events/#touchevent-interface for how these
  // lists fit together.

  if (event.GetType() == WebInputEvent::kTouchEnd ||
      event.GetType() == WebInputEvent::kTouchCancel ||
      event.touches_length > 1) {
    suppressing_touchmoves_within_slop_ = false;
  }

  if (suppressing_touchmoves_within_slop_ &&
      event.GetType() == WebInputEvent::kTouchMove) {
    if (!event.moved_beyond_slop_region)
      return WebInputEventResult::kHandledSuppressed;
    suppressing_touchmoves_within_slop_ = false;
  }

  // Holds the complete set of touches on the screen.
  TouchList* touches = TouchList::Create();

  // A different view on the 'touches' list above, filtered and grouped by
  // event target. Used for the |targetTouches| list in the JS event.
  using TargetTouchesHeapMap = HeapHashMap<EventTarget*, Member<TouchList>>;
  TargetTouchesHeapMap touches_by_target;

  // Array of touches per state, used to assemble the |changedTouches| list.
  ChangedTouches changed_touches[WebTouchPoint::kStateMax + 1];

  for (auto touch_info : touch_infos) {
    const WebTouchPoint& point = touch_info.point;
    WebTouchPoint::State point_state = point.state;

    Touch* touch = Touch::Create(
        touch_info.target_frame.Get(), touch_info.touch_node.Get(), point.id,
        point.screen_position, touch_info.content_point,
        touch_info.adjusted_radius, point.rotation_angle, point.force,
        touch_info.region);

    // Ensure this target's touch list exists, even if it ends up empty, so
    // it can always be passed to TouchEvent::Create below.
    TargetTouchesHeapMap::iterator target_touches_iterator =
        touches_by_target.find(touch_info.touch_node.Get());
    if (target_touches_iterator == touches_by_target.end()) {
      touches_by_target.Set(touch_info.touch_node.Get(), TouchList::Create());
      target_touches_iterator =
          touches_by_target.find(touch_info.touch_node.Get());
    }

    // |touches| and |targetTouches| should only contain information about
    // touches still on the screen, so if this point is released or
    // cancelled it will only appear in the |changedTouches| list.
    if (point_state != WebTouchPoint::kStateReleased &&
        point_state != WebTouchPoint::kStateCancelled) {
      touches->Append(touch);
      target_touches_iterator->value->Append(touch);
    }

    // Now build up the correct list for |changedTouches|.
    // Note that  any touches that are in the TouchStationary state (e.g. if
    // the user had several points touched but did not move them all) should
    // never be in the |changedTouches| list so we do not handle them
    // explicitly here. See https://bugs.webkit.org/show_bug.cgi?id=37609
    // for further discussion about the TouchStationary state.
    if (point_state != WebTouchPoint::kStateStationary &&
        touch_info.known_target) {
      DCHECK_LE(point_state, WebTouchPoint::kStateMax);
      if (!changed_touches[point_state].touches_)
        changed_touches[point_state].touches_ = TouchList::Create();
      changed_touches[point_state].touches_->Append(touch);
      changed_touches[point_state].targets_.insert(touch_info.touch_node);
      changed_touches[point_state].pointer_type_ = point.pointer_type;
    }
  }

  if (all_touches_released) {
    touch_sequence_document_.Clear();
    current_touch_action_ = TouchAction::kTouchActionAuto;
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;

  // Now iterate through the |changedTouches| list and |m_targets| within it,
  // sending TouchEvents to the targets as required.
  for (unsigned state = 0; state <= WebTouchPoint::kStateMax; ++state) {
    if (!changed_touches[state].touches_)
      continue;

    const AtomicString& event_name(TouchEventNameForTouchPointState(
        static_cast<WebTouchPoint::State>(state)));
    for (const auto& event_target : changed_touches[state].targets_) {
      EventTarget* touch_event_target = event_target;
      TouchEvent* touch_event = TouchEvent::Create(
          event, touches, touches_by_target.at(touch_event_target),
          changed_touches[state].touches_.Get(), event_name,
          touch_event_target->ToNode()->GetDocument().domWindow(),
          current_touch_action_);

      DispatchEventResult dom_dispatch_result =
          touch_event_target->DispatchEvent(touch_event);

      // Only report for top level documents with a single touch on
      // touch-start or the first touch-move.
      if (event.touch_start_or_first_touch_move && touch_infos.size() == 1 &&
          frame_->IsMainFrame()) {
        // Record the disposition and latency of touch starts and first touch
        // moves before and after the page is fully loaded respectively.
        int64_t latency_in_micros =
            (TimeTicks::Now() -
             TimeTicks::FromSeconds(event.TimeStampSeconds()))
                .InMicroseconds();
        if (event.IsCancelable()) {
          if (frame_->GetDocument()->IsLoadCompleted()) {
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

        // Report the touch disposition when there is an active fling animation.
        if (event.dispatch_type ==
            WebInputEvent::kListenersForcedNonBlockingDueToFling) {
          DEFINE_STATIC_LOCAL(EnumerationHistogram,
                              touch_dispositions_during_fling_histogram,
                              ("Event.Touch.TouchDispositionsDuringFling2",
                               kTouchEventDispatchResultTypeMax));
          touch_dispositions_during_fling_histogram.Count(
              touch_event->PreventDefaultCalledOnUncancelableEvent()
                  ? kHandledTouches
                  : kUnhandledTouches);
        }
      }
      event_result = EventHandlingUtil::MergeEventResult(
          event_result,
          EventHandlingUtil::ToWebInputEventResult(dom_dispatch_result));
    }
  }

  // Do not suppress any touchmoves if the touchstart is consumed.
  if (IsTouchSequenceStart(event) &&
      event_result == WebInputEventResult::kNotHandled) {
    suppressing_touchmoves_within_slop_ = true;
  }

  return event_result;
}

void TouchEventManager::UpdateTargetAndRegionMapsForTouchStarts(
    HeapVector<TouchInfo>& touch_infos) {
  for (auto& touch_info : touch_infos) {
    // Touch events implicitly capture to the touched node, and don't change
    // active/hover states themselves (Gesture events do). So we only need
    // to hit-test on touchstart and when the target could be different than
    // the corresponding pointer event target.
    if (touch_info.point.state == WebTouchPoint::kStatePressed) {
      HitTestRequest::HitTestRequestType hit_type =
          HitTestRequest::kTouchEvent | HitTestRequest::kReadOnly |
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
          (!touch_info.touch_node ||
           &touch_info.touch_node->GetDocument() != touch_sequence_document_)) {
        if (touch_sequence_document_->GetFrame()) {
          LayoutPoint frame_point = LayoutPoint(
              touch_sequence_document_->GetFrame()->View()->RootFrameToContents(
                  touch_info.point.position));
          result = EventHandlingUtil::HitTestResultInFrame(
              touch_sequence_document_->GetFrame(), frame_point, hit_type);
          Node* node = result.InnerNode();
          if (!node)
            continue;
          if (isHTMLCanvasElement(node)) {
            HitTestCanvasResult* hit_test_canvas_result =
                toHTMLCanvasElement(node)->GetControlAndIdIfHitRegionExists(
                    result.PointInInnerNodeFrame());
            if (hit_test_canvas_result->GetControl())
              node = hit_test_canvas_result->GetControl();
            touch_info.region = hit_test_canvas_result->GetId();
          }
          // Touch events should not go to text nodes.
          if (node->IsTextNode())
            node = FlatTreeTraversal::Parent(*node);
          touch_info.touch_node = node;
        } else {
          continue;
        }
      }
      if (!touch_info.touch_node)
        continue;
      if (!touch_sequence_document_) {
        // Keep track of which document should receive all touch events
        // in the active sequence. This must be a single document to
        // ensure we don't leak Nodes between documents.
        touch_sequence_document_ = &(touch_info.touch_node->GetDocument());
        DCHECK(touch_sequence_document_->GetFrame()->View());
      }

      // Ideally we'd ASSERT(!m_targetForTouchID.contains(point.id())
      // since we shouldn't get a touchstart for a touch that's already
      // down. However EventSender allows this to be violated and there's
      // some tests that take advantage of it. There may also be edge
      // cases in the browser where this happens.
      // See http://crbug.com/345372.
      target_for_touch_id_.Set(touch_info.point.id, touch_info.touch_node);

      region_for_touch_id_.Set(touch_info.point.id, touch_info.region);

      TouchAction effective_touch_action =
          TouchActionUtil::ComputeEffectiveTouchAction(*touch_info.touch_node);
      if (effective_touch_action != TouchAction::kTouchActionAuto) {
        frame_->GetPage()->GetChromeClient().SetTouchAction(
            frame_, effective_touch_action);

        // Combine the current touch action sequence with the touch action
        // for the current finger press.
        current_touch_action_ &= effective_touch_action;
      }
    }
  }
}

void TouchEventManager::SetAllPropertiesOfTouchInfos(
    HeapVector<TouchInfo>& touch_infos) {
  for (auto& touch_info : touch_infos) {
    WebTouchPoint::State point_state = touch_info.point.state;
    Node* touch_node = nullptr;
    String region_id;

    if (point_state == WebTouchPoint::kStateReleased ||
        point_state == WebTouchPoint::kStateCancelled) {
      // The target should be the original target for this touch, so get
      // it from the hashmap. As it's a release or cancel we also remove
      // it from the map.
      touch_node = target_for_touch_id_.Take(touch_info.point.id);
      region_id = region_for_touch_id_.Take(touch_info.point.id);
    } else {
      // No hittest is performed on move or stationary, since the target
      // is not allowed to change anyway.
      touch_node = target_for_touch_id_.at(touch_info.point.id);
      region_id = region_for_touch_id_.at(touch_info.point.id);
    }

    LocalFrame* target_frame = nullptr;
    bool known_target = false;
    if (touch_node) {
      Document& doc = touch_node->GetDocument();
      // If the target node has moved to a new document while it was being
      // touched, we can't send events to the new document because that could
      // leak nodes from one document to another. See http://crbug.com/394339.
      if (&doc == touch_sequence_document_.Get()) {
        target_frame = doc.GetFrame();
        known_target = true;
      }
    }
    if (!known_target) {
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

    // pagePoint should always be in the target element's document coordinates.
    FloatPoint page_point =
        target_frame->View()->RootFrameToContents(touch_info.point.position);
    float scale_factor = 1.0f / target_frame->PageZoomFactor();

    touch_info.touch_node = touch_node;
    touch_info.target_frame = target_frame;
    touch_info.content_point = page_point.ScaledBy(scale_factor);
    touch_info.adjusted_radius =
        FloatSize(touch_info.point.radius_x, touch_info.point.radius_y)
            .ScaledBy(scale_factor);
    touch_info.known_target = known_target;
    touch_info.region = region_id;
  }
}

bool TouchEventManager::ReHitTestTouchPointsIfNeeded(
    const WebTouchEvent& event,
    HeapVector<TouchInfo>& touch_infos) {
  bool new_touch_sequence = true;
  bool all_touches_released = true;

  for (unsigned i = 0; i < event.touches_length; ++i) {
    WebTouchPoint::State state = event.touches[i].state;
    if (state != WebTouchPoint::kStatePressed)
      new_touch_sequence = false;
    if (state != WebTouchPoint::kStateReleased &&
        state != WebTouchPoint::kStateCancelled)
      all_touches_released = false;
  }
  if (new_touch_sequence) {
    // Ideally we'd ASSERT(!m_touchSequenceDocument) here since we should
    // have cleared the active document when we saw the last release. But we
    // have some tests that violate this, ClusterFuzz could trigger it, and
    // there may be cases where the browser doesn't reliably release all
    // touches. http://crbug.com/345372 tracks this.
    touch_sequence_document_.Clear();
  }

  DCHECK(frame_->View());
  if (touch_sequence_document_ &&
      (!touch_sequence_document_->GetFrame() ||
       !touch_sequence_document_->GetFrame()->View())) {
    // If the active touch document has no frame or view, it's probably being
    // destroyed so we can't dispatch events.
    return false;
  }

  UpdateTargetAndRegionMapsForTouchStarts(touch_infos);

  touch_pressed_ = !all_touches_released;

  // If there's no document receiving touch events, or no handlers on the
  // document set to receive the events, then we can skip all the rest of
  // this work.
  if (!touch_sequence_document_ || !touch_sequence_document_->GetPage() ||
      !HasTouchHandlers(
          touch_sequence_document_->GetPage()->GetEventHandlerRegistry()) ||
      !touch_sequence_document_->GetFrame()) {
    if (all_touches_released) {
      touch_sequence_document_.Clear();
    }
    return false;
  }

  SetAllPropertiesOfTouchInfos(touch_infos);

  return true;
}

WebInputEventResult TouchEventManager::HandleTouchEvent(
    const WebTouchEvent& event,
    HeapVector<TouchInfo>& touch_infos) {
  if (!ReHitTestTouchPointsIfNeeded(event, touch_infos))
    return WebInputEventResult::kNotHandled;

  bool all_touches_released = true;
  for (unsigned i = 0; i < event.touches_length; ++i) {
    WebTouchPoint::State state = event.touches[i].state;
    if (state != WebTouchPoint::kStateReleased &&
        state != WebTouchPoint::kStateCancelled)
      all_touches_released = false;
  }

  return DispatchTouchEvents(event, touch_infos, all_touches_released);
}

bool TouchEventManager::IsAnyTouchActive() const {
  return touch_pressed_;
}

}  // namespace blink
