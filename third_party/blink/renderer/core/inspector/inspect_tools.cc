// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspect_tools.h"

#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

InspectorHighlightContrastInfo FetchContrast(Node* node) {
  InspectorHighlightContrastInfo result;
  if (!node->IsElementNode())
    return result;

  Vector<Color> bgcolors;
  String font_size;
  String font_weight;
  InspectorCSSAgent::GetBackgroundColors(ToElement(node), &bgcolors, &font_size,
                                         &font_weight);
  if (bgcolors.size() == 1) {
    result.font_size = font_size;
    result.font_weight = font_weight;
    result.background_color = bgcolors[0];
  }
  return result;
}

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
  highlight_config_ =
      InspectorOverlayAgent::ToHighlightConfig(highlight_config.get());
}

void SearchingForNodeTool::Trace(blink::Visitor* visitor) {
  InspectTool::Trace(visitor);
  visitor->Trace(dom_agent_);
  visitor->Trace(hovered_node_);
  visitor->Trace(event_target_node_);
}

void SearchingForNodeTool::Draw(float scale) {
  Node* node = hovered_node_.Get();
  if (!hovered_node_)
    return;
  bool append_element_info = (node->IsElementNode() || node->IsTextNode()) &&
                             !omit_tooltip_ && highlight_config_->show_info &&
                             node->GetLayoutObject() &&
                             node->GetDocument().GetFrame();
  InspectorHighlight highlight(node, *highlight_config_, contrast_info_,
                               append_element_info);
  if (event_target_node_) {
    highlight.AppendEventTargetQuads(event_target_node_.Get(),
                                     *highlight_config_);
  }
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

bool SearchingForNodeTool::HandleInputEvent(LocalFrameView* frame_view,
                                            const WebInputEvent& input_event,
                                            bool* swallow_next_mouse_up,
                                            bool* swallow_next_escape_up) {
  if (input_event.GetType() == WebInputEvent::kGestureScrollBegin ||
      input_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    hovered_node_.Clear();
    event_target_node_.Clear();
    overlay_->ScheduleUpdate();
    return false;
  }
  return InspectTool::HandleInputEvent(
      frame_view, input_event, swallow_next_mouse_up, swallow_next_escape_up);
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
      hovered_node_.Clear();
      return false;
    }
  }

  // Store values for the highlight.
  hovered_node_ = node;
  event_target_node_ = (event.GetModifiers() & WebInputEvent::kShiftKey)
                           ? HoveredNodeForEvent(frame, event, false)
                           : nullptr;
  if (event_target_node_ == hovered_node_)
    event_target_node_ = nullptr;
  omit_tooltip_ = event.GetModifiers() &
                  (WebInputEvent::kControlKey | WebInputEvent::kMetaKey);

  contrast_info_ = FetchContrast(node);
  NodeHighlightRequested(node);
  return true;
}

bool SearchingForNodeTool::HandleMouseDown(const WebMouseEvent& event,
                                           bool* swallow_next_mouse_up) {
  if (hovered_node_) {
    *swallow_next_mouse_up = true;
    overlay_->Inspect(hovered_node_.Get());
    hovered_node_.Clear();
    return true;
  }
  return false;
}

bool SearchingForNodeTool::HandleGestureTapEvent(const WebGestureEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, false);
  if (node) {
    overlay_->Inspect(node);
    return true;
  }
  return false;
}

