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

#include "web/InspectorOverlay.h"

#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8InspectorOverlayHost.h"
#include "core/dom/Node.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/wtf/AutoReset.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "v8/include/v8.h"
#include "web/ChromeClientImpl.h"
#include "web/PageOverlay.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"

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
  return HoveredNodeForPoint(frame, RoundedIntPoint(transformed_point.position),
                             ignore_pointer_events_none);
}
}  // namespace

class InspectorOverlay::InspectorPageOverlayDelegate final
    : public PageOverlay::Delegate {
 public:
  explicit InspectorPageOverlayDelegate(InspectorOverlay& overlay)
      : overlay_(&overlay) {}

  void PaintPageOverlay(const PageOverlay&,
                        GraphicsContext& graphics_context,
                        const WebSize& web_view_size) const override {
    if (overlay_->IsEmpty())
      return;

    FrameView* view = overlay_->OverlayMainFrame()->View();
    DCHECK(!view->NeedsLayout());
    view->Paint(graphics_context,
                CullRect(IntRect(0, 0, view->Width(), view->Height())));
  }

 private:
  Persistent<InspectorOverlay> overlay_;
};

class InspectorOverlay::InspectorOverlayChromeClient final
    : public EmptyChromeClient {
 public:
  static InspectorOverlayChromeClient* Create(ChromeClient& client,
                                              InspectorOverlay& overlay) {
    return new InspectorOverlayChromeClient(client, overlay);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(client_);
    visitor->Trace(overlay_);
    EmptyChromeClient::Trace(visitor);
  }

  void SetCursor(const Cursor& cursor, LocalFrame* local_root) override {
    ToChromeClientImpl(client_)->SetCursorOverridden(false);
    ToChromeClientImpl(client_)->SetCursor(cursor,
                                           overlay_->frame_impl_->GetFrame());
    ToChromeClientImpl(client_)->SetCursorOverridden(false);
  }

  void SetToolTip(LocalFrame& frame,
                  const String& tooltip,
                  TextDirection direction) override {
    DCHECK_EQ(&frame, overlay_->OverlayMainFrame());
    client_->SetToolTip(*overlay_->frame_impl_->GetFrame(), tooltip, direction);
  }

  void InvalidateRect(const IntRect&) override { overlay_->Invalidate(); }

  void ScheduleAnimation(FrameViewBase* frame_view_base) override {
    if (overlay_->in_layout_)
      return;

    client_->ScheduleAnimation(frame_view_base);
  }

 private:
  InspectorOverlayChromeClient(ChromeClient& client, InspectorOverlay& overlay)
      : client_(&client), overlay_(&overlay) {}

  Member<ChromeClient> client_;
  Member<InspectorOverlay> overlay_;
};

InspectorOverlay::InspectorOverlay(WebLocalFrameImpl* frame_impl)
    : frame_impl_(frame_impl),
      overlay_host_(InspectorOverlayHost::Create()),
      draw_view_size_(false),
      resize_timer_active_(false),
      omit_tooltip_(false),
      timer_(TaskRunnerHelper::Get(TaskType::kUnspecedTimer,
                                   frame_impl->GetFrame()),
             this,
             &InspectorOverlay::OnTimer),
      suspended_(false),
      show_reloading_blanket_(false),
      in_layout_(false),
      needs_update_(false),
      inspect_mode_(InspectorDOMAgent::kNotSearching) {}

InspectorOverlay::~InspectorOverlay() {
  DCHECK(!overlay_page_);
}

DEFINE_TRACE(InspectorOverlay) {
  visitor->Trace(frame_impl_);
  visitor->Trace(highlight_node_);
  visitor->Trace(event_target_node_);
  visitor->Trace(overlay_page_);
  visitor->Trace(overlay_chrome_client_);
  visitor->Trace(overlay_host_);
  visitor->Trace(dom_agent_);
  visitor->Trace(hovered_node_for_inspect_mode_);
}

void InspectorOverlay::Init(v8_inspector::V8InspectorSession* v8_session,
                            InspectorDOMAgent* dom_agent) {
  v8_session_ = v8_session;
  dom_agent_ = dom_agent;
  overlay_host_->SetListener(this);
}

