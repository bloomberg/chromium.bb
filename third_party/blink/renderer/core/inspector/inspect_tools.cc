// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspect_tools.h"

#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
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

std::unique_ptr<protocol::Array<double>> RectForLayoutRect(
    const LayoutRect& rect) {
  std::unique_ptr<protocol::Array<double>> result =
      protocol::Array<double>::create();

  result->addItem(rect.X());
  result->addItem(rect.Y());
  result->addItem(rect.Width());
  result->addItem(rect.Height());
  return result;
}

// Returns |layout_object|'s bounding box in document coordinates.
LayoutRect RectInRootFrame(const LayoutObject* layout_object) {
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  LayoutRect rect_in_absolute(layout_object->AbsoluteBoundingBoxFloatRect());
  return local_frame_view
             ? local_frame_view->ConvertToRootFrame(rect_in_absolute)
             : rect_in_absolute;
}

LayoutRect TextFragmentRectInRootFrame(
    const LayoutObject* layout_object,
    const LayoutText::TextBoxInfo& text_box) {
  FloatRect local_coords_text_box_rect(text_box.local_rect);
  LayoutRect absolute_coords_text_box_rect(
      layout_object->LocalToAbsoluteQuad(local_coords_text_box_rect)
          .BoundingBox());
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  return local_frame_view ? local_frame_view->ConvertToRootFrame(
                                absolute_coords_text_box_rect)
                          : absolute_coords_text_box_rect;
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
                               append_element_info, is_locked_ancestor_);
  if (event_target_node_) {
    highlight.AppendEventTargetQuads(event_target_node_.Get(),
                                     *highlight_config_);
  }
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

bool SearchingForNodeTool::HandleInputEvent(LocalFrameView* frame_view,
                                            const WebInputEvent& input_event,
                                            bool* swallow_next_mouse_up) {
  if (input_event.GetType() == WebInputEvent::kGestureScrollBegin ||
      input_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    hovered_node_.Clear();
    event_target_node_.Clear();
    overlay_->ScheduleUpdate();
    return false;
  }
  return InspectTool::HandleInputEvent(frame_view, input_event,
                                       swallow_next_mouse_up);
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

  // If |node| is in a display locked subtree, highlight the highest locked
  // element instead.
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node)) {
    node = locked_ancestor;
    is_locked_ancestor_ = true;
  } else {
    is_locked_ancestor_ = false;
  }

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
    : selector_list_(selector_list),
      highlight_config_(std::move(highlight_config)) {
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node)) {
    is_locked_ancestor_ = true;
    node_ = locked_ancestor;
  } else {
    node_ = node;
  }
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
                               append_element_info, is_locked_ancestor_);
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
    // Skip elements in locked subtrees.
    if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*element))
      continue;
    InspectorHighlight highlight(element, *highlight_config_, contrast_info_,
                                 false /* append_element_info */,
                                 false /* is_locked_ancestor */);
    overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
  }
}

void NodeHighlightTool::Trace(blink::Visitor* visitor) {
  InspectTool::Trace(visitor);
  visitor->Trace(node_);
}

// NearbyDistanceTool ----------------------------------------------------------

CString NearbyDistanceTool::GetDataResourceName() {
  return "inspect_tool_distances.html";
}

bool NearbyDistanceTool::HandleMouseDown(const WebMouseEvent& event,
                                         bool* swallow_next_mouse_up) {
  return true;
}

bool NearbyDistanceTool::HandleMouseMove(const WebMouseEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, true);

  // Do not highlight within user agent shadow root
  ShadowRoot* shadow_root = InspectorDOMAgent::UserAgentShadowRoot(node);
  if (shadow_root)
    node = &shadow_root->host();

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
  return true;
}

bool NearbyDistanceTool::HandleMouseUp(const WebMouseEvent& event) {
  return true;
}

void NearbyDistanceTool::Draw(float scale) {
  Node* node = hovered_node_.Get();
  if (!node)
    return;
  node->GetDocument().EnsurePaintLocationDataValidForNode(node);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return;

  CSSStyleDeclaration* style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(node, true);
  std::unique_ptr<protocol::DictionaryValue> computed_style =
      protocol::DictionaryValue::create();
  for (size_t i = 0; i < style->length(); ++i) {
    AtomicString name(style->item(i));
    const CSSValue* value = style->GetPropertyCSSValueInternal(name);
    if (!value)
      continue;
    if (value->IsColorValue()) {
      Color color = static_cast<const cssvalue::CSSColorValue*>(value)->Value();
      String hex_color =
          String::Format("#%02X%02X%02X%02X", color.Red(), color.Green(),
                         color.Blue(), color.Alpha());
      computed_style->setString(name, hex_color);
    } else {
      computed_style->setString(name, value->CssText());
    }
  }

  std::unique_ptr<protocol::DOM::BoxModel> model;
  InspectorHighlight::GetBoxModel(node, &model, false);
  boxes_ = protocol::Array<protocol::Array<double>>::create();
  VisitNode(&(node->GetDocument()));
  LayoutRect document_rect(node->GetDocument().GetLayoutView()->DocumentRect());
  LocalFrameView* local_frame_view = node->GetDocument().View();
  boxes_->addItem(
      RectForLayoutRect(local_frame_view->ConvertToRootFrame(document_rect)));

  std::unique_ptr<protocol::DictionaryValue> object =
      protocol::DictionaryValue::create();
  object->setArray("boxes", boxes_->toValue());
  object->setArray("content", model->getContent()->toValue());
  object->setArray("padding", model->getPadding()->toValue());
  object->setArray("border", model->getBorder()->toValue());
  object->setObject("style", std::move(computed_style));
  overlay_->EvaluateInOverlay("drawDistances", std::move(object));
}

