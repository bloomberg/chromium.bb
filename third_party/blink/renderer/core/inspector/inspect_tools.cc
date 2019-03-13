// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspect_tools.h"

#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

Node* HoveredNodeForPoint(LocalFrame* frame,
                          const IntPoint& point_in_root_frame,
                          bool ignore_pointer_events_none) {
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  if (ignore_pointer_events_none)
    hit_type |= HitTestRequest::kIgnorePointerEventsNone;
  HitTestRequest request(hit_type);
  HitTestLocation location(
      frame->View()->ConvertFromRootFrame(point_in_root_frame));
  HitTestResult result(request, location);
  frame->ContentLayoutObject()->HitTest(location, result);
  Node* node = result.InnerPossiblyPseudoNode();
  while (node && node->getNodeType() == Node::kTextNode)
    node = node->parentNode();
  return node;
}

Node* HoveredNodeForEvent(LocalFrame* frame,
                          const WebGestureEvent& event,
                          bool ignore_pointer_events_none) {
  return HoveredNodeForPoint(frame,
                             RoundedIntPoint(event.PositionInRootFrame()),
                             ignore_pointer_events_none);
}

Node* HoveredNodeForEvent(LocalFrame* frame,
                          const WebMouseEvent& event,
                          bool ignore_pointer_events_none) {
  return HoveredNodeForPoint(frame,
                             RoundedIntPoint(event.PositionInRootFrame()),
                             ignore_pointer_events_none);
}

Node* HoveredNodeForEvent(LocalFrame* frame,
                          const WebPointerEvent& event,
                          bool ignore_pointer_events_none) {
  WebPointerEvent transformed_point = event.WebPointerEventInRootFrame();
  return HoveredNodeForPoint(
      frame, RoundedIntPoint(transformed_point.PositionInWidget()),
      ignore_pointer_events_none);
}

}  // namespace

// SearchingForNodeTool --------------------------------------------------------

SearchingForNodeTool::SearchingForNodeTool(InspectorDOMAgent* dom_agent,
                                           bool ua_shadow,
                                           const String& config)
    : dom_agent_(dom_agent), ua_shadow_(ua_shadow) {
  std::unique_ptr<protocol::Value> value =
      protocol::StringUtil::parseJSON(config);
  if (!value)
    return;
  protocol::ErrorSupport errors;
  std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config =
      protocol::Overlay::HighlightConfig::fromValue(value.get(), &errors);
  inspect_mode_highlight_config_ =
      InspectorOverlayAgent::ToHighlightConfig(highlight_config.get());
}

void SearchingForNodeTool::Trace(blink::Visitor* visitor) {
  InspectTool::Trace(visitor);
  visitor->Trace(dom_agent_);
  visitor->Trace(hovered_node_for_inspect_mode_);
}

bool SearchingForNodeTool::HandleMouseMove(const WebMouseEvent& event) {
  LocalFrame* frame = overlay_->GetFrame();
  if (!frame || !frame->View() || !frame->ContentLayoutObject())
    return false;
  Node* node = HoveredNodeForEvent(
      frame, event, event.GetModifiers() & WebInputEvent::kShiftKey);

  // Do not highlight within user agent shadow root unless requested.
  if (!ua_shadow_) {
    ShadowRoot* shadow_root = InspectorDOMAgent::UserAgentShadowRoot(node);
    if (shadow_root)
      node = &shadow_root->host();
  }

  // Shadow roots don't have boxes - use host element instead.
  if (node && node->IsShadowRoot())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return true;

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
    if (!IsA<LocalFrame>(frame_owner->ContentFrame())) {
      // Do not consume event so that remote frame can handle it.
      overlay_->hideHighlight();
      hovered_node_for_inspect_mode_.Clear();
      return false;
    }
  }

  Node* event_target = (event.GetModifiers() & WebInputEvent::kShiftKey)
                           ? HoveredNodeForEvent(frame, event, false)
                           : nullptr;
  if (event_target == node)
    event_target = nullptr;

  if (node && inspect_mode_highlight_config_) {
    hovered_node_for_inspect_mode_ = node;
    NodeHighlightRequested(node);
    bool omit_tooltip = event.GetModifiers() &
                        (WebInputEvent::kControlKey | WebInputEvent::kMetaKey);
    overlay_->InnerHighlightNode(node, event_target, String(),
                                 *inspect_mode_highlight_config_, omit_tooltip);
  }
  return true;
}

bool SearchingForNodeTool::HandleMouseDown(const WebMouseEvent& event,
                                           bool* swallow_next_mouse_up) {
  if (hovered_node_for_inspect_mode_) {
    *swallow_next_mouse_up = true;
    overlay_->Inspect(hovered_node_for_inspect_mode_.Get());
    hovered_node_for_inspect_mode_.Clear();
    return true;
  }
  return false;
}

