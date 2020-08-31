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
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_inspector_overlay_host.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
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
#include "third_party/blink/renderer/core/inspector/inspect_tools.h"
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
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "v8/include/v8.h"

using crdtp::SpanFrom;
using crdtp::json::ConvertCBORToJSON;

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace {

bool ParseQuad(std::unique_ptr<protocol::Array<double>> quad_array,
               FloatQuad* quad) {
  const size_t kCoordinatesInQuad = 8;
  if (!quad_array || quad_array->size() != kCoordinatesInQuad)
    return false;
  quad->SetP1(FloatPoint((*quad_array)[0], (*quad_array)[1]));
  quad->SetP2(FloatPoint((*quad_array)[2], (*quad_array)[3]));
  quad->SetP3(FloatPoint((*quad_array)[4], (*quad_array)[5]));
  quad->SetP4(FloatPoint((*quad_array)[6], (*quad_array)[7]));
  return true;
}

}  // namespace

// InspectTool -----------------------------------------------------------------

void InspectTool::Init(InspectorOverlayAgent* overlay,
                       OverlayFrontend* frontend) {
  overlay_ = overlay;
  frontend_ = frontend;
  DoInit();
}

int InspectTool::GetDataResourceId() {
  return IDR_INSPECT_TOOL_HIGHLIGHT_HTML;
}

bool InspectTool::HandleInputEvent(LocalFrameView* frame_view,
                                   const WebInputEvent& input_event,
                                   bool* swallow_next_mouse_up) {
  if (input_event.GetType() == WebInputEvent::Type::kGestureTap) {
    // We only have a use for gesture tap.
    WebGestureEvent transformed_event = TransformWebGestureEvent(
        frame_view, static_cast<const WebGestureEvent&>(input_event));
    return HandleGestureTapEvent(transformed_event);
  }

  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    WebMouseEvent transformed_event = TransformWebMouseEvent(
        frame_view, static_cast<const WebMouseEvent&>(input_event));
    return HandleMouseEvent(transformed_event, swallow_next_mouse_up);
  }

  if (WebInputEvent::IsPointerEventType(input_event.GetType())) {
    WebPointerEvent transformed_event = TransformWebPointerEvent(
        frame_view, static_cast<const WebPointerEvent&>(input_event));
    return HandlePointerEvent(transformed_event);
  }

  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
    return HandleKeyboardEvent(
        static_cast<const WebKeyboardEvent&>(input_event));
  }

  return false;
}

bool InspectTool::HandleMouseEvent(const WebMouseEvent& mouse_event,
                                   bool* swallow_next_mouse_up) {
  if (mouse_event.GetType() == WebInputEvent::Type::kMouseMove)
    return HandleMouseMove(mouse_event);

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown)
    return HandleMouseDown(mouse_event, swallow_next_mouse_up);

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp)
    return HandleMouseUp(mouse_event);

  return false;
}

bool InspectTool::HandleMouseDown(const WebMouseEvent&,
                                  bool* swallow_next_mouse_up) {
  return false;
}

bool InspectTool::HandleMouseUp(const WebMouseEvent&) {
  return false;
}

bool InspectTool::HandleMouseMove(const WebMouseEvent&) {
  return false;
}

bool InspectTool::HandleGestureTapEvent(const WebGestureEvent&) {
  return false;
}

bool InspectTool::HandlePointerEvent(const WebPointerEvent&) {
  return false;
}

bool InspectTool::HandleKeyboardEvent(const WebKeyboardEvent&) {
  return false;
}

bool InspectTool::ForwardEventsToOverlay() {
  return true;
}

bool InspectTool::HideOnMouseMove() {
  return false;
}

bool InspectTool::HideOnHideHighlight() {
  return false;
}

void InspectTool::Trace(Visitor* visitor) {
  visitor->Trace(overlay_);
}

// Hinge -----------------------------------------------------------------------

Hinge::Hinge(FloatQuad quad,
             Color content_color,
             Color outline_color,
             InspectorOverlayAgent* overlay)
    : quad_(quad),
      content_color_(content_color),
      outline_color_(outline_color),
      overlay_(overlay) {}

// static
int Hinge::GetDataResourceId() {
  // TODO (soxia): In the future, we should make the hinge working properly
  // with tools using different resources.
  return IDR_INSPECT_TOOL_HIGHLIGHT_HTML;
}

void Hinge::Trace(Visitor* visitor) {
  visitor->Trace(overlay_);
}