bool SearchingForNodeTool::HandlePointerEvent(const WebPointerEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, false);
  if (node) {
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

// QuadHighlightTool -----------------------------------------------------------

QuadHighlightTool::QuadHighlightTool(std::unique_ptr<FloatQuad> quad,
                                     Color color,
                                     Color outline_color)
    : quad_(std::move(quad)), color_(color), outline_color_(outline_color) {}

bool QuadHighlightTool::ForwardEventsToOverlay() {
  return false;
}

bool QuadHighlightTool::HideOnHideHighlight() {
  return true;
}

void QuadHighlightTool::Draw(float scale) {
  InspectorHighlight highlight(scale);
  highlight.AppendQuad(*quad_, color_, outline_color_);
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

// NodeHighlightTool -----------------------------------------------------------

NodeHighlightTool::NodeHighlightTool(
    Member<Node> node,
    String selector_list,
    std::unique_ptr<InspectorHighlightConfig> highlight_config)
    : node_(node),
      selector_list_(selector_list),
      highlight_config_(std::move(highlight_config)) {
  contrast_info_ = FetchContrast(node);
}

bool NodeHighlightTool::ForwardEventsToOverlay() {
  return false;
}

bool NodeHighlightTool::HideOnHideHighlight() {
  return true;
}

void NodeHighlightTool::Draw(float scale) {
  DrawNode();
  DrawMatchingSelector();
}

void NodeHighlightTool::DrawNode() {
  bool append_element_info = (node_->IsElementNode() || node_->IsTextNode()) &&
                             highlight_config_->show_info &&
                             node_->GetLayoutObject() &&
                             node_->GetDocument().GetFrame();
  InspectorHighlight highlight(node_.Get(), *highlight_config_, contrast_info_,
                               append_element_info);
  std::unique_ptr<protocol::DictionaryValue> highlight_json =
      highlight.AsProtocolValue();
  overlay_->EvaluateInOverlay("drawHighlight", std::move(highlight_json));
}

void NodeHighlightTool::DrawMatchingSelector() {
  if (selector_list_.IsEmpty() || !node_)
    return;
  DummyExceptionStateForTesting exception_state;
  ContainerNode* query_base = node_->ContainingShadowRoot();
  if (!query_base)
    query_base = node_->ownerDocument();
  StaticElementList* elements = query_base->QuerySelectorAll(
      AtomicString(selector_list_), exception_state);
  if (exception_state.HadException())
    return;

  for (unsigned i = 0; i < elements->length(); ++i) {
    Element* element = elements->item(i);
    InspectorHighlight highlight(element, *highlight_config_, contrast_info_,
                                 false);
    overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
  }
}

void NodeHighlightTool::Trace(blink::Visitor* visitor) {
  InspectTool::Trace(visitor);
  visitor->Trace(node_);
}

// ShowViewSizeTool ------------------------------------------------------------

void ShowViewSizeTool::Draw(float scale) {
  overlay_->EvaluateInOverlay("drawViewSize", "");
}

CString ShowViewSizeTool::GetDataResourceName() {
  return "inspect_tool_viewport_size.html";
}

bool ShowViewSizeTool::ForwardEventsToOverlay() {
  return false;
}

// ScreenshotTool --------------------------------------------------------------

void ScreenshotTool::DoInit() {
  auto& client = overlay_->GetFrame()->GetPage()->GetChromeClient();
  client.SetCursorOverridden(false);
  client.SetCursor(CrossCursor(), overlay_->GetFrame());
  client.SetCursorOverridden(true);
}

CString ScreenshotTool::GetDataResourceName() {
  return "inspect_tool_screenshot.html";
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
  float rscale = 1.f / scale;
  p1.Scale(rscale, rscale);
  p2.Scale(rscale, rscale);
  std::unique_ptr<protocol::DictionaryValue> data =
      protocol::DictionaryValue::create();
  data->setInteger("x1", p1.X());
  data->setInteger("y1", p1.Y());
  data->setInteger("x2", p2.X());
  data->setInteger("y2", p2.Y());
  overlay_->EvaluateInOverlay("drawScreenshotBorder", std::move(data));
}

// PausedInDebuggerTool --------------------------------------------------------

CString PausedInDebuggerTool::GetDataResourceName() {
  return "inspect_tool_paused.html";
}

void PausedInDebuggerTool::Draw(float scale) {
  overlay_->EvaluateInOverlay("drawPausedInDebuggerMessage", message_);
}

}  // namespace blink
