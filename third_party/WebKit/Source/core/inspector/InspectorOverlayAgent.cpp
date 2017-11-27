/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorOverlayAgent.h"

#include <algorithm>
#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8InspectorOverlayHost.h"
#include "build/build_config.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Node.h"
#include "core/dom/StaticNodeList.h"
#include "core/events/WebInputEventConversion.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/PageOverlay.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/wtf/AutoReset.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebData.h"
#include "v8/include/v8.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace {

namespace OverlayAgentState {
static const char kEnabled[] = "enabled";
static const char kShowDebugBorders[] = "showDebugBorders";
static const char kShowFPSCounter[] = "showFPSCounter";
static const char kShowPaintRects[] = "showPaintRects";
static const char kShowScrollBottleneckRects[] = "showScrollBottleneckRects";
static const char kShowSizeOnResize[] = "showSizeOnResize";
static const char kSuspended[] = "suspended";
static const char kPausedInDebuggerMessage[] = "pausedInDebuggerMessage";
}  // namespace OverlayAgentState

Node* HoveredNodeForPoint(LocalFrame* frame,
                          const IntPoint& point_in_root_frame,
                          bool ignore_pointer_events_none) {
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  if (ignore_pointer_events_none)
    hit_type |= HitTestRequest::kIgnorePointerEventsNone;
  HitTestRequest request(hit_type);
  HitTestResult result(request,
                       frame->View()->RootFrameToContents(point_in_root_frame));
  frame->ContentLayoutItem().HitTest(result);
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
                          const WebTouchEvent& event,
                          bool ignore_pointer_events_none) {
  if (!event.touches_length)
    return nullptr;
  WebTouchPoint transformed_point = event.TouchPointInRootFrame(0);
  return HoveredNodeForPoint(
      frame, RoundedIntPoint(transformed_point.PositionInWidget()),
      ignore_pointer_events_none);
}

bool ParseQuad(std::unique_ptr<protocol::Array<double>> quad_array,
               FloatQuad* quad) {
  const size_t kCoordinatesInQuad = 8;
  if (!quad_array || quad_array->length() != kCoordinatesInQuad)
    return false;
  quad->SetP1(FloatPoint(quad_array->get(0), quad_array->get(1)));
  quad->SetP2(FloatPoint(quad_array->get(2), quad_array->get(3)));
  quad->SetP3(FloatPoint(quad_array->get(4), quad_array->get(5)));
  quad->SetP4(FloatPoint(quad_array->get(6), quad_array->get(7)));
  return true;
}

#if defined(OS_MACOSX)
const int kCtrlOrMeta = WebInputEvent::kMetaKey;
#else
const int kCtrlOrMeta = WebInputEvent::kControlKey;
#endif

}  // namespace

class InspectorOverlayAgent::InspectorPageOverlayDelegate final
    : public PageOverlay::Delegate {
 public:
  explicit InspectorPageOverlayDelegate(InspectorOverlayAgent& overlay)
      : overlay_(&overlay) {}

  void PaintPageOverlay(const PageOverlay&,
                        GraphicsContext& graphics_context,
                        const WebSize& web_view_size) const override {
    if (overlay_->IsEmpty())
      return;

    LocalFrameView* view = overlay_->OverlayMainFrame()->View();
    DCHECK(!view->NeedsLayout());
    view->PaintWithLifecycleUpdate(
        graphics_context, kGlobalPaintNormalPhase,
        CullRect(IntRect(0, 0, view->Width(), view->Height())));
  }

 private:
  Persistent<InspectorOverlayAgent> overlay_;
};

class InspectorOverlayAgent::InspectorOverlayChromeClient final
    : public EmptyChromeClient {
 public:
  static InspectorOverlayChromeClient* Create(ChromeClient& client,
                                              InspectorOverlayAgent& overlay) {
    return new InspectorOverlayChromeClient(client, overlay);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(client_);
    visitor->Trace(overlay_);
    EmptyChromeClient::Trace(visitor);
  }

  void SetCursor(const Cursor& cursor, LocalFrame* local_root) override {
    client_->SetCursorOverridden(false);
    client_->SetCursor(cursor, overlay_->frame_impl_->GetFrame());
    client_->SetCursorOverridden(false);
  }

  void SetToolTip(LocalFrame& frame,
                  const String& tooltip,
                  TextDirection direction) override {
    DCHECK_EQ(&frame, overlay_->OverlayMainFrame());
    client_->SetToolTip(*overlay_->frame_impl_->GetFrame(), tooltip, direction);
  }

  void InvalidateRect(const IntRect&) override { overlay_->Invalidate(); }

  void ScheduleAnimation(const PlatformFrameView* frame_view) override {
    if (overlay_->in_layout_)
      return;

    client_->ScheduleAnimation(frame_view);
  }

 private:
  InspectorOverlayChromeClient(ChromeClient& client,
                               InspectorOverlayAgent& overlay)
      : client_(&client), overlay_(&overlay) {}

  Member<ChromeClient> client_;
  Member<InspectorOverlayAgent> overlay_;
};