void Hinge::Draw(float scale) {
  InspectorHighlight highlight(scale);
  highlight.AppendQuad(quad_, content_color_, outline_color_);
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

// InspectorOverlayAgent -------------------------------------------------------

class InspectorOverlayAgent::InspectorPageOverlayDelegate final
    : public FrameOverlay::Delegate,
      public cc::ContentLayerClient {
 public:
  explicit InspectorPageOverlayDelegate(InspectorOverlayAgent& overlay)
      : overlay_(&overlay) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      layer_ = cc::PictureLayer::Create(this);
      layer_->SetIsDrawable(true);
      layer_->SetHitTestable(false);
    }
  }
  ~InspectorPageOverlayDelegate() override {
    if (layer_)
      layer_->ClearClient();
  }

  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const IntSize&) const override {
    if (!overlay_->IsVisible())
      return;

    overlay_->PaintOverlayPage();

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      layer_->SetBounds(gfx::Size(frame_overlay.Size()));
      DEFINE_STATIC_LOCAL(LiteralDebugNameClient, debug_name_client,
                          ("InspectorOverlay"));
      RecordForeignLayer(graphics_context, debug_name_client,
                         DisplayItem::kForeignLayerDevToolsOverlay, layer_,
                         FloatPoint(), &PropertyTreeState::Root());
      return;
    }

    frame_overlay.Invalidate();
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay);
    // The overlay frame is has a standalone paint property tree. Paint it in
    // its root space into a paint record, then draw the record into the proper
    // target space in the overlaid frame.
    PaintRecordBuilder paint_record_builder(nullptr, &graphics_context);
    overlay_->OverlayMainFrame()->View()->PaintOutsideOfLifecycle(
        paint_record_builder.Context(), kGlobalPaintNormalPhase);
    graphics_context.DrawRecord(paint_record_builder.EndRecording());
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
  InspectorOverlayChromeClient(ChromeClient& client,
                               InspectorOverlayAgent& overlay)
      : client_(&client), overlay_(&overlay) {}

  void Trace(Visitor* visitor) override {
    visitor->Trace(client_);
    visitor->Trace(overlay_);
    EmptyChromeClient::Trace(visitor);
  }

  void SetCursor(const ui::Cursor& cursor, LocalFrame* local_root) override {
    client_->SetCursorOverridden(false);
    client_->SetCursor(cursor, overlay_->GetFrame());
    client_->SetCursorOverridden(true);
  }

  void SetToolTip(LocalFrame& frame,
                  const String& tooltip,
                  TextDirection direction) override {
    DCHECK_EQ(&frame, overlay_->OverlayMainFrame());
    client_->SetToolTip(*overlay_->GetFrame(), tooltip, direction);
  }

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
      resize_timer_(
          frame_impl->GetFrame()->GetTaskRunner(TaskType::kInternalInspector),
          this,
          &InspectorOverlayAgent::OnResizeTimer),
      disposed_(false),
      v8_session_(v8_session),
      dom_agent_(dom_agent),
      swallow_next_mouse_up_(false),
      backend_node_id_to_inspect_(0),
      enabled_(&agent_state_, false),
      show_ad_highlights_(&agent_state_, false),
      show_debug_borders_(&agent_state_, false),
      show_fps_counter_(&agent_state_, false),
      show_paint_rects_(&agent_state_, false),
      show_layout_shift_regions_(&agent_state_, false),
      show_scroll_bottleneck_rects_(&agent_state_, false),
      show_hit_test_borders_(&agent_state_, false),
      show_size_on_resize_(&agent_state_, false),
      paused_in_debugger_message_(&agent_state_, String()),
      inspect_mode_(&agent_state_, protocol::Overlay::InspectModeEnum::None),
      inspect_mode_protocol_config_(&agent_state_, std::vector<uint8_t>()) {}

InspectorOverlayAgent::~InspectorOverlayAgent() {
  DCHECK(!overlay_page_);
}

