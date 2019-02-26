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

#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_inspector_overlay_host.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_host.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "v8/include/v8.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

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

}  // namespace

class InspectorOverlayAgent::InspectorPageOverlayDelegate final
    : public FrameOverlay::Delegate,
      public cc::ContentLayerClient {
 public:
  explicit InspectorPageOverlayDelegate(InspectorOverlayAgent& overlay)
      : overlay_(&overlay) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      layer_ = cc::PictureLayer::Create(this);
      layer_->SetIsDrawable(true);
    }
  }
  ~InspectorPageOverlayDelegate() override {
    if (layer_)
      layer_->ClearClient();
  }

  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const IntSize&) const override {
    if (overlay_->IsEmpty())
      return;

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      layer_->SetBounds(gfx::Size(frame_overlay.Size()));
      RecordForeignLayer(graphics_context,
                         DisplayItem::kForeignLayerDevToolsOverlay, layer_,
                         PropertyTreeState::Root());
      return;
    }

    LocalFrameView* view = overlay_->OverlayMainFrame()->View();
    view->PaintOutsideOfLifecycle(graphics_context, kGlobalPaintNormalPhase);
  }

  void Invalidate() override {
    if (layer_)
      layer_->SetNeedsDisplay();
  }

  const cc::Layer* GetLayer() const { return layer_.get(); }

 private:
  // cc::ContentLayerClient implementation
  gfx::Rect PaintableRegion() override { return gfx::Rect(layer_->bounds()); }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override {
    auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
    display_list->StartPaint();
    display_list->push<cc::DrawRecordOp>(
        overlay_->OverlayMainFrame()->View()->GetPaintRecord());
    display_list->EndPaintOfUnpaired(PaintableRegion());
    display_list->Finalize();
    return display_list;
  }

  Persistent<InspectorOverlayAgent> overlay_;
  // For CompositeAfterPaint.
  scoped_refptr<cc::PictureLayer> layer_;
};

class InspectorOverlayAgent::InspectorOverlayChromeClient final
    : public EmptyChromeClient {
 public:
  static InspectorOverlayChromeClient* Create(ChromeClient& client,
                                              InspectorOverlayAgent& overlay) {
    return MakeGarbageCollected<InspectorOverlayChromeClient>(client, overlay);
  }

  InspectorOverlayChromeClient(ChromeClient& client,
                               InspectorOverlayAgent& overlay)
      : client_(&client), overlay_(&overlay) {}

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(client_);
    visitor->Trace(overlay_);
    EmptyChromeClient::Trace(visitor);
  }

  void SetCursor(const Cursor& cursor, LocalFrame* local_root) override {
    if (overlay_->inspect_mode_.Get() ==
        protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
      return;
    }
    client_->SetCursorOverridden(false);
    client_->SetCursor(cursor, overlay_->frame_impl_->GetFrame());
    client_->SetCursorOverridden(true);
  }

  void SetToolTip(LocalFrame& frame,
                  const String& tooltip,
                  TextDirection direction) override {
    DCHECK_EQ(&frame, overlay_->OverlayMainFrame());
    client_->SetToolTip(*overlay_->frame_impl_->GetFrame(), tooltip, direction);
  }

  void InvalidateRect(const IntRect&) override { overlay_->Invalidate(); }

 private:
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
      resize_timer_active_(false),
      omit_tooltip_(false),
      timer_(
          frame_impl->GetFrame()->GetTaskRunner(TaskType::kInternalInspector),
          this,
          &InspectorOverlayAgent::OnTimer),
      disposed_(false),
      in_layout_(false),
      needs_update_(false),
      v8_session_(v8_session),
      dom_agent_(dom_agent),
      swallow_next_mouse_up_(false),
      swallow_next_escape_up_(false),
      backend_node_id_to_inspect_(0),
      enabled_(&agent_state_, false),
      suspended_(&agent_state_, false),
      show_ad_highlights_(&agent_state_, false),
      show_debug_borders_(&agent_state_, false),
      show_fps_counter_(&agent_state_, false),
      show_paint_rects_(&agent_state_, false),
      show_scroll_bottleneck_rects_(&agent_state_, false),
      show_hit_test_borders_(&agent_state_, false),
      show_size_on_resize_(&agent_state_, false),
      paused_in_debugger_message_(&agent_state_, String()),
      inspect_mode_(&agent_state_, protocol::Overlay::InspectModeEnum::None),
      inspect_mode_protocol_config_(&agent_state_, String()) {}

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
  setShowAdHighlights(show_ad_highlights_.Get());
  setShowDebugBorders(show_debug_borders_.Get());
  setShowFPSCounter(show_fps_counter_.Get());
  setShowPaintRects(show_paint_rects_.Get());
  setShowScrollBottleneckRects(show_scroll_bottleneck_rects_.Get());
  setShowHitTestBorders(show_hit_test_borders_.Get());
  setShowViewportSizeOnResize(show_size_on_resize_.Get());
  if (paused_in_debugger_message_.Get().IsNull())
    setPausedInDebuggerMessage(paused_in_debugger_message_.Get());
  setSuspended(suspended_.Get());
  if (inspect_mode_.Get() != protocol::Overlay::InspectModeEnum::None) {
    std::unique_ptr<protocol::Value> value =
        protocol::StringUtil::parseJSON(inspect_mode_protocol_config_.Get());
    std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config;
    protocol::ErrorSupport errors;
    if (value) {
      highlight_config =
          protocol::Overlay::HighlightConfig::fromValue(value.get(), &errors);
    }
    SetSearchingForNode(inspect_mode_.Get(), std::move(highlight_config));
  }
}