bool SearchingForNodeTool::HandleGestureTapEvent(const WebGestureEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    overlay_->InnerHighlightNode(node, nullptr, String(),
                                 *inspect_mode_highlight_config_, false);
    overlay_->Inspect(node);
    return true;
  }
  return false;
}

bool SearchingForNodeTool::HandlePointerEvent(const WebPointerEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    overlay_->InnerHighlightNode(node, nullptr, String(),
                                 *inspect_mode_highlight_config_, false);
    overlay_->Inspect(node);
    return true;
  }
  return false;
}

void SearchingForNodeTool::NodeHighlightRequested(Node* node) {
  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return;

  int node_id = dom_agent_->PushNodePathToFrontend(node);
  if (node_id)
    frontend_->nodeHighlightRequested(node_id);
}

// ScreenshotTool --------------------------------------------------------------

void ScreenshotTool::DoInit() {
  auto& client = overlay_->GetFrame()->GetPage()->GetChromeClient();
  client.SetCursorOverridden(false);
  client.SetCursor(CrossCursor(), overlay_->GetFrame());
  client.SetCursorOverridden(true);
}

bool ScreenshotTool::HandleMouseUp(const WebMouseEvent& event) {
  if (screenshot_anchor_ == IntPoint::Zero())
    return true;
  float scale = 1.0f;
  IntPoint p1 = screenshot_anchor_;
  IntPoint p2 = screenshot_position_;
  if (LocalFrame* frame = overlay_->GetFrame()) {
    scale = frame->GetPage()->PageScaleFactor();
    p1 = frame->View()->ConvertFromRootFrame(p1);
    p2 = frame->View()->ConvertFromRootFrame(p2);
    if (const RootFrameViewport* root_frame_viewport =
            frame->View()->GetRootFrameViewport()) {
      IntSize scroll_offset = FlooredIntSize(
          root_frame_viewport->LayoutViewport().GetScrollOffset());
      p1 += scroll_offset;
      p2 += scroll_offset;
    }
  }
  float dp_to_dip = 1.f / overlay_->WindowToViewportScale();
  p1.Scale(scale, dp_to_dip);
  p2.Scale(scale, dp_to_dip);
  // Points are in device independent pixels (dip) now.
  IntRect rect =
      UnionRectEvenIfEmpty(IntRect(p1, IntSize()), IntRect(p2, IntSize()));
  if (rect.Width() < 5 || rect.Height() < 5) {
    screenshot_anchor_ = IntPoint::Zero();
    return true;
  }
  frontend_->screenshotRequested(protocol::Page::Viewport::create()
                                     .setX(rect.X())
                                     .setY(rect.Y())
                                     .setWidth(rect.Width())
                                     .setHeight(rect.Height())
                                     .setScale(scale)
                                     .build());
  return true;
}

bool ScreenshotTool::HandleKeyboardEvent(const WebKeyboardEvent& event,
                                         bool* swallow_next_escape_up) {
  if (event.GetType() == WebInputEvent::kRawKeyDown &&
      event.windows_key_code == VKEY_ESCAPE &&
      screenshot_anchor_ != IntPoint::Zero()) {
    screenshot_anchor_ = IntPoint::Zero();
    *swallow_next_escape_up = true;
    return true;
  }
  return false;
}

bool ScreenshotTool::HandleMouseDown(const WebMouseEvent& event,
                                     bool* swallow_next_mouse_up) {
  screenshot_anchor_ = RoundedIntPoint(event.PositionInRootFrame());
  screenshot_position_ = screenshot_anchor_;
  return true;
}

bool ScreenshotTool::HandleMouseMove(const WebMouseEvent& event) {
  screenshot_position_ = RoundedIntPoint(event.PositionInRootFrame());
  return true;
}

void ScreenshotTool::Draw(float scale) {
  if (screenshot_anchor_ == IntPoint::Zero())
    return;
  const VisualViewport& visual_viewport =
      overlay_->GetFrame()->GetPage()->GetVisualViewport();
  IntPoint p1 = visual_viewport.RootFrameToViewport(screenshot_anchor_);
  IntPoint p2 = visual_viewport.RootFrameToViewport(screenshot_position_);
  p1.Scale(scale, scale);
  p2.Scale(scale, scale);
  std::unique_ptr<protocol::DictionaryValue> data =
      protocol::DictionaryValue::create();
  data->setInteger("x1", p1.X());
  data->setInteger("y1", p1.Y());
  data->setInteger("x2", p2.X());
  data->setInteger("y2", p2.Y());
  overlay_->EvaluateInOverlay("drawScreenshotBorder", std::move(data));
}

// PausedInDebuggerTool --------------------------------------------------------

void PausedInDebuggerTool::Draw(float scale) {
  overlay_->EvaluateInOverlay("drawPausedInDebuggerMessage", message_);
}

}  // namespace blink