void InspectorOverlayAgent::Trace(Visitor* visitor) {
  visitor->Trace(frame_impl_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(overlay_page_);
  visitor->Trace(overlay_chrome_client_);
  visitor->Trace(overlay_host_);
  visitor->Trace(dom_agent_);
  visitor->Trace(inspect_tool_);
  visitor->Trace(hinge_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorOverlayAgent::Restore() {
  if (enabled_.Get())
    enable();
  setShowAdHighlights(show_ad_highlights_.Get());
  setShowDebugBorders(show_debug_borders_.Get());
  setShowFPSCounter(show_fps_counter_.Get());
  setShowPaintRects(show_paint_rects_.Get());
  setShowLayoutShiftRegions(show_layout_shift_regions_.Get());
  setShowScrollBottleneckRects(show_scroll_bottleneck_rects_.Get());
  setShowHitTestBorders(show_hit_test_borders_.Get());
  setShowViewportSizeOnResize(show_size_on_resize_.Get());
  PickTheRightTool();
}

void InspectorOverlayAgent::Dispose() {
  InspectorBaseAgent::Dispose();
  disposed_ = true;
}

Response InspectorOverlayAgent::enable() {
  if (!dom_agent_->Enabled())
    return Response::ServerError("DOM should be enabled first");
  enabled_.Set(true);
  if (backend_node_id_to_inspect_) {
    GetFrontend()->inspectNodeRequested(
        static_cast<int>(backend_node_id_to_inspect_));
  }
  backend_node_id_to_inspect_ = 0;
  SetNeedsUnbufferedInput(true);
  return Response::Success();
}

Response InspectorOverlayAgent::disable() {
  enabled_.Clear();
  setShowAdHighlights(false);
  setShowDebugBorders(false);
  setShowFPSCounter(false);
  setShowPaintRects(false);
  setShowLayoutShiftRegions(false);
  setShowScrollBottleneckRects(false);
  setShowHitTestBorders(false);
  setShowViewportSizeOnResize(false);
  paused_in_debugger_message_.Clear();
  inspect_mode_.Set(protocol::Overlay::InspectModeEnum::None);
  inspect_mode_protocol_config_.Set(std::vector<uint8_t>());
  if (overlay_page_) {
    overlay_page_->WillBeDestroyed();
    overlay_page_.Clear();
    overlay_chrome_client_.Clear();
    overlay_host_->ClearDelegate();
    overlay_host_.Clear();
  }
  resize_timer_.Stop();
  resize_timer_active_ = false;
  frame_overlay_.reset();
  frame_resource_name_ = 0;
  PickTheRightTool();
  SetNeedsUnbufferedInput(false);
  return Response::Success();
}

Response InspectorOverlayAgent::setShowAdHighlights(bool show) {
  show_ad_highlights_.Set(show);
  frame_impl_->ViewImpl()->GetPage()->GetSettings().SetHighlightAds(show);
  return Response::Success();
}

Response InspectorOverlayAgent::setShowDebugBorders(bool show) {
  show_debug_borders_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.IsSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl) {
    cc::LayerTreeDebugState debug_state = widget_impl->GetLayerTreeDebugState();
    if (show)
      debug_state.show_debug_borders.set();
    else
      debug_state.show_debug_borders.reset();
    widget_impl->SetLayerTreeDebugState(debug_state);
  }
  return Response::Success();
}

Response InspectorOverlayAgent::setShowFPSCounter(bool show) {
  show_fps_counter_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.IsSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl) {
    cc::LayerTreeDebugState debug_state = widget_impl->GetLayerTreeDebugState();
    debug_state.show_fps_counter = show;
    widget_impl->SetLayerTreeDebugState(debug_state);
  }
  return Response::Success();
}

Response InspectorOverlayAgent::setShowPaintRects(bool show) {
  show_paint_rects_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.IsSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl) {
    cc::LayerTreeDebugState debug_state = widget_impl->GetLayerTreeDebugState();
    debug_state.show_paint_rects = show;
    widget_impl->SetLayerTreeDebugState(debug_state);
  }
  if (!show && frame_impl_->GetFrameView())
    frame_impl_->GetFrameView()->Invalidate();
  return Response::Success();
}

Response InspectorOverlayAgent::setShowLayoutShiftRegions(bool show) {
  show_layout_shift_regions_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.IsSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl) {
    cc::LayerTreeDebugState debug_state = widget_impl->GetLayerTreeDebugState();
    debug_state.show_layout_shift_regions = show;
    widget_impl->SetLayerTreeDebugState(debug_state);
  }

  if (!show && frame_impl_->GetFrameView())
    frame_impl_->GetFrameView()->Invalidate();
  return Response::Success();
}

Response InspectorOverlayAgent::setShowScrollBottleneckRects(bool show) {
  show_scroll_bottleneck_rects_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.IsSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl) {
    cc::LayerTreeDebugState debug_state = widget_impl->GetLayerTreeDebugState();
    debug_state.show_touch_event_handler_rects = show;
    debug_state.show_wheel_event_handler_rects = show;
    debug_state.show_non_fast_scrollable_rects = show;
    debug_state.show_main_thread_scrolling_reason_rects = show;
    widget_impl->SetLayerTreeDebugState(debug_state);
  }

  return Response::Success();
}

Response InspectorOverlayAgent::setShowHitTestBorders(bool show) {
  show_hit_test_borders_.Set(show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.IsSuccess())
      return response;
  }
  WebFrameWidget* widget = frame_impl_->LocalRoot()->FrameWidget();
  WebFrameWidgetBase* widget_impl = static_cast<WebFrameWidgetBase*>(widget);
  // While a frame is being detached the inspector will shutdown and
  // turn off debug overlays, but the WebFrameWidget is already gone.
  if (widget_impl) {
    cc::LayerTreeDebugState debug_state = widget_impl->GetLayerTreeDebugState();
    debug_state.show_hit_test_borders = show;
    widget_impl->SetLayerTreeDebugState(debug_state);
  }
  return Response::Success();
}