void InspectorOverlayAgent::Dispose() {
  InspectorBaseAgent::Dispose();
  disposed_ = true;
  ClearInternal();
}

Response InspectorOverlayAgent::enable() {
  if (!dom_agent_->Enabled())
    return Response::Error("DOM should be enabled first");
  enabled_.Set(true);
  if (backend_node_id_to_inspect_) {
    GetFrontend()->inspectNodeRequested(
        static_cast<int>(backend_node_id_to_inspect_));
  }
  backend_node_id_to_inspect_ = 0;
  SetNeedsUnbufferedInput(true);
  return Response::OK();
}

Response InspectorOverlayAgent::disable() {
  enabled_.Clear();
  setShowAdHighlights(false);
  setShowDebugBorders(false);
  setShowFPSCounter(false);
  setShowPaintRects(false);
  setShowScrollBottleneckRects(false);
  setShowHitTestBorders(false);
  setShowViewportSizeOnResize(false);
  setPausedInDebuggerMessage(String());
  setSuspended(false);
  SetSearchingForNode(protocol::Overlay::InspectModeEnum::None,
                      Maybe<protocol::Overlay::HighlightConfig>());
  SetNeedsUnbufferedInput(false);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowAdHighlights(bool show) {
  show_ad_highlights_.Set(show);
  frame_impl_->ViewImpl()->GetPage()->GetSettings().SetHighlightAds(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowDebugBorders(bool show) {
  show_debug_borders_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl)
    widget_impl->Client()->SetShowDebugBorders(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowFPSCounter(bool show) {
  show_fps_counter_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl)
    widget_impl->Client()->SetShowFPSCounter(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowPaintRects(bool show) {
  show_paint_rects_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl)
    widget_impl->Client()->SetShowPaintRects(show);
  if (!show && frame_impl_->GetFrameView())
    frame_impl_->GetFrameView()->Invalidate();
  return Response::OK();
}

Response InspectorOverlayAgent::setShowScrollBottleneckRects(bool show) {
  show_scroll_bottleneck_rects_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl)
    widget_impl->Client()->SetShowScrollBottleneckRects(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowHitTestBorders(bool show) {
  show_hit_test_borders_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl)
    widget_impl->Client()->SetShowHitTestBorders(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setShowViewportSizeOnResize(bool show) {
  show_size_on_resize_.Set(show);
  return Response::OK();
}

Response InspectorOverlayAgent::setPausedInDebuggerMessage(
    Maybe<String> message) {
  paused_in_debugger_message_.Set(message.fromMaybe(String()));
  ScheduleUpdate();
  return Response::OK();
}

Response InspectorOverlayAgent::setSuspended(bool suspended) {
  if (suspended && !suspended_.Get())
    ClearInternal();
  suspended_.Set(suspended);
  SetNeedsUnbufferedInput(!suspended);
  return Response::OK();
}

Response InspectorOverlayAgent::setInspectMode(
    const String& mode,
    Maybe<protocol::Overlay::HighlightConfig> highlight_config) {
  if (mode != protocol::Overlay::InspectModeEnum::SearchForNode &&
      mode != protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM &&
      mode != protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot &&
      mode != protocol::Overlay::InspectModeEnum::None) {
    return Response::Error(
        String("Unknown mode \"" + mode + "\" was provided."));
  }
  return SetSearchingForNode(mode, std::move(highlight_config));
}

Response InspectorOverlayAgent::highlightRect(
    int x,
    int y,
    int width,
    int height,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<FloatQuad> quad =
      std::make_unique<FloatQuad>(FloatRect(x, y, width, height));
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
    Maybe<String> object_id,
    Maybe<String> selector_list) {
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

  InnerHighlightNode(node, nullptr, selector_list.fromMaybe(String()),
                     *highlight_config, false);
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
    InnerHighlightNode(frame->DeprecatedLocalOwner(), nullptr, String(),
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
  InspectorHighlight highlight(node, InspectorHighlight::DefaultConfig(),
                               InspectorHighlightContrastInfo(), true);
  *result = highlight.AsProtocolValue();
  return Response::OK();
}

void InspectorOverlayAgent::Invalidate() {
  if (IsEmpty())
    return;

  if (!frame_overlay_) {
    frame_overlay_ = FrameOverlay::Create(
        frame_impl_->GetFrame(),
        std::make_unique<InspectorPageOverlayDelegate>(*this));
  }

  frame_overlay_->Update();
  if (auto* frame_view = frame_impl_->GetFrameView())
    frame_view->SetPaintArtifactCompositorNeedsUpdate();
}

void InspectorOverlayAgent::UpdateAllOverlayLifecyclePhases() {
  if (frame_overlay_)
    frame_overlay_->Update();

  if (!IsEmpty()) {
    base::AutoReset<bool> scoped(&in_layout_, true);
    if (needs_update_) {
      needs_update_ = false;
      RebuildOverlayPage();
    }
    OverlayMainFrame()->View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kOther);
  }

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() && frame_overlay_ &&
      frame_overlay_->GetGraphicsLayer())
    frame_overlay_->GetGraphicsLayer()->Paint();
}

void InspectorOverlayAgent::PaintOverlay(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (frame_overlay_)
    frame_overlay_->Paint(context);
}

bool InspectorOverlayAgent::IsInspectorLayer(const cc::Layer* layer) const {
  if (!frame_overlay_)
    return false;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    return layer == static_cast<const InspectorPageOverlayDelegate*>(
                        frame_overlay_->GetDelegate())
                        ->GetLayer();
  }
  return layer == frame_overlay_->GetGraphicsLayer()->CcLayer();
}

void InspectorOverlayAgent::DispatchBufferedTouchEvents() {
  if (IsEmpty())
    return;
  OverlayMainFrame()->GetEventHandler().DispatchBufferedTouchEvents();
}

bool InspectorOverlayAgent::HandleInputEvent(const WebInputEvent& input_event) {
  if (input_event.GetType() == WebInputEvent::kMouseUp &&
      swallow_next_mouse_up_) {
    swallow_next_mouse_up_ = false;
    return true;
  }

  if (input_event.GetType() == WebInputEvent::kKeyUp &&
      swallow_next_escape_up_) {
    auto keyboard_event = static_cast<const WebKeyboardEvent&>(input_event);
    if (keyboard_event.windows_key_code == VKEY_ESCAPE) {
      swallow_next_escape_up_ = false;
      return true;
    }
  }

  if (IsEmpty())
    return false;

  // In the inspect mode, after clicking, keyboard events are dispatched here.
  // Handle Escape below.

  if (inspect_mode_.Get() != protocol::Overlay::InspectModeEnum::None &&
      input_event.GetType() == WebInputEvent::kRawKeyDown) {
    auto keyboard_event = static_cast<const WebKeyboardEvent&>(input_event);
    if (keyboard_event.windows_key_code == VKEY_ESCAPE) {
      // If we are in the process of dragging, reset the dragging.
      // Otherwise, cancel searching.
      if (screenshot_anchor_ != IntPoint::Zero()) {
        screenshot_anchor_ = IntPoint::Zero();
        ScheduleUpdate();
      } else {
        GetFrontend()->inspectModeCanceled();
      }
      swallow_next_escape_up_ = true;
      return true;
    }
  }

  bool handled = false;

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
      handled =
          OverlayMainFrame()->GetEventHandler().HandleMouseMoveEvent(
              mouse_event,
              TransformWebMouseEventVector(frame_impl_->GetFrameView(),
                                           std::vector<const WebInputEvent*>()),
              TransformWebMouseEventVector(
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

  if (WebInputEvent::IsPointerEventType(input_event.GetType())) {
    WebPointerEvent transformed_event = TransformWebPointerEvent(
        frame_impl_->GetFrameView(),
        static_cast<const WebPointerEvent&>(input_event));
    handled = HandlePointerEvent(transformed_event);
    if (handled)
      return true;
    OverlayMainFrame()->GetEventHandler().HandlePointerEvent(
        transformed_event, Vector<WebPointerEvent>(),
        Vector<WebPointerEvent>());
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

void InspectorOverlayAgent::InnerHideHighlight() {
  highlight_node_.Clear();
  event_target_node_.Clear();
  highlight_quad_.reset();
  highlight_node_contrast_ = InspectorHighlightContrastInfo();
  ScheduleUpdate();
}

void InspectorOverlayAgent::InnerHighlightNode(
    Node* node,
    Node* event_target,
    String selector_list,
    const InspectorHighlightConfig& highlight_config,
    bool omit_tooltip) {
  node_highlight_config_ = highlight_config;
  highlight_node_ = node;
  highlight_selector_list_ = selector_list;
  event_target_node_ = event_target;
  omit_tooltip_ = omit_tooltip;
  highlight_node_contrast_ = InspectorHighlightContrastInfo();

  if (node->IsElementNode()) {
    // Compute the color contrast information here.
    Vector<Color> bgcolors;
    String font_size;
    String font_weight;
    InspectorCSSAgent::GetBackgroundColors(ToElement(node), &bgcolors,
                                           &font_size, &font_weight);
    if (bgcolors.size() == 1) {
      highlight_node_contrast_.font_size = font_size;
      highlight_node_contrast_.font_weight = font_weight;
      highlight_node_contrast_.background_color = bgcolors[0];
    }
  }

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
  if (suspended_.Get())
    return true;
  bool has_visible_elements =
      highlight_node_ || event_target_node_ || highlight_quad_ ||
      (resize_timer_active_ && show_size_on_resize_.Get()) ||
      !paused_in_debugger_message_.Get().IsNull();
  return !has_visible_elements &&
         inspect_mode_.Get() == protocol::Overlay::InspectModeEnum::None;
}

void InspectorOverlayAgent::ScheduleUpdate() {
  auto& client = frame_impl_->GetFrame()->GetPage()->GetChromeClient();
  if (IsEmpty()) {
    if (frame_overlay_) {
      frame_overlay_.reset();
      client.SetCursorOverridden(false);
      client.SetCursor(PointerCursor(), frame_impl_->GetFrame());
      if (auto* frame_view = frame_impl_->GetFrameView())
        frame_view->SetPaintArtifactCompositorNeedsUpdate();
    }
    return;
  }
  needs_update_ = true;
  client.ScheduleAnimation(frame_impl_->GetFrame()->View());
}

void InspectorOverlayAgent::RebuildOverlayPage() {
  LocalFrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = frame_impl_->GetFrame();
  if (!view || !frame)
    return;

  IntSize viewport_size = frame->GetPage()->GetVisualViewport().Size();
  OverlayMainFrame()->View()->Resize(viewport_size);
  OverlayPage()->GetVisualViewport().SetSize(viewport_size);
  OverlayMainFrame()->SetPageZoomFactor(WindowToViewportScale());

  Reset(viewport_size);
  DrawMatchingSelector();
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

void InspectorOverlayAgent::DrawMatchingSelector() {
  if (highlight_selector_list_.IsEmpty() || !highlight_node_)
    return;
  DummyExceptionStateForTesting exception_state;
  ContainerNode* query_base = highlight_node_->ContainingShadowRoot();
  if (!query_base)
    query_base = highlight_node_->ownerDocument();
  StaticElementList* elements = query_base->QuerySelectorAll(
      AtomicString(highlight_selector_list_), exception_state);
  if (exception_state.HadException())
    return;

  for (unsigned i = 0; i < elements->length(); ++i) {
    Element* element = elements->item(i);
    InspectorHighlight highlight(element, node_highlight_config_,
                                 highlight_node_contrast_, false);
    std::unique_ptr<protocol::DictionaryValue> highlight_json =
        highlight.AsProtocolValue();
    EvaluateInOverlay("drawHighlight", std::move(highlight_json));
  }
}

void InspectorOverlayAgent::DrawNodeHighlight() {
  if (!highlight_node_)
    return;

  bool append_element_info =
      (highlight_node_->IsElementNode() || highlight_node_->IsTextNode()) &&
      !omit_tooltip_ && node_highlight_config_.show_info &&
      highlight_node_->GetLayoutObject() &&
      highlight_node_->GetDocument().GetFrame();
  InspectorHighlight highlight(highlight_node_.Get(), node_highlight_config_,
                               highlight_node_contrast_, append_element_info);
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
  if (inspect_mode_.Get() == protocol::Overlay::InspectModeEnum::None &&
      !paused_in_debugger_message_.Get().IsNull()) {
    EvaluateInOverlay("drawPausedInDebuggerMessage",
                      paused_in_debugger_message_.Get());
  }
}

void InspectorOverlayAgent::DrawViewSize() {
  if (resize_timer_active_ && show_size_on_resize_.Get())
    EvaluateInOverlay("drawViewSize", "");
}

void InspectorOverlayAgent::DrawScreenshotBorder() {
  if (screenshot_anchor_ == IntPoint::Zero())
    return;
  const VisualViewport& visual_viewport =
      frame_impl_->GetFrame()->GetPage()->GetVisualViewport();
  IntPoint p1 = visual_viewport.RootFrameToViewport(screenshot_anchor_);
  IntPoint p2 = visual_viewport.RootFrameToViewport(screenshot_position_);
  float scale = 1.f / WindowToViewportScale();
  p1.Scale(scale, scale);
  p2.Scale(scale, scale);
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

  DEFINE_STATIC_LOCAL(Persistent<LocalFrameClient>, dummy_local_frame_client,
                      (EmptyLocalFrameClient::Create()));
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  DCHECK(!overlay_chrome_client_);
  overlay_chrome_client_ = InspectorOverlayChromeClient::Create(
      frame_impl_->GetFrame()->GetPage()->GetChromeClient(), *this);
  page_clients.chrome_client = overlay_chrome_client_.Get();
  overlay_page_ = Page::Create(page_clients);
  overlay_host_ = MakeGarbageCollected<InspectorOverlayHost>(this);

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

  LocalFrame* frame =
      LocalFrame::Create(dummy_local_frame_client, *overlay_page_, nullptr);
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
  return To<LocalFrame>(OverlayPage()->MainFrame());
}

void InspectorOverlayAgent::Reset(const IntSize& viewport_size) {
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

  // TODO(szager): These values have been zero since root layer scrolling
  // landed. Probably they should be derived from
  // LocalFrameView::LayoutViewport(); but I have no idea who the consumers
  // of these values are, so I'm leaving them zero pending investigation.
  reset_data->setInteger("scrollX", 0);
  reset_data->setInteger("scrollY", 0);
  EvaluateInOverlay("reset", std::move(reset_data));
}

void InspectorOverlayAgent::EvaluateInOverlay(const String& method,
                                              const String& argument) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
  command->pushValue(protocol::StringValue::create(method));
  command->pushValue(protocol::StringValue::create(argument));
  To<LocalFrame>(OverlayPage()->MainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" + command->toJSONString() + ")",
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
  To<LocalFrame>(OverlayPage()->MainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" + command->toJSONString() + ")",
          ScriptSourceLocationType::kInspector,
          ScriptController::kExecuteScriptWhenScriptsDisabled);
}

String InspectorOverlayAgent::EvaluateInOverlayForTest(const String& script) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::HandleScope handle_scope(ToIsolate(OverlayMainFrame()));
  v8::Local<v8::Value> string =
      To<LocalFrame>(OverlayPage()->MainFrame())
          ->GetScriptController()
          .ExecuteScriptInMainWorldAndReturnValue(
              ScriptSourceCode(script, ScriptSourceLocationType::kInspector),
              KURL(), SanitizeScriptErrors::kSanitize, ScriptFetchOptions(),
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
  paused_in_debugger_message_.Clear();
  inspect_mode_.Set(protocol::Overlay::InspectModeEnum::None);
  inspect_mode_protocol_config_.Set(String());
  timer_.Stop();
  frame_overlay_.reset();
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
  if (resized && show_size_on_resize_.Get()) {
    resize_timer_active_ = true;
    timer_.StartOneShot(TimeDelta::FromSeconds(1), FROM_HERE);
  }
  ScheduleUpdate();
}

bool InspectorOverlayAgent::HandleMouseMove(const WebMouseEvent& event) {
  if (!InSomeInspectMode())
    return false;

  if (inspect_mode_.Get() ==
      protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
    screenshot_position_ = RoundedIntPoint(event.PositionInRootFrame());
    ScheduleUpdate();
    return true;
  }

  LocalFrame* frame = frame_impl_->GetFrame();
  if (!frame || !frame->View() || !frame->ContentLayoutObject())
    return false;
  Node* node = HoveredNodeForEvent(
      frame, event, event.GetModifiers() & WebInputEvent::kShiftKey);

  // Do not highlight within user agent shadow root unless requested.
  if (inspect_mode_.Get() !=
      protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM) {
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
    InnerHighlightNode(node, event_target, String(),
                       *inspect_mode_highlight_config_, omit_tooltip);
  }
  return true;
}

bool InspectorOverlayAgent::HandleMouseDown(const WebMouseEvent& event) {
  swallow_next_mouse_up_ = false;
  if (!InSomeInspectMode())
    return false;

  if (inspect_mode_.Get() ==
      protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
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
  if (inspect_mode_.Get() !=
      protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
    return false;
  }

  if (screenshot_anchor_ == IntPoint::Zero())
    return true;
  float scale = 1.0f;
  IntPoint p1 = screenshot_anchor_;
  IntPoint p2 = screenshot_position_;
  if (LocalFrame* frame = frame_impl_->GetFrame()) {
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
  float dp_to_dip = 1.f / WindowToViewportScale();
  p1.Scale(dp_to_dip, dp_to_dip);
  p2.Scale(dp_to_dip, dp_to_dip);
  // Points are in device independent pixels (dip) now.
  IntRect rect =
      UnionRectEvenIfEmpty(IntRect(p1, IntSize()), IntRect(p2, IntSize()));
  if (rect.Width() < 5 || rect.Height() < 5) {
    screenshot_anchor_ = IntPoint::Zero();
    return true;
  }
  GetFrontend()->screenshotRequested(protocol::Page::Viewport::create()
                                         .setX(rect.X())
                                         .setY(rect.Y())
                                         .setWidth(rect.Width())
                                         .setHeight(rect.Height())
                                         .setScale(scale)
                                         .build());
  return true;
}

bool InspectorOverlayAgent::HandleGestureEvent(const WebGestureEvent& event) {
  if (!InSomeInspectMode() || event.GetType() != WebInputEvent::kGestureTap)
    return false;
  Node* node = HoveredNodeForEvent(frame_impl_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    InnerHighlightNode(node, nullptr, String(), *inspect_mode_highlight_config_,
                       false);
    Inspect(node);
    return true;
  }
  return false;
}

bool InspectorOverlayAgent::HandlePointerEvent(const WebPointerEvent& event) {
  if (!InSomeInspectMode())
    return false;
  Node* node = HoveredNodeForEvent(frame_impl_->GetFrame(), event, false);
  if (node && inspect_mode_highlight_config_) {
    InnerHighlightNode(node, nullptr, String(), *inspect_mode_highlight_config_,
                       false);
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

bool InspectorOverlayAgent::InSomeInspectMode() {
  return inspect_mode_.Get() != protocol::Overlay::InspectModeEnum::None;
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

  DOMNodeId backend_node_id = DOMNodeIds::IdForNode(node);
  if (!enabled_.Get()) {
    backend_node_id_to_inspect_ = backend_node_id;
    return;
  }

  GetFrontend()->inspectNodeRequested(IdentifiersFactory::IntIdForNode(node));
}

void InspectorOverlayAgent::NodeHighlightRequested(Node* node) {
  if (!enabled_.Get())
    return;

  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return;

  int node_id = dom_agent_->PushNodePathToFrontend(node);
  if (node_id)
    GetFrontend()->nodeHighlightRequested(node_id);
}

Response InspectorOverlayAgent::SetSearchingForNode(
    String search_mode,
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object) {
  if (search_mode == protocol::Overlay::InspectModeEnum::None) {
    inspect_mode_.Set(search_mode);
    hovered_node_for_inspect_mode_.Clear();
    screenshot_anchor_ = IntPoint::Zero();
    screenshot_position_ = IntPoint::Zero();
    InnerHideHighlight();
    return Response::OK();
  }

  String serialized_config =
      highlight_inspector_object.isJust()
          ? highlight_inspector_object.fromJust()->toJSON()
          : String();
  std::unique_ptr<InspectorHighlightConfig> config;
  Response response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &config);
  if (!response.isSuccess())
    return response;
  inspect_mode_.Set(search_mode);
  inspect_mode_protocol_config_.Set(serialized_config);
  inspect_mode_highlight_config_ = std::move(config);

  if (search_mode ==
      protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
    auto& client = frame_impl_->GetFrame()->GetPage()->GetChromeClient();
    client.SetCursorOverridden(false);
    client.SetCursor(CrossCursor(), frame_impl_->GetFrame());
    client.SetCursorOverridden(true);
    hovered_node_for_inspect_mode_.Clear();
    InnerHideHighlight();
  } else {
    ScheduleUpdate();
  }
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
  highlight_config->show_styles = config->getShowStyles(false);
  highlight_config->show_rulers = config->getShowRulers(false);
  highlight_config->show_extension_lines = config->getShowExtensionLines(false);
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

  *out_config = std::move(highlight_config);
  return Response::OK();
}

void InspectorOverlayAgent::SetNeedsUnbufferedInput(bool unbuffered) {
  LocalFrame* frame = frame_impl_->GetFrame();
  if (frame) {
    frame->GetPage()->GetChromeClient().SetNeedsUnbufferedInputForDebugger(
        frame, unbuffered);
  }
}

}  // namespace blink