void InspectorOverlay::Invalidate() {
  if (!page_overlay_) {
    page_overlay_ = PageOverlay::Create(
        frame_impl_, WTF::WrapUnique(new InspectorPageOverlayDelegate(*this)));
  }

  page_overlay_->Update();
}

void InspectorOverlay::UpdateAllLifecyclePhases() {
  if (IsEmpty())
    return;

  AutoReset<bool> scoped(&in_layout_, true);
  if (needs_update_) {
    needs_update_ = false;
    RebuildOverlayPage();
  }
  OverlayMainFrame()->View()->UpdateAllLifecyclePhases();
}

bool InspectorOverlay::HandleInputEvent(const WebInputEvent& input_event) {
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
      handled = HandleMousePress();

    if (handled)
      return true;

    if (mouse_event.GetType() == WebInputEvent::kMouseMove) {
      handled = OverlayMainFrame()->GetEventHandler().HandleMouseMoveEvent(
                    mouse_event, TransformWebMouseEventVector(
                                     frame_impl_->GetFrameView(),
                                     std::vector<const WebInputEvent*>())) !=
                WebInputEventResult::kNotHandled;
    }
    if (mouse_event.GetType() == WebInputEvent::kMouseDown)
      handled = OverlayMainFrame()->GetEventHandler().HandleMousePressEvent(
                    mouse_event) != WebInputEventResult::kNotHandled;
    if (mouse_event.GetType() == WebInputEvent::kMouseUp)
      handled = OverlayMainFrame()->GetEventHandler().HandleMouseReleaseEvent(
                    mouse_event) != WebInputEventResult::kNotHandled;
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

void InspectorOverlay::SetPausedInDebuggerMessage(const String& message) {
  paused_in_debugger_message_ = message;
  ScheduleUpdate();
}

void InspectorOverlay::ShowReloadingBlanket() {
  show_reloading_blanket_ = true;
  ScheduleUpdate();
}

void InspectorOverlay::HideReloadingBlanket() {
  if (!show_reloading_blanket_)
    return;
  show_reloading_blanket_ = false;
  if (suspended_)
    ClearInternal();
  else
    ScheduleUpdate();
}

void InspectorOverlay::HideHighlight() {
  highlight_node_.Clear();
  event_target_node_.Clear();
  highlight_quad_.reset();
  ScheduleUpdate();
}

void InspectorOverlay::HighlightNode(
    Node* node,
    const InspectorHighlightConfig& highlight_config,
    bool omit_tooltip) {
  HighlightNode(node, nullptr, highlight_config, omit_tooltip);
}

void InspectorOverlay::HighlightNode(
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

void InspectorOverlay::SetInspectMode(
    InspectorDOMAgent::SearchMode search_mode,
    std::unique_ptr<InspectorHighlightConfig> highlight_config) {
  inspect_mode_ = search_mode;
  ScheduleUpdate();

  if (search_mode != InspectorDOMAgent::kNotSearching) {
    inspect_mode_highlight_config_ = std::move(highlight_config);
  } else {
    hovered_node_for_inspect_mode_.Clear();
    HideHighlight();
  }
}

void InspectorOverlay::HighlightQuad(
    std::unique_ptr<FloatQuad> quad,
    const InspectorHighlightConfig& highlight_config) {
  quad_highlight_config_ = highlight_config;
  highlight_quad_ = std::move(quad);
  omit_tooltip_ = false;
  ScheduleUpdate();
}

bool InspectorOverlay::IsEmpty() {
  if (show_reloading_blanket_)
    return false;
  if (suspended_)
    return true;
  bool has_visible_elements = highlight_node_ || event_target_node_ ||
                              highlight_quad_ ||
                              (resize_timer_active_ && draw_view_size_) ||
                              !paused_in_debugger_message_.IsNull();
  return !has_visible_elements &&
         inspect_mode_ == InspectorDOMAgent::kNotSearching;
}

void InspectorOverlay::ScheduleUpdate() {
  if (IsEmpty()) {
    if (page_overlay_)
      page_overlay_.reset();
    return;
  }
  needs_update_ = true;
  FrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = frame_impl_->GetFrame();
  if (view && frame)
    frame->GetPage()->GetChromeClient().ScheduleAnimation(view);
}

void InspectorOverlay::RebuildOverlayPage() {
  FrameView* view = frame_impl_->GetFrameView();
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
}

static std::unique_ptr<protocol::DictionaryValue> BuildObjectForSize(
    const IntSize& size) {
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  result->setInteger("width", size.Width());
  result->setInteger("height", size.Height());
  return result;
}

void InspectorOverlay::DrawNodeHighlight() {
  if (!highlight_node_)
    return;

  String selectors = node_highlight_config_.selector_list;
  StaticElementList* elements = nullptr;
  DummyExceptionStateForTesting exception_state;
  ContainerNode* query_base = highlight_node_->ContainingShadowRoot();
  if (!query_base)
    query_base = highlight_node_->ownerDocument();
  if (selectors.length())
    elements =
        query_base->QuerySelectorAll(AtomicString(selectors), exception_state);
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
  if (event_target_node_)
    highlight.AppendEventTargetQuads(event_target_node_.Get(),
                                     node_highlight_config_);

  std::unique_ptr<protocol::DictionaryValue> highlight_json =
      highlight.AsProtocolValue();
  EvaluateInOverlay("drawHighlight", std::move(highlight_json));
}

void InspectorOverlay::DrawQuadHighlight() {
  if (!highlight_quad_)
    return;

  InspectorHighlight highlight(WindowToViewportScale());
  highlight.AppendQuad(*highlight_quad_, quad_highlight_config_.content,
                       quad_highlight_config_.content_outline);
  EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

void InspectorOverlay::DrawPausedInDebuggerMessage() {
  if (inspect_mode_ == InspectorDOMAgent::kNotSearching &&
      !paused_in_debugger_message_.IsNull())
    EvaluateInOverlay("drawPausedInDebuggerMessage",
                      paused_in_debugger_message_);
}

void InspectorOverlay::DrawViewSize() {
  if (resize_timer_active_ && draw_view_size_)
    EvaluateInOverlay("drawViewSize", "");
}

float InspectorOverlay::WindowToViewportScale() const {
  LocalFrame* frame = frame_impl_->GetFrame();
  if (!frame)
    return 1.0f;
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(1.0f);
}

Page* InspectorOverlay::OverlayPage() {
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
  // FIXME: http://crbug.com/363843. Inspector should probably create its
  // own graphics layers and attach them to the tree rather than going
  // through some non-composited paint function.
  overlay_settings.SetAcceleratedCompositingEnabled(false);

  LocalFrame* frame =
      LocalFrame::Create(&dummy_local_frame_client, *overlay_page_, 0);
  frame->SetView(FrameView::Create(*frame));
  frame->Init();
  FrameLoader& loader = frame->Loader();
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  const WebData& overlay_page_html_resource =
      Platform::Current()->LoadResource("InspectorOverlayPage.html");
  loader.Load(
      FrameLoadRequest(0, BlankURL(),
                       SubstituteData(overlay_page_html_resource, "text/html",
                                      "UTF-8", KURL(), kForceSynchronousLoad)));
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

#if OS(WIN)
  EvaluateInOverlay("setPlatform", "windows");
#elif OS(MACOSX)
  EvaluateInOverlay("setPlatform", "mac");
#elif OS(POSIX)
  EvaluateInOverlay("setPlatform", "linux");
#endif

  return overlay_page_.Get();
}

LocalFrame* InspectorOverlay::OverlayMainFrame() {
  return ToLocalFrame(OverlayPage()->MainFrame());
}

void InspectorOverlay::Reset(const IntSize& viewport_size,
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

void InspectorOverlay::EvaluateInOverlay(const String& method,
                                         const String& argument) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
  command->pushValue(protocol::StringValue::create(method));
  command->pushValue(protocol::StringValue::create(argument));
  ToLocalFrame(OverlayPage()->MainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" + command->serialize() + ")",
          ScriptController::kExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlay::EvaluateInOverlay(
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
          ScriptController::kExecuteScriptWhenScriptsDisabled);
}

String InspectorOverlay::EvaluateInOverlayForTest(const String& script) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::HandleScope handle_scope(ToIsolate(OverlayMainFrame()));
  v8::Local<v8::Value> string =
      ToLocalFrame(OverlayPage()->MainFrame())
          ->GetScriptController()
          .ExecuteScriptInMainWorldAndReturnValue(
              ScriptSourceCode(script),
              ScriptController::kExecuteScriptWhenScriptsDisabled);
  return ToCoreStringWithUndefinedOrNullCheck(string);
}

void InspectorOverlay::OnTimer(TimerBase*) {
  resize_timer_active_ = false;
  ScheduleUpdate();
}

void InspectorOverlay::ClearInternal() {
  if (overlay_page_) {
    overlay_page_->WillBeDestroyed();
    overlay_page_.Clear();
    overlay_chrome_client_.Clear();
  }
  resize_timer_active_ = false;
  paused_in_debugger_message_ = String();
  inspect_mode_ = InspectorDOMAgent::kNotSearching;
  timer_.Stop();
  HideHighlight();
}

void InspectorOverlay::Clear() {
  ClearInternal();
  v8_session_ = nullptr;
  dom_agent_.Clear();
  overlay_host_->SetListener(nullptr);
}

void InspectorOverlay::OverlayResumed() {
  if (v8_session_)
    v8_session_->resume();
}

void InspectorOverlay::OverlaySteppedOver() {
  if (v8_session_)
    v8_session_->stepOver();
}

void InspectorOverlay::Suspend() {
  if (!suspended_) {
    suspended_ = true;
    ClearInternal();
  }
}

void InspectorOverlay::Resume() {
  suspended_ = false;
}

void InspectorOverlay::PageLayoutInvalidated(bool resized) {
  if (resized && draw_view_size_) {
    resize_timer_active_ = true;
    timer_.StartOneShot(1, BLINK_FROM_HERE);
  }
  ScheduleUpdate();
}

void InspectorOverlay::SetShowViewportSizeOnResize(bool show) {
  draw_view_size_ = show;
}

bool InspectorOverlay::HandleMouseMove(const WebMouseEvent& event) {
  if (!ShouldSearchForNode())
    return false;

  LocalFrame* frame = frame_impl_->GetFrame();
  if (!frame || !frame->View() || frame->ContentLayoutItem().IsNull())
    return false;
  Node* node = HoveredNodeForEvent(
      frame, event, event.GetModifiers() & WebInputEvent::kShiftKey);

  // Do not highlight within user agent shadow root unless requested.
  if (inspect_mode_ != InspectorDOMAgent::kSearchingForUAShadow) {
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
      HideHighlight();
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
    if (dom_agent_)
      dom_agent_->NodeHighlightedInOverlay(node);
    HighlightNode(node, event_target, *inspect_mode_highlight_config_,
                  (event.GetModifiers() &
                   (WebInputEvent::kControlKey | WebInputEvent::kMetaKey)));
  }
  return true;
}

bool InspectorOverlay::HandleMousePress() {
  if (!ShouldSearchForNode())
    return false;

  if (hovered_node_for_inspect_mode_) {
    Inspect(hovered_node_for_inspect_mode_.Get());
    hovered_node_for_inspect_mode_.Clear();
    return true;
  }
  return false;
}

bool InspectorOverlay::HandleGestureEvent(const WebGestureEvent& event) {
  if (!ShouldSearchForNode() || event.GetType() != WebInputEvent::kGestureTap)
    return false;
  Node* node = HoveredNodeForEvent(frame_impl_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    HighlightNode(node, *inspect_mode_highlight_config_, false);
    Inspect(node);
    return true;
  }
  return false;
}

bool InspectorOverlay::HandleTouchEvent(const WebTouchEvent& event) {
  if (!ShouldSearchForNode())
    return false;
  Node* node = HoveredNodeForEvent(frame_impl_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    HighlightNode(node, *inspect_mode_highlight_config_, false);
    Inspect(node);
    return true;
  }
  return false;
}

bool InspectorOverlay::ShouldSearchForNode() {
  return inspect_mode_ != InspectorDOMAgent::kNotSearching;
}

void InspectorOverlay::Inspect(Node* node) {
  if (dom_agent_)
    dom_agent_->Inspect(node);
}

}  // namespace blink