Response InspectorOverlayAgent::setShowViewportSizeOnResize(bool show) {
  show_size_on_resize_.Set(show);
  return Response::Success();
}

Response InspectorOverlayAgent::setPausedInDebuggerMessage(
    Maybe<String> message) {
  paused_in_debugger_message_.Set(message.fromMaybe(String()));
  PickTheRightTool();
  return Response::Success();
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
  SetInspectTool(MakeGarbageCollected<QuadHighlightTool>(
      std::move(quad), InspectorDOMAgent::ParseColor(color.fromMaybe(nullptr)),
      InspectorDOMAgent::ParseColor(outline_color.fromMaybe(nullptr))));
  return Response::Success();
}

Response InspectorOverlayAgent::highlightQuad(
    std::unique_ptr<protocol::Array<double>> quad_array,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<FloatQuad> quad = std::make_unique<FloatQuad>();
  if (!ParseQuad(std::move(quad_array), quad.get()))
    return Response::ServerError("Invalid Quad format");
  SetInspectTool(MakeGarbageCollected<QuadHighlightTool>(
      std::move(quad), InspectorDOMAgent::ParseColor(color.fromMaybe(nullptr)),
      InspectorDOMAgent::ParseColor(outline_color.fromMaybe(nullptr))));
  return Response::Success();
}

Response InspectorOverlayAgent::setShowHinge(
    protocol::Maybe<protocol::Overlay::HingeConfig> tool_config) {
  // Hide the hinge when called without a configuration.
  if (!tool_config.isJust()) {
    hinge_ = nullptr;
    if (!inspect_tool_)
      DisableFrameOverlay();
    ScheduleUpdate();
    return Response::Success();
  }

  // Create a hinge
  protocol::Overlay::HingeConfig* config = tool_config.fromJust();
  protocol::DOM::Rect* rect = config->getRect();
  int x = rect->getX();
  int y = rect->getY();
  int width = rect->getWidth();
  int height = rect->getHeight();
  if (x < 0 || y < 0 || width < 0 || height < 0)
    return Response::InvalidParams("Invalid hinge rectangle.");

  // Use default color if a content color is not provided.
  Color content_color =
      config->hasContentColor()
          ? InspectorDOMAgent::ParseColor(config->getContentColor(nullptr))
          : Color(38, 38, 38);
  // outlineColor uses a kTransparent default from ParseColor if not provided.
  Color outline_color =
      InspectorDOMAgent::ParseColor(config->getOutlineColor(nullptr));

  DCHECK(frame_impl_->GetFrameView() && GetFrame());

  LoadFrameForTool(Hinge::GetDataResourceId());
  EnsureEnableFrameOverlay();
  FloatQuad quad(FloatRect(x, y, width, height));
  hinge_ =
      MakeGarbageCollected<Hinge>(quad, content_color, outline_color, this);
  ScheduleUpdate();

  return Response::Success();
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
  if (!response.IsSuccess())
    return response;

  std::unique_ptr<InspectorHighlightConfig> highlight_config;
  response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &highlight_config);
  if (!response.IsSuccess())
    return response;

  SetInspectTool(MakeGarbageCollected<NodeHighlightTool>(
      node, selector_list.fromMaybe(String()), std::move(highlight_config)));
  return Response::Success();
}

Response InspectorOverlayAgent::highlightFrame(
    const String& frame_id,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  // FIXME: Inspector doesn't currently work cross process.
  if (!frame)
    return Response::ServerError("Invalid frame id");
  if (frame->DeprecatedLocalOwner()) {
    std::unique_ptr<InspectorHighlightConfig> highlight_config =
        std::make_unique<InspectorHighlightConfig>();
    highlight_config->show_info = true;  // Always show tooltips for frames.
    highlight_config->content =
        InspectorDOMAgent::ParseColor(color.fromMaybe(nullptr));
    highlight_config->content_outline =
        InspectorDOMAgent::ParseColor(outline_color.fromMaybe(nullptr));

    SetInspectTool(MakeGarbageCollected<NodeHighlightTool>(
        frame->DeprecatedLocalOwner(), String(), std::move(highlight_config)));
  } else {
    PickTheRightTool();
  }
  return Response::Success();
}

Response InspectorOverlayAgent::hideHighlight() {
  if (inspect_tool_ && inspect_tool_->HideOnHideHighlight())
    PickTheRightTool();

  return Response::Success();
}