void NearbyDistanceTool::VisitNode(Node* node) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (layout_object)
    AddLayoutBox(layout_object);

  if (node->IsElementNode()) {
    Element* element = ToElement(node);

    if (element->GetPseudoId()) {
      if (layout_object)
        VisitPseudoLayoutChildren(element->GetPseudoId(), layout_object);
    } else {
      for (PseudoId pseudo_id :
           {kPseudoIdFirstLetter, kPseudoIdBefore, kPseudoIdAfter}) {
        if (Node* pseudo_node = element->GetPseudoElement(pseudo_id))
          VisitNode(pseudo_node);
      }
    }
  }

  if (!node->IsContainerNode())
    return;
  Node* first_child = InspectorDOMSnapshotAgent::FirstChild(*node, false);
  for (Node* child = first_child; child;
       child = InspectorDOMSnapshotAgent::NextSibling(*child, false))
    VisitNode(child);
}

void NearbyDistanceTool::VisitPseudoLayoutChildren(
    PseudoId pseudo_id,
    LayoutObject* layout_object) {
  protocol::DOM::PseudoType pseudo_type;
  if (!InspectorDOMAgent::GetPseudoElementType(pseudo_id, &pseudo_type))
    return;
  for (LayoutObject* child = layout_object->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsAnonymous())
      AddLayoutBox(child);
  }
}

void NearbyDistanceTool::AddLayoutBox(LayoutObject* layout_object) {
  if (layout_object->IsText()) {
    LayoutText* layout_text = ToLayoutText(layout_object);
    for (const auto& text_box : layout_text->GetTextBoxInfo()) {
      LayoutRect text_rect(
          TextFragmentRectInRootFrame(layout_object, text_box));
      boxes_->addItem(RectForLayoutRect(text_rect));
    }
  } else {
    LayoutRect rect(RectInRootFrame(layout_object));
    boxes_->addItem(RectForLayoutRect(rect));
  }
}

void NearbyDistanceTool::Trace(blink::Visitor* visitor) {
  InspectTool::Trace(visitor);
  visitor->Trace(hovered_node_);
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

void ScreenshotTool::Dispatch(const String& message) {
  std::unique_ptr<protocol::Value> value =
      protocol::StringUtil::parseJSON(message);
  if (!value)
    return;
  protocol::ErrorSupport errors;
  std::unique_ptr<protocol::DOM::Rect> box =
      protocol::DOM::Rect::fromValue(value.get(), &errors);
  if (errors.hasErrors())
    return;

  float scale = 1.0f;
  // Capture values in the CSS pixels.
  IntPoint p1(box->getX(), box->getY());
  IntPoint p2(box->getX() + box->getWidth(), box->getY() + box->getHeight());

  if (LocalFrame* frame = overlay_->GetFrame()) {
    float emulation_scale = overlay_->GetFrame()
                                ->GetPage()
                                ->GetChromeClient()
                                .InputEventsScaleForEmulation();
    // Convert from overlay terms into the absolute.
    p1.Scale(1 / emulation_scale, 1 / emulation_scale);
    p2.Scale(1 / emulation_scale, 1 / emulation_scale);

    // Scroll offset in the viewport is in the device pixels, convert before
    // calling ViewportToRootFrame.
    float dip_to_dp = overlay_->WindowToViewportScale();
    p1.Scale(dip_to_dp, dip_to_dp);
    p2.Scale(dip_to_dp, dip_to_dp);

    const VisualViewport& visual_viewport =
        frame->GetPage()->GetVisualViewport();
    p1 = visual_viewport.ViewportToRootFrame(p1);
    p2 = visual_viewport.ViewportToRootFrame(p2);

    scale = frame->GetPage()->PageScaleFactor();
    if (const RootFrameViewport* root_frame_viewport =
            frame->View()->GetRootFrameViewport()) {
      IntSize scroll_offset = FlooredIntSize(
          root_frame_viewport->LayoutViewport().GetScrollOffset());
      // Accunt for the layout scroll (different from viewport scroll offset).
      p1 += scroll_offset;
      p2 += scroll_offset;
    }
  }

  // Go back to dip for the protocol.
  float dp_to_dip = 1.f / overlay_->WindowToViewportScale();
  p1.Scale(dp_to_dip, dp_to_dip);
  p2.Scale(dp_to_dip, dp_to_dip);

  // Points are in device independent pixels (dip) now.
  IntRect rect =
      UnionRectEvenIfEmpty(IntRect(p1, IntSize()), IntRect(p2, IntSize()));
  frontend_->screenshotRequested(protocol::Page::Viewport::create()
                                     .setX(rect.X())
                                     .setY(rect.Y())
                                     .setWidth(rect.Width())
                                     .setHeight(rect.Height())
                                     .setScale(scale)
                                     .build());
}

// PausedInDebuggerTool --------------------------------------------------------

CString PausedInDebuggerTool::GetDataResourceName() {
  return "inspect_tool_paused.html";
}

void PausedInDebuggerTool::Draw(float scale) {
  overlay_->EvaluateInOverlay("drawPausedInDebuggerMessage", message_);
}

void PausedInDebuggerTool::Dispatch(const String& message) {
  if (message == "resume")
    v8_session_->resume();
  else if (message == "stepOver")
    v8_session_->stepOver();
}

}  // namespace blink