InspectorOverlayAgent::InspectorOverlayAgent(
    WebLocalFrameImpl* frame_impl,
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session,
    InspectorDOMAgent* dom_agent)
    : frame_impl_(frame_impl),
      inspected_frames_(inspected_frames),
      enabled_(false),
      draw_view_size_(false),
      resize_timer_active_(false),
      omit_tooltip_(false),
      timer_(frame_impl->GetFrame()->GetTaskRunner(TaskType::kUnspecedTimer),
             this,
             &InspectorOverlayAgent::OnTimer),
      suspended_(false),
      disposed_(false),
      show_reloading_blanket_(false),
      in_layout_(false),
      needs_update_(false),
      v8_session_(v8_session),
      dom_agent_(dom_agent),
      swallow_next_mouse_up_(false),
      inspect_mode_(kNotSearching),
      backend_node_id_to_inspect_(0) {}

InspectorOverlayAgent::~InspectorOverlayAgent() {
  DCHECK(!overlay_page_);
}

void InspectorOverlayAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_impl_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(highlight_node_);
  visitor->Trace(event_target_node_);
  visitor->Trace(overlay_page_);
  visitor->Trace(overlay_chrome_client_);
  visitor->Trace(overlay_host_);
  visitor->Trace(dom_agent_);
  visitor->Trace(hovered_node_for_inspect_mode_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorOverlayAgent::Restore() {
  if (state_->booleanProperty(OverlayAgentState::kEnabled, false))
    enabled_ = true;
  setShowDebugBorders(
      state_->booleanProperty(OverlayAgentState::kShowDebugBorders, false));
  setShowFPSCounter(
      state_->booleanProperty(OverlayAgentState::kShowFPSCounter, false));
  setShowPaintRects(
      state_->booleanProperty(OverlayAgentState::kShowPaintRects, false));
  setShowScrollBottleneckRects(state_->booleanProperty(
      OverlayAgentState::kShowScrollBottleneckRects, false));
  setShowViewportSizeOnResize(
      state_->booleanProperty(OverlayAgentState::kShowSizeOnResize, false));
  String message;
  if (state_->getString(OverlayAgentState::kPausedInDebuggerMessage, &message))
    setPausedInDebuggerMessage(message);
  setSuspended(state_->booleanProperty(OverlayAgentState::kSuspended, false));
}

void InspectorOverlayAgent::Dispose() {
  InspectorBaseAgent::Dispose();
  show_reloading_blanket_ = false;
  disposed_ = true;
  ClearInternal();
}

Response InspectorOverlayAgent::enable() {
  if (!dom_agent_->Enabled())
    return Response::Error("DOM should be enabled first");
  state_->setBoolean(OverlayAgentState::kEnabled, true);
  enabled_ = true;
  if (backend_node_id_to_inspect_)
    GetFrontend()->inspectNodeRequested(backend_node_id_to_inspect_);
  backend_node_id_to_inspect_ = 0;
  return Response::OK();
}

Response InspectorOverlayAgent::disable() {
  state_->setBoolean(OverlayAgentState::kEnabled, false);
  enabled_ = false;
  setShowDebugBorders(false);
  setShowFPSCounter(false);
  setShowPaintRects(false);
  setShowScrollBottleneckRects(false);
  setShowViewportSizeOnResize(false);
  setPausedInDebuggerMessage(String());
  setSuspended(false);
  SetSearchingForNode(kNotSearching,
                      Maybe<protocol::Overlay::HighlightConfig>());
  return Response::OK();
}

Response InspectorOverlayAgent::setShowDebugBorders(bool show) {
  state_->setBoolean(OverlayAgentState::kShowDebugBorders, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  frame_impl_->ViewImpl()->SetShowDebugBorders(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowFPSCounter(bool show) {
  state_->setBoolean(OverlayAgentState::kShowFPSCounter, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  frame_impl_->ViewImpl()->SetShowFPSCounter(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowPaintRects(bool show) {
  state_->setBoolean(OverlayAgentState::kShowPaintRects, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  frame_impl_->ViewImpl()->SetShowPaintRects(show);
  if (!show && frame_impl_->GetFrameView())
    frame_impl_->GetFrameView()->Invalidate();
  return Response::OK();
}

Response InspectorOverlayAgent::setShowScrollBottleneckRects(bool show) {
  state_->setBoolean(OverlayAgentState::kShowScrollBottleneckRects, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  frame_impl_->ViewImpl()->SetShowScrollBottleneckRects(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowViewportSizeOnResize(bool show) {
  state_->setBoolean(OverlayAgentState::kShowSizeOnResize, show);
  draw_view_size_ = show;
  return Response::OK();
}

Response InspectorOverlayAgent::setPausedInDebuggerMessage(
    Maybe<String> message) {
  String just_message = message.fromMaybe(String());
  state_->setString(OverlayAgentState::kPausedInDebuggerMessage, just_message);
  paused_in_debugger_message_ = just_message;
  ScheduleUpdate();
  return Response::OK();
}

Response InspectorOverlayAgent::setSuspended(bool suspended) {
  state_->setBoolean(OverlayAgentState::kSuspended, suspended);
  if (suspended && !suspended_ && !show_reloading_blanket_)
    ClearInternal();
  suspended_ = suspended;
  return Response::OK();
}

Response InspectorOverlayAgent::setInspectMode(
    const String& mode,
    Maybe<protocol::Overlay::HighlightConfig> highlight_config) {
  SearchMode search_mode;
  if (mode == protocol::Overlay::InspectModeEnum::SearchForNode) {
    search_mode = kSearchingForNormal;
  } else if (mode == protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM) {
    search_mode = kSearchingForUAShadow;
  } else if (mode == protocol::Overlay::InspectModeEnum::None) {
    search_mode = kNotSearching;
  } else {
    return Response::Error(
        String("Unknown mode \"" + mode + "\" was provided."));
  }

  if (search_mode != kNotSearching) {
    Response response = dom_agent_->PushDocumentUponHandlelessOperation();
    if (!response.isSuccess())
      return response;
  }

  return SetSearchingForNode(search_mode, std::move(highlight_config));
}

Response InspectorOverlayAgent::highlightRect(
    int x,
    int y,
    int width,
    int height,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<FloatQuad> quad =
      WTF::WrapUnique(new FloatQuad(FloatRect(x, y, width, height)));
  InnerHighlightQuad(std::move(quad), std::move(color),
                     std::move(outline_color));
  return Response::OK();
}

Response InspectorOverlayAgent::highlightQuad(
    std::unique_ptr<protocol::Array<double>> quad_array,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<FloatQuad> quad = std::make_unique<FloatQuad>();
  if (!ParseQuad(std::move(quad_array), quad.get()))
    return Response::Error("Invalid Quad format");
  InnerHighlightQuad(std::move(quad), std::move(color),
                     std::move(outline_color));
  return Response::OK();
}

Response InspectorOverlayAgent::highlightNode(
    std::unique_ptr<protocol::Overlay::HighlightConfig>
        highlight_inspector_object,
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id) {
  Node* node = nullptr;
  Response response =
      dom_agent_->AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.isSuccess())
    return response;

  std::unique_ptr<InspectorHighlightConfig> highlight_config;
  response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &highlight_config);
  if (!response.isSuccess())
    return response;

  InnerHighlightNode(node, nullptr, *highlight_config, false);
  return Response::OK();
}

Response InspectorOverlayAgent::highlightFrame(
    const String& frame_id,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  // FIXME: Inspector doesn't currently work cross process.
  if (frame && frame->DeprecatedLocalOwner()) {
    std::unique_ptr<InspectorHighlightConfig> highlight_config =
        std::make_unique<InspectorHighlightConfig>();
    highlight_config->show_info = true;  // Always show tooltips for frames.
    highlight_config->content =
        InspectorDOMAgent::ParseColor(color.fromMaybe(nullptr));
    highlight_config->content_outline =
        InspectorDOMAgent::ParseColor(outline_color.fromMaybe(nullptr));
    InnerHighlightNode(frame->DeprecatedLocalOwner(), nullptr,
                       *highlight_config, false);
  }
  return Response::OK();
}

Response InspectorOverlayAgent::hideHighlight() {
  InnerHideHighlight();
  return Response::OK();
}

Response InspectorOverlayAgent::getHighlightObjectForTest(
    int node_id,
    std::unique_ptr<protocol::DictionaryValue>* result) {
  Node* node = nullptr;
  Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  InspectorHighlight highlight(node, InspectorHighlight::DefaultConfig(), true);
  *result = highlight.AsProtocolValue();
  return Response::OK();
}

void InspectorOverlayAgent::Invalidate() {
  if (IsEmpty())
    return;

  if (!page_overlay_) {
    page_overlay_ = PageOverlay::Create(
        frame_impl_, WTF::WrapUnique(new InspectorPageOverlayDelegate(*this)));
  }

  page_overlay_->Update();
}

void InspectorOverlayAgent::PaintOverlay() {
  UpdateAllLifecyclePhases();
  // TODO(chrishtr): integrate paint into the overlay's lifecycle.
  if (page_overlay_ && page_overlay_->GetGraphicsLayer())
    page_overlay_->GetGraphicsLayer()->Paint(nullptr);
}

void InspectorOverlayAgent::LayoutOverlay() {
  if (page_overlay_)
    page_overlay_->Update();
}

bool InspectorOverlayAgent::IsInspectorLayer(GraphicsLayer* layer) {
  return page_overlay_ && page_overlay_->GetGraphicsLayer() == layer;
}

void InspectorOverlayAgent::UpdateAllLifecyclePhases() {
  if (IsEmpty())
    return;

  AutoReset<bool> scoped(&in_layout_, true);
  if (needs_update_) {
    needs_update_ = false;
    RebuildOverlayPage();
  }
  OverlayMainFrame()->View()->UpdateAllLifecyclePhases();
}

bool InspectorOverlayAgent::HandleInputEvent(const WebInputEvent& input_event) {
  bool handled = false;

  if (IsEmpty())
    return false;

  if (input_event.GetType() == WebInputEvent::kGestureTap) {
    // We only have a use for gesture tap.
    WebGestureEvent transformed_event = TransformWebGestureEvent(
        frame_impl_->GetFrameView(),
        static_cast<const WebGestureEvent&>(input_event));
    handled = HandleGestureEvent(transformed_event);
    if (handled)
      return true;

    OverlayMainFrame()->GetEventHandler().HandleGestureEvent(transformed_event);
  }
  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    WebMouseEvent mouse_event =
        TransformWebMouseEvent(frame_impl_->GetFrameView(),
                               static_cast<const WebMouseEvent&>(input_event));

    if (mouse_event.GetType() == WebInputEvent::kMouseMove)
      handled = HandleMouseMove(mouse_event);
    else if (mouse_event.GetType() == WebInputEvent::kMouseDown)
      handled = HandleMouseDown(mouse_event);
    else if (mouse_event.GetType() == WebInputEvent::kMouseUp)
      handled = HandleMouseUp(mouse_event);

    if (handled)
      return true;

    if (mouse_event.GetType() == WebInputEvent::kMouseMove) {
      handled = OverlayMainFrame()->GetEventHandler().HandleMouseMoveEvent(
                    mouse_event, TransformWebMouseEventVector(
                                     frame_impl_->GetFrameView(),
                                     std::vector<const WebInputEvent*>())) !=
                WebInputEventResult::kNotHandled;
    }
    if (mouse_event.GetType() == WebInputEvent::kMouseDown) {
      handled = OverlayMainFrame()->GetEventHandler().HandleMousePressEvent(
                    mouse_event) != WebInputEventResult::kNotHandled;
    }
    if (mouse_event.GetType() == WebInputEvent::kMouseUp) {
      handled = OverlayMainFrame()->GetEventHandler().HandleMouseReleaseEvent(
                    mouse_event) != WebInputEventResult::kNotHandled;
    }
  }

  if (WebInputEvent::IsTouchEventType(input_event.GetType())) {
    WebTouchEvent transformed_event =
        TransformWebTouchEvent(frame_impl_->GetFrameView(),
                               static_cast<const WebTouchEvent&>(input_event));
    handled = HandleTouchEvent(transformed_event);
    if (handled)
      return true;
    OverlayMainFrame()->GetEventHandler().HandleTouchEvent(
        transformed_event, Vector<WebTouchEvent>());
  }
  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
    OverlayMainFrame()->GetEventHandler().KeyEvent(
        static_cast<const WebKeyboardEvent&>(input_event));
  }

  if (input_event.GetType() == WebInputEvent::kMouseWheel) {
    WebMouseWheelEvent transformed_event = TransformWebMouseWheelEvent(
        frame_impl_->GetFrameView(),
        static_cast<const WebMouseWheelEvent&>(input_event));
    handled = OverlayMainFrame()->GetEventHandler().HandleWheelEvent(
                  transformed_event) != WebInputEventResult::kNotHandled;
  }

  return handled;
}

void InspectorOverlayAgent::ShowReloadingBlanket() {
  show_reloading_blanket_ = true;
  ScheduleUpdate();
}

void InspectorOverlayAgent::HideReloadingBlanket() {
  if (!show_reloading_blanket_)
    return;
  show_reloading_blanket_ = false;
  if (suspended_)
    ClearInternal();
  else
    ScheduleUpdate();
}

void InspectorOverlayAgent::InnerHideHighlight() {
  highlight_node_.Clear();
  event_target_node_.Clear();
  highlight_quad_.reset();
  ScheduleUpdate();
}

void InspectorOverlayAgent::InnerHighlightNode(
    Node* node,
    Node* event_target,
    const InspectorHighlightConfig& highlight_config,
    bool omit_tooltip) {
  node_highlight_config_ = highlight_config;
  highlight_node_ = node;
  event_target_node_ = event_target;
  omit_tooltip_ = omit_tooltip;
  ScheduleUpdate();
}

void InspectorOverlayAgent::InnerHighlightQuad(
    std::unique_ptr<FloatQuad> quad,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  quad_content_color_ = InspectorDOMAgent::ParseColor(color.fromMaybe(nullptr));
  quad_content_outline_color_ =
      InspectorDOMAgent::ParseColor(outline_color.fromMaybe(nullptr));
  highlight_quad_ = std::move(quad);
  omit_tooltip_ = false;
  ScheduleUpdate();
}

bool InspectorOverlayAgent::IsEmpty() {
  if (disposed_)
    return true;
  if (show_reloading_blanket_)
    return false;
  if (suspended_)
    return true;
  bool has_visible_elements = highlight_node_ || event_target_node_ ||
                              highlight_quad_ ||
                              (resize_timer_active_ && draw_view_size_) ||
                              !paused_in_debugger_message_.IsNull();
  return !has_visible_elements && inspect_mode_ == kNotSearching;
}

void InspectorOverlayAgent::ScheduleUpdate() {
  if (IsEmpty()) {
    if (page_overlay_)
      page_overlay_.reset();
    return;
  }
  needs_update_ = true;
  LocalFrame* frame = frame_impl_->GetFrame();
  if (frame) {
    frame->GetPage()->GetChromeClient().ScheduleAnimation(frame->View());
  }
}

void InspectorOverlayAgent::RebuildOverlayPage() {
  LocalFrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = frame_impl_->GetFrame();
  if (!view || !frame)
    return;

  IntRect visible_rect_in_document =
      view->GetScrollableArea()->VisibleContentRect();
  IntSize viewport_size = frame->GetPage()->GetVisualViewport().Size();
  OverlayMainFrame()->View()->Resize(viewport_size);
  OverlayPage()->GetVisualViewport().SetSize(viewport_size);
  OverlayMainFrame()->SetPageZoomFactor(WindowToViewportScale());

  Reset(viewport_size, visible_rect_in_document.Location());

  if (show_reloading_blanket_) {
    EvaluateInOverlay("showReloadingBlanket", "");
    return;
  }
  DrawNodeHighlight();
  DrawQuadHighlight();
  DrawPausedInDebuggerMessage();
  DrawViewSize();
  DrawScreenshotBorder();
}

static std::unique_ptr<protocol::DictionaryValue> BuildObjectForSize(
    const IntSize& size) {
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  result->setInteger("width", size.Width());
  result->setInteger("height", size.Height());
  return result;
}

void InspectorOverlayAgent::DrawNodeHighlight() {
  if (!highlight_node_)
    return;

  String selectors = node_highlight_config_.selector_list;
  StaticElementList* elements = nullptr;
  DummyExceptionStateForTesting exception_state;
  ContainerNode* query_base = highlight_node_->ContainingShadowRoot();
  if (!query_base)
    query_base = highlight_node_->ownerDocument();
  if (selectors.length()) {
    elements =
        query_base->QuerySelectorAll(AtomicString(selectors), exception_state);
  }
  if (elements && !exception_state.HadException()) {
    for (unsigned i = 0; i < elements->length(); ++i) {
      Element* element = elements->item(i);
      InspectorHighlight highlight(element, node_highlight_config_, false);
      std::unique_ptr<protocol::DictionaryValue> highlight_json =
          highlight.AsProtocolValue();
      EvaluateInOverlay("drawHighlight", std::move(highlight_json));
    }
  }

  bool append_element_info =
      highlight_node_->IsElementNode() && !omit_tooltip_ &&
      node_highlight_config_.show_info && highlight_node_->GetLayoutObject() &&
      highlight_node_->GetDocument().GetFrame();
  InspectorHighlight highlight(highlight_node_.Get(), node_highlight_config_,
                               append_element_info);
  if (event_target_node_) {
    highlight.AppendEventTargetQuads(event_target_node_.Get(),
                                     node_highlight_config_);
  }

  std::unique_ptr<protocol::DictionaryValue> highlight_json =
      highlight.AsProtocolValue();
  EvaluateInOverlay("drawHighlight", std::move(highlight_json));
}

void InspectorOverlayAgent::DrawQuadHighlight() {
  if (!highlight_quad_)
    return;

  InspectorHighlight highlight(WindowToViewportScale());
  highlight.AppendQuad(*highlight_quad_, quad_content_color_,
                       quad_content_outline_color_);
  EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

void InspectorOverlayAgent::DrawPausedInDebuggerMessage() {
  if (inspect_mode_ == kNotSearching && !paused_in_debugger_message_.IsNull()) {
    EvaluateInOverlay("drawPausedInDebuggerMessage",
                      paused_in_debugger_message_);
  }
}

void InspectorOverlayAgent::DrawViewSize() {
  if (resize_timer_active_ && draw_view_size_)
    EvaluateInOverlay("drawViewSize", "");
}

void InspectorOverlayAgent::DrawScreenshotBorder() {
  if (!screenshot_mode_)
    return;
  VisualViewport& visual_viewport =
      frame_impl_->GetFrame()->GetPage()->GetVisualViewport();
  IntPoint p1 = visual_viewport.RootFrameToViewport(screenshot_anchor_);
  IntPoint p2 = visual_viewport.RootFrameToViewport(screenshot_position_);
  std::unique_ptr<protocol::DictionaryValue> data =
      protocol::DictionaryValue::create();
  data->setInteger("x1", p1.X());
  data->setInteger("y1", p1.Y());
  data->setInteger("x2", p2.X());
  data->setInteger("y2", p2.Y());
  EvaluateInOverlay("drawScreenshotBorder", std::move(data));
}

float InspectorOverlayAgent::WindowToViewportScale() const {
  LocalFrame* frame = frame_impl_->GetFrame();
  if (!frame)
    return 1.0f;
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(1.0f);
}

Page* InspectorOverlayAgent::OverlayPage() {
  if (overlay_page_)
    return overlay_page_.Get();

  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  DEFINE_STATIC_LOCAL(LocalFrameClient, dummy_local_frame_client,
                      (EmptyLocalFrameClient::Create()));
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  DCHECK(!overlay_chrome_client_);
  overlay_chrome_client_ = InspectorOverlayChromeClient::Create(
      frame_impl_->GetFrame()->GetPage()->GetChromeClient(), *this);
  page_clients.chrome_client = overlay_chrome_client_.Get();
  overlay_page_ = Page::Create(page_clients);
  overlay_host_ = new InspectorOverlayHost(this);

  Settings& settings = frame_impl_->GetFrame()->GetPage()->GetSettings();
  Settings& overlay_settings = overlay_page_->GetSettings();

  overlay_settings.GetGenericFontFamilySettings().UpdateStandard(
      settings.GetGenericFontFamilySettings().Standard());
  overlay_settings.GetGenericFontFamilySettings().UpdateSerif(
      settings.GetGenericFontFamilySettings().Serif());
  overlay_settings.GetGenericFontFamilySettings().UpdateSansSerif(
      settings.GetGenericFontFamilySettings().SansSerif());
  overlay_settings.GetGenericFontFamilySettings().UpdateCursive(
      settings.GetGenericFontFamilySettings().Cursive());
  overlay_settings.GetGenericFontFamilySettings().UpdateFantasy(
      settings.GetGenericFontFamilySettings().Fantasy());
  overlay_settings.GetGenericFontFamilySettings().UpdatePictograph(
      settings.GetGenericFontFamilySettings().Pictograph());
  overlay_settings.SetMinimumFontSize(settings.GetMinimumFontSize());
  overlay_settings.SetMinimumLogicalFontSize(
      settings.GetMinimumLogicalFontSize());
  overlay_settings.SetScriptEnabled(true);
  overlay_settings.SetPluginsEnabled(false);
  overlay_settings.SetLoadsImagesAutomatically(true);
  overlay_settings.SetForceDisplayList2dCanvasEnabled(true);
  // FIXME: http://crbug.com/363843. Inspector should probably create its
  // own graphics layers and attach them to the tree rather than going
  // through some non-composited paint function.
  overlay_settings.SetAcceleratedCompositingEnabled(false);

  LocalFrame* frame =
      LocalFrame::Create(&dummy_local_frame_client, *overlay_page_, nullptr);
  frame->SetView(LocalFrameView::Create(*frame));
  frame->Init();
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  const WebData& overlay_page_html_resource =
      Platform::Current()->GetDataResource("InspectorOverlayPage.html");
  frame->ForceSynchronousDocumentInstall("text/html",
                                         overlay_page_html_resource);
  v8::Isolate* isolate = ToIsolate(frame);
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  DCHECK(script_state);
  ScriptState::Scope scope(script_state);
  v8::Local<v8::Object> global = script_state->GetContext()->Global();
  v8::Local<v8::Value> overlay_host_obj =
      ToV8(overlay_host_.Get(), global, isolate);
  DCHECK(!overlay_host_obj.IsEmpty());
  global
      ->Set(script_state->GetContext(),
            V8AtomicString(isolate, "InspectorOverlayHost"), overlay_host_obj)
      .ToChecked();

#if defined(OS_WIN)
  EvaluateInOverlay("setPlatform", "windows");
#elif defined(OS_MACOSX)
  EvaluateInOverlay("setPlatform", "mac");
#elif defined(OS_POSIX)
  EvaluateInOverlay("setPlatform", "linux");
#endif

  return overlay_page_.Get();
}

LocalFrame* InspectorOverlayAgent::OverlayMainFrame() {
  return ToLocalFrame(OverlayPage()->MainFrame());
}

void InspectorOverlayAgent::Reset(const IntSize& viewport_size,
                                  const IntPoint& document_scroll_offset) {
  std::unique_ptr<protocol::DictionaryValue> reset_data =
      protocol::DictionaryValue::create();
  reset_data->setDouble(
      "deviceScaleFactor",
      frame_impl_->GetFrame()->GetPage()->DeviceScaleFactorDeprecated());
  reset_data->setDouble(
      "pageScaleFactor",
      frame_impl_->GetFrame()->GetPage()->GetVisualViewport().Scale());

  IntRect viewport_in_screen =
      frame_impl_->GetFrame()->GetPage()->GetChromeClient().ViewportToScreen(
          IntRect(IntPoint(), viewport_size), frame_impl_->GetFrame()->View());
  reset_data->setObject("viewportSize",
                        BuildObjectForSize(viewport_in_screen.Size()));

  // The zoom factor in the overlay frame already has been multiplied by the
  // window to viewport scale (aka device scale factor), so cancel it.
  reset_data->setDouble(
      "pageZoomFactor",
      frame_impl_->GetFrame()->PageZoomFactor() / WindowToViewportScale());

  reset_data->setInteger("scrollX", document_scroll_offset.X());
  reset_data->setInteger("scrollY", document_scroll_offset.Y());
  EvaluateInOverlay("reset", std::move(reset_data));
}

void InspectorOverlayAgent::EvaluateInOverlay(const String& method,
                                              const String& argument) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
  command->pushValue(protocol::StringValue::create(method));
  command->pushValue(protocol::StringValue::create(argument));
  ToLocalFrame(OverlayPage()->MainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" + command->serialize() + ")",
          ScriptSourceLocationType::kInspector,
          ScriptController::kExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlayAgent::EvaluateInOverlay(
    const String& method,
    std::unique_ptr<protocol::Value> argument) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
  command->pushValue(protocol::StringValue::create(method));
  command->pushValue(std::move(argument));
  ToLocalFrame(OverlayPage()->MainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" + command->serialize() + ")",
          ScriptSourceLocationType::kInspector,
          ScriptController::kExecuteScriptWhenScriptsDisabled);
}

String InspectorOverlayAgent::EvaluateInOverlayForTest(const String& script) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::HandleScope handle_scope(ToIsolate(OverlayMainFrame()));
  v8::Local<v8::Value> string =
      ToLocalFrame(OverlayPage()->MainFrame())
          ->GetScriptController()
          .ExecuteScriptInMainWorldAndReturnValue(
              ScriptSourceCode(script, ScriptSourceLocationType::kInspector),
              ScriptFetchOptions(),
              ScriptController::kExecuteScriptWhenScriptsDisabled);
  return ToCoreStringWithUndefinedOrNullCheck(string);
}

void InspectorOverlayAgent::OnTimer(TimerBase*) {
  resize_timer_active_ = false;
  ScheduleUpdate();
}

void InspectorOverlayAgent::ClearInternal() {
  if (overlay_page_) {
    overlay_page_->WillBeDestroyed();
    overlay_page_.Clear();
    overlay_chrome_client_.Clear();
    overlay_host_->ClearListener();
    overlay_host_.Clear();
  }
  resize_timer_active_ = false;
  paused_in_debugger_message_ = String();
  inspect_mode_ = kNotSearching;
  screenshot_mode_ = false;
  timer_.Stop();
  page_overlay_.reset();
  InnerHideHighlight();
}

void InspectorOverlayAgent::OverlayResumed() {
  if (v8_session_)
    v8_session_->resume();
}

void InspectorOverlayAgent::OverlaySteppedOver() {
  if (v8_session_)
    v8_session_->stepOver();
}

void InspectorOverlayAgent::PageLayoutInvalidated(bool resized) {
  if (resized && draw_view_size_) {
    resize_timer_active_ = true;
    timer_.StartOneShot(TimeDelta::FromSeconds(1), BLINK_FROM_HERE);
  }
  ScheduleUpdate();
}

bool InspectorOverlayAgent::HandleMouseMove(const WebMouseEvent& event) {
  if (!ShouldSearchForNode())
    return false;

  if (event.GetModifiers() & kCtrlOrMeta) {
    InnerHideHighlight();
    hovered_node_for_inspect_mode_.Clear();
    if (screenshot_mode_) {
      screenshot_position_ = RoundedIntPoint(event.PositionInRootFrame());
      ScheduleUpdate();
    }
    return true;
  }

  if (screenshot_mode_) {
    screenshot_mode_ = false;
    ScheduleUpdate();
  }

  LocalFrame* frame = frame_impl_->GetFrame();
  if (!frame || !frame->View() || frame->ContentLayoutItem().IsNull())
    return false;
  Node* node = HoveredNodeForEvent(
      frame, event, event.GetModifiers() & WebInputEvent::kShiftKey);

  // Do not highlight within user agent shadow root unless requested.
  if (inspect_mode_ != kSearchingForUAShadow) {
    ShadowRoot* shadow_root = InspectorDOMAgent::UserAgentShadowRoot(node);
    if (shadow_root)
      node = &shadow_root->host();
  }

  // Shadow roots don't have boxes - use host element instead.
  if (node && node->IsShadowRoot())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return true;

  if (node->IsFrameOwnerElement()) {
    HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(node);
    if (frame_owner->ContentFrame() &&
        !frame_owner->ContentFrame()->IsLocalFrame()) {
      // Do not consume event so that remote frame can handle it.
      InnerHideHighlight();
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
    InnerHighlightNode(node, event_target, *inspect_mode_highlight_config_,
                       omit_tooltip);
  }
  return true;
}

bool InspectorOverlayAgent::HandleMouseDown(const WebMouseEvent& event) {
  swallow_next_mouse_up_ = false;
  screenshot_mode_ = false;
  if (!ShouldSearchForNode())
    return false;

  if ((event.GetModifiers() & kCtrlOrMeta) &&
      (event.GetModifiers() & WebInputEvent::kLeftButtonDown)) {
    InnerHideHighlight();
    hovered_node_for_inspect_mode_.Clear();
    screenshot_mode_ = true;
    screenshot_anchor_ = RoundedIntPoint(event.PositionInRootFrame());
    screenshot_position_ = screenshot_anchor_;
    ScheduleUpdate();
    return true;
  }

  if (hovered_node_for_inspect_mode_) {
    swallow_next_mouse_up_ = true;
    Inspect(hovered_node_for_inspect_mode_.Get());
    hovered_node_for_inspect_mode_.Clear();
    return true;
  }
  return false;
}

bool InspectorOverlayAgent::HandleMouseUp(const WebMouseEvent& event) {
  if (screenshot_mode_) {
    screenshot_mode_ = false;
    float scale = 1.0f;
    IntPoint p1 = screenshot_anchor_;
    IntPoint p2 = screenshot_position_;
    if (LocalFrame* frame = frame_impl_->GetFrame()) {
      scale = frame->GetPage()->PageScaleFactor();
      p1 = frame->View()->RootFrameToContents(p1);
      p2 = frame->View()->RootFrameToContents(p2);
    }
    int min_x = std::min(p1.X(), p2.X());
    int max_x = std::max(p1.X(), p2.X());
    int min_y = std::min(p1.Y(), p2.Y());
    int max_y = std::max(p1.Y(), p2.Y());
    GetFrontend()->screenshotRequested(protocol::Page::Viewport::create()
                                           .setX(min_x)
                                           .setY(min_y)
                                           .setWidth(max_x - min_x)
                                           .setHeight(max_y - min_y)
                                           .setScale(scale)
                                           .build());
    return true;
  }
  if (swallow_next_mouse_up_) {
    swallow_next_mouse_up_ = false;
    return true;
  }
  return false;
}

bool InspectorOverlayAgent::HandleGestureEvent(const WebGestureEvent& event) {
  if (!ShouldSearchForNode() || event.GetType() != WebInputEvent::kGestureTap)
    return false;
  Node* node = HoveredNodeForEvent(frame_impl_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    InnerHighlightNode(node, nullptr, *inspect_mode_highlight_config_, false);
    Inspect(node);
    return true;
  }
  return false;
}

bool InspectorOverlayAgent::HandleTouchEvent(const WebTouchEvent& event) {
  if (!ShouldSearchForNode())
    return false;
  Node* node = HoveredNodeForEvent(frame_impl_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    InnerHighlightNode(node, nullptr, *inspect_mode_highlight_config_, false);
    Inspect(node);
    return true;
  }
  return false;
}

Response InspectorOverlayAgent::CompositingEnabled() {
  bool main_frame = frame_impl_->ViewImpl() && !frame_impl_->Parent();
  if (!main_frame || !frame_impl_->ViewImpl()
                          ->GetPage()
                          ->GetSettings()
                          .GetAcceleratedCompositingEnabled())
    return Response::Error("Compositing mode is not supported");
  return Response::OK();
}

bool InspectorOverlayAgent::ShouldSearchForNode() {
  return inspect_mode_ != kNotSearching;
}

void InspectorOverlayAgent::Inspect(Node* inspected_node) {
  if (!inspected_node)
    return;

  Node* node = inspected_node;
  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();
  if (!node)
    return;

  int backend_node_id = DOMNodeIds::IdForNode(node);
  if (!enabled_) {
    backend_node_id_to_inspect_ = backend_node_id;
    return;
  }

  GetFrontend()->inspectNodeRequested(backend_node_id);
}

void InspectorOverlayAgent::NodeHighlightRequested(Node* node) {
  if (!enabled_)
    return;

  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return;

  int node_id = dom_agent_->PushNodePathToFrontend(node);
  GetFrontend()->nodeHighlightRequested(node_id);
}

Response InspectorOverlayAgent::SetSearchingForNode(
    SearchMode search_mode,
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object) {
  if (search_mode == kNotSearching) {
    inspect_mode_ = search_mode;
    screenshot_mode_ = false;
    ScheduleUpdate();
    hovered_node_for_inspect_mode_.Clear();
    InnerHideHighlight();
    return Response::OK();
  }

  std::unique_ptr<InspectorHighlightConfig> config;
  Response response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &config);
  if (!response.isSuccess())
    return response;
  inspect_mode_ = search_mode;
  inspect_mode_highlight_config_ = std::move(config);
  ScheduleUpdate();
  return Response::OK();
}

Response InspectorOverlayAgent::HighlightConfigFromInspectorObject(
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object,
    std::unique_ptr<InspectorHighlightConfig>* out_config) {
  if (!highlight_inspector_object.isJust()) {
    return Response::Error(
        "Internal error: highlight configuration parameter is missing");
  }

  protocol::Overlay::HighlightConfig* config =
      highlight_inspector_object.fromJust();
  std::unique_ptr<InspectorHighlightConfig> highlight_config =
      std::make_unique<InspectorHighlightConfig>();
  highlight_config->show_info = config->getShowInfo(false);
  highlight_config->show_rulers = config->getShowRulers(false);
  highlight_config->show_extension_lines = config->getShowExtensionLines(false);
  highlight_config->display_as_material = config->getDisplayAsMaterial(false);
  highlight_config->content =
      InspectorDOMAgent::ParseColor(config->getContentColor(nullptr));
  highlight_config->padding =
      InspectorDOMAgent::ParseColor(config->getPaddingColor(nullptr));
  highlight_config->border =
      InspectorDOMAgent::ParseColor(config->getBorderColor(nullptr));
  highlight_config->margin =
      InspectorDOMAgent::ParseColor(config->getMarginColor(nullptr));
  highlight_config->event_target =
      InspectorDOMAgent::ParseColor(config->getEventTargetColor(nullptr));
  highlight_config->shape =
      InspectorDOMAgent::ParseColor(config->getShapeColor(nullptr));
  highlight_config->shape_margin =
      InspectorDOMAgent::ParseColor(config->getShapeMarginColor(nullptr));
  highlight_config->css_grid =
      InspectorDOMAgent::ParseColor(config->getCssGridColor(nullptr));
  highlight_config->selector_list = config->getSelectorList("");

  *out_config = std::move(highlight_config);
  return Response::OK();
}

}  // namespace blink