Response InspectorOverlayAgent::getHighlightObjectForTest(
    int node_id,
    Maybe<bool> include_distance,
    Maybe<bool> include_style,
    Maybe<String> colorFormat,
    std::unique_ptr<protocol::DictionaryValue>* result) {
  Node* node = nullptr;
  Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  bool is_locked_ancestor = false;

  // If |node| is in a display locked subtree, highlight the highest locked
  // ancestor element instead.
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node)) {
    node = locked_ancestor;
    is_locked_ancestor = true;
  }

  InspectorHighlightConfig config = InspectorHighlight::DefaultConfig();
  config.show_styles = include_style.fromMaybe(false);

  String format = colorFormat.fromMaybe("hex");

  namespace ColorFormatEnum = protocol::Overlay::ColorFormatEnum;
  if (format == ColorFormatEnum::Hsl) {
    config.color_format = ColorFormat::HSL;
  } else if (format == ColorFormatEnum::Rgb) {
    config.color_format = ColorFormat::RGB;
  } else {
    config.color_format = ColorFormat::HEX;
  }

  InspectorHighlight highlight(node, config, InspectorHighlightContrastInfo(),
                               true /* append_element_info */,
                               include_distance.fromMaybe(false),
                               is_locked_ancestor);
  *result = highlight.AsProtocolValue();
  return Response::Success();
}

void InspectorOverlayAgent::UpdatePrePaint() {
  if (frame_overlay_)
    frame_overlay_->UpdatePrePaint();
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

LocalFrame* InspectorOverlayAgent::GetFrame() const {
  return frame_impl_->GetFrame();
}

void InspectorOverlayAgent::DispatchBufferedTouchEvents() {
  if (!inspect_tool_)
    return;
  OverlayMainFrame()->GetEventHandler().DispatchBufferedTouchEvents();
}

WebInputEventResult InspectorOverlayAgent::HandleInputEvent(
    const WebInputEvent& input_event) {
  if (input_event.GetType() == WebInputEvent::Type::kMouseUp &&
      swallow_next_mouse_up_) {
    swallow_next_mouse_up_ = false;
    return WebInputEventResult::kHandledSuppressed;
  }

  LocalFrame* frame = GetFrame();
  if (!frame || !frame->View() || !frame->ContentLayoutObject() ||
      !inspect_tool_)
    return WebInputEventResult::kNotHandled;

  bool handled = inspect_tool_->HandleInputEvent(
      frame_impl_->GetFrameView(), input_event, &swallow_next_mouse_up_);

  if (handled) {
    ScheduleUpdate();
    return WebInputEventResult::kHandledSuppressed;
  }

  if (inspect_tool_->ForwardEventsToOverlay()) {
    WebInputEventResult result = HandleInputEventInOverlay(input_event);
    if (result != WebInputEventResult::kNotHandled) {
      ScheduleUpdate();
      return result;
    }
  }

  // Exit tool upon unhandled Esc.
  if (input_event.GetType() == WebInputEvent::Type::kRawKeyDown) {
    const WebKeyboardEvent& keyboard_event =
        static_cast<const WebKeyboardEvent&>(input_event);
    if (keyboard_event.windows_key_code == VKEY_ESCAPE) {
      GetFrontend()->inspectModeCanceled();
      return WebInputEventResult::kNotHandled;
    }
  }

  if (input_event.GetType() == WebInputEvent::Type::kMouseMove &&
      inspect_tool_->HideOnMouseMove()) {
    PickTheRightTool();
  }

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult InspectorOverlayAgent::HandleInputEventInOverlay(
    const WebInputEvent& input_event) {
  if (input_event.GetType() == WebInputEvent::Type::kGestureTap) {
    return OverlayMainFrame()->GetEventHandler().HandleGestureEvent(
        static_cast<const WebGestureEvent&>(input_event));
  }

  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    WebMouseEvent mouse_event = static_cast<const WebMouseEvent&>(input_event);
    if (mouse_event.GetType() == WebInputEvent::Type::kMouseMove) {
      return OverlayMainFrame()->GetEventHandler().HandleMouseMoveEvent(
          mouse_event, {}, {});
    }
    if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown) {
      return OverlayMainFrame()->GetEventHandler().HandleMousePressEvent(
          mouse_event);
    }
    if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp) {
      return OverlayMainFrame()->GetEventHandler().HandleMouseReleaseEvent(
          mouse_event);
    }
    return WebInputEventResult::kNotHandled;
  }

  if (WebInputEvent::IsPointerEventType(input_event.GetType())) {
    return OverlayMainFrame()->GetEventHandler().HandlePointerEvent(
        static_cast<const WebPointerEvent&>(input_event),
        Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  }

  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
    return OverlayMainFrame()->GetEventHandler().KeyEvent(
        static_cast<const WebKeyboardEvent&>(input_event));
  }

  if (input_event.GetType() == WebInputEvent::Type::kMouseWheel) {
    return OverlayMainFrame()->GetEventHandler().HandleWheelEvent(
        static_cast<const WebMouseWheelEvent&>(input_event));
  }

  return WebInputEventResult::kNotHandled;
}

void InspectorOverlayAgent::ScheduleUpdate() {
  if (IsVisible()) {
    GetFrame()->GetPage()->GetChromeClient().ScheduleAnimation(
        GetFrame()->View());
  }
}

