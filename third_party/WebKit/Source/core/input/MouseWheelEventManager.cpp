// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/MouseWheelEventManager.h"

#include "core/dom/Document.h"
#include "core/events/WheelEvent.h"
#include "core/frame/LocalFrameView.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/ScrollManager.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/api/LayoutViewItem.h"
#include "public/platform/WebMouseWheelEvent.h"

namespace blink {
MouseWheelEventManager::MouseWheelEventManager(LocalFrame& frame,
                                               ScrollManager& scroll_manager)
    : frame_(frame), wheel_target_(nullptr), scroll_manager_(scroll_manager) {}

DEFINE_TRACE(MouseWheelEventManager) {
  visitor->Trace(frame_);
  visitor->Trace(wheel_target_);
  visitor->Trace(scroll_manager_);
}

void MouseWheelEventManager::Clear() {
  wheel_target_ = nullptr;
}

WebInputEventResult MouseWheelEventManager::HandleWheelEvent(
    const WebMouseWheelEvent& event) {
  bool wheel_scroll_latching =
      RuntimeEnabledFeatures::TouchpadAndWheelScrollLatchingEnabled();

  Document* doc = frame_->GetDocument();
  if (!doc || doc->GetLayoutViewItem().IsNull())
    return WebInputEventResult::kNotHandled;

  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;

  if (wheel_scroll_latching) {
    const int kWheelEventPhaseEndedEventMask =
        WebMouseWheelEvent::kPhaseEnded | WebMouseWheelEvent::kPhaseCancelled;
    const int kWheelEventPhaseNoEventMask =
        kWheelEventPhaseEndedEventMask | WebMouseWheelEvent::kPhaseMayBegin;

    if ((event.phase & kWheelEventPhaseEndedEventMask) ||
        (event.momentum_phase & kWheelEventPhaseEndedEventMask)) {
      wheel_target_ = nullptr;
    }

    if ((event.phase & kWheelEventPhaseNoEventMask) ||
        (event.momentum_phase & kWheelEventPhaseNoEventMask)) {
      // Filter wheel events with zero deltas and reset the wheel_target_ node.
      DCHECK(!event.delta_x && !event.delta_y);
      return WebInputEventResult::kNotHandled;
    }

    if (event.phase == WebMouseWheelEvent::kPhaseBegan) {
      // Find and save the wheel_target_, this target will be used for the rest
      // of the current scrolling sequence.
      DCHECK(!wheel_target_);
      wheel_target_ = FindTargetNode(event, doc, view);
    }
  } else {  // !wheel_scroll_latching, wheel_target_ will be updated for each
            // wheel event.
#if OS(MACOSX)
    // Filter Mac OS specific phases, usually with a zero-delta.
    // https://crbug.com/553732
    // TODO(chongz): EventSender sends events with
    // |WebMouseWheelEvent::PhaseNone|,
    // but it shouldn't.
    const int kWheelEventPhaseNoEventMask =
        WebMouseWheelEvent::kPhaseEnded | WebMouseWheelEvent::kPhaseCancelled |
        WebMouseWheelEvent::kPhaseMayBegin;
    if ((event.phase & kWheelEventPhaseNoEventMask) ||
        (event.momentum_phase & kWheelEventPhaseNoEventMask))
      return WebInputEventResult::kNotHandled;
#endif

    wheel_target_ = FindTargetNode(event, doc, view);
  }

  LocalFrame* subframe =
      EventHandlingUtil::SubframeForTargetNode(wheel_target_.Get());
  if (subframe) {
    WebInputEventResult result =
        subframe->GetEventHandler().HandleWheelEvent(event);
    if (result != WebInputEventResult::kNotHandled)
      scroll_manager_->SetFrameWasScrolledByUser();
    return result;
  }

  if (wheel_target_) {
    WheelEvent* dom_event =
        WheelEvent::Create(event, wheel_target_->GetDocument().domWindow());
    DispatchEventResult dom_event_result =
        wheel_target_->DispatchEvent(dom_event);
    if (dom_event_result != DispatchEventResult::kNotCanceled)
      return EventHandlingUtil::ToWebInputEventResult(dom_event_result);
  }

  return WebInputEventResult::kNotHandled;
}

Node* MouseWheelEventManager::FindTargetNode(const WebMouseWheelEvent& event,
                                             const Document* doc,
                                             const LocalFrameView* view) {
  DCHECK(doc && !doc->GetLayoutViewItem().IsNull() && view);
  LayoutPoint v_point =
      view->RootFrameToContents(FlooredIntPoint(event.PositionInRootFrame()));

  HitTestRequest request(HitTestRequest::kReadOnly);
  HitTestResult result(request, v_point);
  doc->GetLayoutViewItem().HitTest(result);

  Node* node = result.InnerNode();
  // Wheel events should not dispatch to text nodes.
  if (node && node->IsTextNode())
    node = FlatTreeTraversal::Parent(*node);

  // If we're over the frame scrollbar, scroll the document.
  if (!node && result.GetScrollbar())
    node = doc->documentElement();

  return node;
}

}  // namespace blink
