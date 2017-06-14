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
    : frame_(frame), scroll_manager_(scroll_manager) {}

DEFINE_TRACE(MouseWheelEventManager) {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
}

WebInputEventResult MouseWheelEventManager::HandleWheelEvent(
    const WebMouseWheelEvent& event) {
#if OS(MACOSX)
  // Filter Mac OS specific phases, usually with a zero-delta.
  // https://crbug.com/553732
  // TODO(chongz): EventSender sends events with
  // |WebMouseWheelEvent::PhaseNone|,
  // but it shouldn't.
  const int kWheelEventPhaseNoEventMask = WebMouseWheelEvent::kPhaseEnded |
                                          WebMouseWheelEvent::kPhaseCancelled |
                                          WebMouseWheelEvent::kPhaseMayBegin;
  if ((event.phase & kWheelEventPhaseNoEventMask) ||
      (event.momentum_phase & kWheelEventPhaseNoEventMask))
    return WebInputEventResult::kNotHandled;
#endif
  Document* doc = frame_->GetDocument();

  if (doc->GetLayoutViewItem().IsNull())
    return WebInputEventResult::kNotHandled;

  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;

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

  LocalFrame* subframe = EventHandlingUtil::SubframeForTargetNode(node);
  if (subframe) {
    WebInputEventResult result =
        subframe->GetEventHandler().HandleWheelEvent(event);
    if (result != WebInputEventResult::kNotHandled)
      scroll_manager_->SetFrameWasScrolledByUser();
    return result;
  }

  if (node) {
    WheelEvent* dom_event =
        WheelEvent::Create(event, node->GetDocument().domWindow());
    DispatchEventResult dom_event_result = node->DispatchEvent(dom_event);
    if (dom_event_result != DispatchEventResult::kNotCanceled)
      return EventHandlingUtil::ToWebInputEventResult(dom_event_result);
  }

  return WebInputEventResult::kNotHandled;
}

}  // namespace blink