void InspectorOverlayAgent::PaintOverlayPage() {
  DCHECK(overlay_page_);

  LocalFrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = GetFrame();
  if (!view || !frame)
    return;

  LocalFrame* overlay_frame = OverlayMainFrame();
  // To make overlay render the same size text with any emulation scale,
  // compensate the emulation scale using page scale.
  float emulation_scale =
      frame->GetPage()->GetChromeClient().InputEventsScaleForEmulation();
  IntSize viewport_size = frame->GetPage()->GetVisualViewport().Size();
  viewport_size.Scale(emulation_scale);
  overlay_page_->GetVisualViewport().SetSize(viewport_size);
  overlay_page_->SetDefaultPageScaleLimits(1 / emulation_scale,
                                           1 / emulation_scale);
  overlay_page_->GetVisualViewport().SetScale(1 / emulation_scale);

  overlay_frame->SetPageZoomFactor(WindowToViewportScale());
  overlay_frame->View()->Resize(viewport_size);

  Reset(viewport_size);

  if (inspect_tool_)
    inspect_tool_->Draw(WindowToViewportScale());
  if (hinge_)
    hinge_->Draw(WindowToViewportScale());

  OverlayMainFrame()->View()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kInspector);
}

static std::unique_ptr<protocol::DictionaryValue> BuildObjectForSize(
    const IntSize& size) {
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  result->setInteger("width", size.Width());
  result->setInteger("height", size.Height());
  return result;
}

float InspectorOverlayAgent::WindowToViewportScale() const {
  LocalFrame* frame = GetFrame();
  if (!frame)
    return 1.0f;
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(frame,
                                                                    1.0f);
}

void InspectorOverlayAgent::LoadFrameForTool(int data_resource_id) {
  if (frame_resource_name_ == data_resource_id)
    return;

  frame_resource_name_ = data_resource_id;

  if (overlay_page_) {
    overlay_page_->WillBeDestroyed();
    overlay_page_.Clear();
    overlay_chrome_client_.Clear();
    overlay_host_->ClearDelegate();
    overlay_host_.Clear();
  }

  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  DCHECK(!overlay_chrome_client_);
  overlay_chrome_client_ = MakeGarbageCollected<InspectorOverlayChromeClient>(
      GetFrame()->GetPage()->GetChromeClient(), *this);
  page_clients.chrome_client = overlay_chrome_client_.Get();
  overlay_page_ = Page::CreateNonOrdinary(page_clients);
  overlay_host_ = MakeGarbageCollected<InspectorOverlayHost>(this);

  Settings& settings = GetFrame()->GetPage()->GetSettings();
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

  DEFINE_STATIC_LOCAL(Persistent<LocalFrameClient>, dummy_local_frame_client,
                      (MakeGarbageCollected<EmptyLocalFrameClient>()));
  auto* frame = MakeGarbageCollected<LocalFrame>(
      dummy_local_frame_client, *overlay_page_, nullptr,
      base::UnguessableToken::Create(), nullptr, nullptr);
  frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame));
  frame->Init();
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append("<style>", static_cast<size_t>(7));
  data->Append(UncompressResourceAsBinary(IDR_INSPECT_TOOL_COMMON_CSS));
  data->Append("</style>", static_cast<size_t>(8));
  data->Append("<script>", static_cast<size_t>(8));
  data->Append(UncompressResourceAsBinary(IDR_INSPECT_TOOL_COMMON_JS));
  data->Append("</script>", static_cast<size_t>(9));
  data->Append(UncompressResourceAsBinary(frame_resource_name_));

  frame->ForceSynchronousDocumentInstall("text/html", data);

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
}

LocalFrame* InspectorOverlayAgent::OverlayMainFrame() {
  DCHECK(overlay_page_);
  return To<LocalFrame>(overlay_page_->MainFrame());
}

void InspectorOverlayAgent::Reset(const IntSize& viewport_size) {
  std::unique_ptr<protocol::DictionaryValue> reset_data =
      protocol::DictionaryValue::create();
  reset_data->setDouble("deviceScaleFactor",
                        GetFrame()->GetPage()->DeviceScaleFactorDeprecated());
  reset_data->setDouble(
      "emulationScaleFactor",
      GetFrame()->GetPage()->GetChromeClient().InputEventsScaleForEmulation());
  reset_data->setDouble("pageScaleFactor",
                        GetFrame()->GetPage()->GetVisualViewport().Scale());

  IntRect viewport_in_screen =
      GetFrame()->GetPage()->GetChromeClient().ViewportToScreen(
          IntRect(IntPoint(), viewport_size), GetFrame()->View());
  reset_data->setObject("viewportSize",
                        BuildObjectForSize(viewport_in_screen.Size()));

  // The zoom factor in the overlay frame already has been multiplied by the
  // window to viewport scale (aka device scale factor), so cancel it.
  reset_data->setDouble("pageZoomFactor",
                        GetFrame()->PageZoomFactor() / WindowToViewportScale());

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
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(command->Serialize()), &json);
  To<LocalFrame>(OverlayMainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" +
              String(reinterpret_cast<const char*>(json.data()), json.size()) +
              ")",
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
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(command->Serialize()), &json);
  To<LocalFrame>(OverlayMainFrame())
      ->GetScriptController()
      .ExecuteScriptInMainWorld(
          "dispatch(" +
              String(reinterpret_cast<const char*>(json.data()), json.size()) +
              ")",
          ScriptSourceLocationType::kInspector,
          ScriptController::kExecuteScriptWhenScriptsDisabled);
}

String InspectorOverlayAgent::EvaluateInOverlayForTest(const String& script) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::HandleScope handle_scope(ToIsolate(OverlayMainFrame()));
  v8::Local<v8::Value> string =
      To<LocalFrame>(OverlayMainFrame())
          ->GetScriptController()
          .ExecuteScriptInMainWorldAndReturnValue(
              ScriptSourceCode(script, ScriptSourceLocationType::kInspector),
              KURL(), SanitizeScriptErrors::kSanitize, ScriptFetchOptions(),
              ScriptController::kExecuteScriptWhenScriptsDisabled);
  return ToCoreStringWithUndefinedOrNullCheck(string);
}

void InspectorOverlayAgent::OnResizeTimer(TimerBase*) {
  if (resize_timer_active_) {
    // Restore the original tool.
    PickTheRightTool();
    return;
  }

  // Show the resize tool.
  SetInspectTool(MakeGarbageCollected<ShowViewSizeTool>());
  resize_timer_active_ = true;
  resize_timer_.Stop();
  resize_timer_.StartOneShot(base::TimeDelta::FromSeconds(1), FROM_HERE);
}

void InspectorOverlayAgent::Dispatch(const String& message) {
  inspect_tool_->Dispatch(message);
}

void InspectorOverlayAgent::PageLayoutInvalidated(bool resized) {
  if (resized && show_size_on_resize_.Get()) {
    resize_timer_active_ = false;
    // Handle the resize in the next cycle to decouple overlay page rebuild from
    // the main page layout to avoid document lifecycle issues caused by
    // Microtask::PerformCheckpoint() called when we rebuild the overlay page.
    resize_timer_.Stop();
    resize_timer_.StartOneShot(base::TimeDelta::FromSeconds(0), FROM_HERE);
    return;
  }
  ScheduleUpdate();
}

Response InspectorOverlayAgent::CompositingEnabled() {
  bool main_frame = frame_impl_->ViewImpl() && !frame_impl_->Parent();
  if (!main_frame || !frame_impl_->ViewImpl()
                          ->GetPage()
                          ->GetSettings()
                          .GetAcceleratedCompositingEnabled())
    return Response::ServerError("Compositing mode is not supported");
  return Response::Success();
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

Response InspectorOverlayAgent::setInspectMode(
    const String& mode,
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object) {
  if (mode != protocol::Overlay::InspectModeEnum::None &&
      mode != protocol::Overlay::InspectModeEnum::SearchForNode &&
      mode != protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM &&
      mode != protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot &&
      mode != protocol::Overlay::InspectModeEnum::ShowDistances) {
    return Response::ServerError(
        String("Unknown mode \"" + mode + "\" was provided.").Utf8());
  }

  std::vector<uint8_t> serialized_config;
  if (highlight_inspector_object.isJust()) {
    highlight_inspector_object.fromJust()->AppendSerialized(&serialized_config);
  }
  std::unique_ptr<InspectorHighlightConfig> config;
  Response response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &config);
  if (!response.IsSuccess())
    return response;
  inspect_mode_.Set(mode);
  inspect_mode_protocol_config_.Set(serialized_config);

  PickTheRightTool();
  return Response::Success();
}

void InspectorOverlayAgent::PickTheRightTool() {
  InspectTool* inspect_tool = nullptr;

  String inspect_mode = inspect_mode_.Get();
  if (inspect_mode == protocol::Overlay::InspectModeEnum::SearchForNode ||
      inspect_mode ==
          protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM) {
    inspect_tool = MakeGarbageCollected<SearchingForNodeTool>(
        dom_agent_,
        inspect_mode ==
            protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM,
        inspect_mode_protocol_config_.Get());
  } else if (inspect_mode ==
             protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
    inspect_tool = MakeGarbageCollected<ScreenshotTool>();
  } else if (inspect_mode ==
             protocol::Overlay::InspectModeEnum::ShowDistances) {
    inspect_tool = MakeGarbageCollected<NearbyDistanceTool>();
  } else if (!paused_in_debugger_message_.Get().IsNull()) {
    inspect_tool = MakeGarbageCollected<PausedInDebuggerTool>(
        v8_session_, paused_in_debugger_message_.Get());
  }

  SetInspectTool(inspect_tool);
}

void InspectorOverlayAgent::DisableFrameOverlay() {
  if (IsVisible() || !frame_overlay_)
    return;

  frame_overlay_.reset();
  auto& client = GetFrame()->GetPage()->GetChromeClient();
  client.SetCursorOverridden(false);
  client.SetCursor(PointerCursor(), GetFrame());

  if (auto* frame_view = frame_impl_->GetFrameView())
    frame_view->SetPaintArtifactCompositorNeedsUpdate();
}

void InspectorOverlayAgent::EnsureEnableFrameOverlay() {
  if (frame_overlay_)
    return;

  frame_overlay_ = std::make_unique<FrameOverlay>(
      GetFrame(), std::make_unique<InspectorPageOverlayDelegate>(*this));
}

void InspectorOverlayAgent::SetInspectTool(InspectTool* inspect_tool) {
  LocalFrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = GetFrame();
  if (!view || !frame)
    return;

  if (inspect_tool_)
    inspect_tool_->Dispose();

  if (inspect_tool) {
    inspect_tool_ = inspect_tool;
    LoadFrameForTool(inspect_tool->GetDataResourceId());
    EnsureEnableFrameOverlay();
    inspect_tool->Init(this, GetFrontend());
  } else {
    inspect_tool_ = nullptr;
    if (!hinge_)
      DisableFrameOverlay();
  }
  ScheduleUpdate();
}

Response InspectorOverlayAgent::HighlightConfigFromInspectorObject(
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object,
    std::unique_ptr<InspectorHighlightConfig>* out_config) {
  if (!highlight_inspector_object.isJust()) {
    return Response::ServerError(
        "Internal error: highlight configuration parameter is missing");
  }
  protocol::Overlay::HighlightConfig* config =
      highlight_inspector_object.fromJust();

  namespace ColorFormatEnum = protocol::Overlay::ColorFormatEnum;

  String format = config->getColorFormat("hex");

  if (format != ColorFormatEnum::Rgb && format != ColorFormatEnum::Hex &&
      format != ColorFormatEnum::Hsl) {
    return Response::InvalidParams("Unknown color format");
  }

  *out_config = InspectorOverlayAgent::ToHighlightConfig(config);
  return Response::Success();
}

// static
std::unique_ptr<InspectorGridHighlightConfig>
InspectorOverlayAgent::ToGridHighlightConfig(
    protocol::Overlay::GridHighlightConfig* config) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorGridHighlightConfig> highlight_config =
      std::make_unique<InspectorGridHighlightConfig>();
  highlight_config->show_grid_extension_lines =
      config->getShowGridExtensionLines(false);
  highlight_config->grid_border_dash = config->getGridBorderDash(false);
  highlight_config->cell_border_dash = config->getCellBorderDash(false);
  highlight_config->grid_color =
      InspectorDOMAgent::ParseColor(config->getGridBorderColor(nullptr));
  highlight_config->cell_color =
      InspectorDOMAgent::ParseColor(config->getCellBorderColor(nullptr));
  highlight_config->row_gap_color =
      InspectorDOMAgent::ParseColor(config->getRowGapColor(nullptr));
  highlight_config->column_gap_color =
      InspectorDOMAgent::ParseColor(config->getColumnGapColor(nullptr));
  highlight_config->row_hatch_color =
      InspectorDOMAgent::ParseColor(config->getRowHatchColor(nullptr));
  highlight_config->column_hatch_color =
      InspectorDOMAgent::ParseColor(config->getColumnHatchColor(nullptr));
  return highlight_config;
}

// static
std::unique_ptr<InspectorHighlightConfig>
InspectorOverlayAgent::ToHighlightConfig(
    protocol::Overlay::HighlightConfig* config) {
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

  namespace ColorFormatEnum = protocol::Overlay::ColorFormatEnum;

  String format = config->getColorFormat("hex");

  if (format == ColorFormatEnum::Hsl) {
    highlight_config->color_format = ColorFormat::HSL;
  } else if (format == ColorFormatEnum::Rgb) {
    highlight_config->color_format = ColorFormat::RGB;
  } else {
    highlight_config->color_format = ColorFormat::HEX;
  }

  highlight_config->grid_highlight_config =
      InspectorOverlayAgent::ToGridHighlightConfig(
          config->getGridHighlightConfig(nullptr));
  return highlight_config;
}

void InspectorOverlayAgent::SetNeedsUnbufferedInput(bool unbuffered) {
  LocalFrame* frame = GetFrame();
  if (frame) {
    frame->GetPage()->GetChromeClient().SetNeedsUnbufferedInputForDebugger(
        frame, unbuffered);
  }
}

}  // namespace blink
