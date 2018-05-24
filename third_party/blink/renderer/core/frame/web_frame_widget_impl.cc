/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_host.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_impl.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {
namespace {
const int kCaretPadding = 10;
const float kIdealPaddingRatio = 0.3f;

// Returns a rect which is offset and scaled accordingly to |base_rect|'s
// location and size.
FloatRect NormalizeRect(const FloatRect& to_normalize,
                        const FloatRect& base_rect) {
  FloatRect result(to_normalize);
  result.SetLocation(to_normalize.Location() + (-base_rect.Location()));
  result.Scale(1.0 / base_rect.Width(), 1.0 / base_rect.Height());
  return result;
}

}  // namespace

// WebFrameWidget ------------------------------------------------------------

WebFrameWidget* WebFrameWidget::Create(WebWidgetClient* client,
                                       WebLocalFrame* local_root) {
  DCHECK(client) << "A valid WebWidgetClient must be supplied.";
  WebFrameWidgetBase* widget;
  if (!local_root->Parent()) {
    // Note: this isn't a leak, as the object has a self-reference that the
    // caller needs to release by calling Close().
    WebLocalFrameImpl& main_frame = ToWebLocalFrameImpl(*local_root);
    DCHECK(main_frame.ViewImpl());
    // Note: this can't DCHECK that the view's main frame points to
    // |main_frame|, as provisional frames violate this precondition.
    // TODO(dcheng): Remove the special bridge class for main frame widgets.
    widget = new WebViewFrameWidget(*client, *main_frame.ViewImpl());
  } else {
    DCHECK(local_root->Parent()->IsWebRemoteFrame())
        << "Only local roots can have web frame widgets.";
    // Note: this isn't a leak, as the object has a self-reference that the
    // caller needs to release by calling Close().
    widget = WebFrameWidgetImpl::Create(*client);
  }
  widget->BindLocalRoot(*local_root);
  return widget;
}

WebFrameWidgetImpl* WebFrameWidgetImpl::Create(WebWidgetClient& client) {
  // Pass the WebFrameWidgetImpl's self-reference from SelfKeepAlive to the
  // caller.
  return new WebFrameWidgetImpl(client);
}

WebFrameWidgetImpl::WebFrameWidgetImpl(WebWidgetClient& client)
    : WebFrameWidgetBase(client),
      mutator_(nullptr),
      layer_tree_view_(nullptr),
      root_layer_(nullptr),
      root_graphics_layer_(nullptr),
      is_accelerated_compositing_active_(false),
      layer_tree_view_closed_(false),
      suppress_next_keypress_event_(false),
      background_color_override_enabled_(false),
      background_color_override_(Color::kTransparent),
      base_background_color_override_enabled_(false),
      base_background_color_override_(Color::kTransparent),
      ime_accept_events_(true),
      self_keep_alive_(this) {}

WebFrameWidgetImpl::~WebFrameWidgetImpl() = default;

void WebFrameWidgetImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(mouse_capture_node_);
  WebFrameWidgetBase::Trace(visitor);
}

// WebWidget ------------------------------------------------------------------

void WebFrameWidgetImpl::Close() {
  WebFrameWidgetBase::Close();

  mutator_ = nullptr;
  layer_tree_view_ = nullptr;
  root_layer_ = nullptr;
  root_graphics_layer_ = nullptr;
  animation_host_ = nullptr;

  self_keep_alive_.Clear();
}

WebSize WebFrameWidgetImpl::Size() {
  return size_ ? *size_ : WebSize();
}

void WebFrameWidgetImpl::Resize(const WebSize& new_size) {
  if (size_ && *size_ == new_size)
    return;

  if (did_suspend_parsing_) {
    did_suspend_parsing_ = false;
    LocalRootImpl()->GetFrame()->Loader().GetDocumentLoader()->ResumeParser();
  }

  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  if (!view)
    return;

  size_ = new_size;

  UpdateMainFrameLayoutSize();

  view->Resize(*size_);

  // FIXME: In WebViewImpl this layout was a precursor to setting the minimum
  // scale limit.  It is not clear if this is necessary for frame-level widget
  // resize.
  if (view->NeedsLayout())
    view->UpdateLayout();

  // FIXME: Investigate whether this is needed; comment from eseidel suggests
  // that this function is flawed.
  // TODO(kenrb): It would probably make more sense to check whether lifecycle
  // updates are throttled in the root's LocalFrameView, but for OOPIFs that
  // doesn't happen. Need to investigate if OOPIFs can be throttled during
  // load.
  if (LocalRootImpl()->GetFrame()->GetDocument()->IsLoadCompleted())
    SendResizeEventAndRepaint();
}

void WebFrameWidgetImpl::SendResizeEventAndRepaint() {
  // FIXME: This is wrong. The LocalFrameView is responsible sending a
  // resizeEvent as part of layout. Layout is also responsible for sending
  // invalidations to the embedder. This method and all callers may be wrong. --
  // eseidel.
  if (LocalRootImpl()->GetFrameView()) {
    // Enqueues the resize event.
    LocalRootImpl()->GetFrame()->GetDocument()->EnqueueResizeEvent();
  }

  DCHECK(Client());
  if (IsAcceleratedCompositingActive()) {
    UpdateLayerTreeViewport();
  } else {
    WebRect damaged_rect(0, 0, size_->width, size_->height);
    Client()->DidInvalidateRect(damaged_rect);
  }
}

void WebFrameWidgetImpl::ResizeVisualViewport(const WebSize& new_size) {
  if (!LocalRootImpl()) {
    // We should figure out why we get here when there is no local root
    // (https://crbug.com/792345).
    return;
  }

  // TODO(alexmos, kenrb): resizing behavior such as this should be changed
  // to use Page messages.  This uses the visual viewport size to set size on
  // both the WebViewImpl size and the Page's VisualViewport. If there are
  // multiple OOPIFs on a page, this will currently be set redundantly by
  // each of them. See https://crbug.com/599688.
  View()->Resize(new_size);

  View()->DidUpdateFullscreenSize();
}

void WebFrameWidgetImpl::UpdateMainFrameLayoutSize() {
  if (!LocalRootImpl())
    return;

  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  if (!view)
    return;

  WebSize layout_size = *size_;

  view->SetLayoutSize(layout_size);
}

void WebFrameWidgetImpl::DidEnterFullscreen() {
  View()->DidEnterFullscreen();
}

void WebFrameWidgetImpl::DidExitFullscreen() {
  View()->DidExitFullscreen();
}

void WebFrameWidgetImpl::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  GetPage()->Animator().SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebFrameWidgetImpl::BeginFrame(base::TimeTicks last_frame_time) {
  TRACE_EVENT1("blink", "WebFrameWidgetImpl::beginFrame", "frameTime",
               last_frame_time);
  DCHECK(!last_frame_time.is_null());

  if (!LocalRootImpl())
    return;

  UpdateGestureAnimation(last_frame_time);

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      LocalRootImpl()->GetFrame()->GetDocument()->Lifecycle());
  PageWidgetDelegate::Animate(*GetPage(), last_frame_time);
  // Animate can cause the local frame to detach.
  if (LocalRootImpl())
    GetPage()->GetValidationMessageClient().LayoutOverlay();
}

void WebFrameWidgetImpl::UpdateLifecycle(LifecycleUpdate requested_update) {
  TRACE_EVENT0("blink", "WebFrameWidgetImpl::updateAllLifecyclePhases");
  if (!LocalRootImpl())
    return;

  bool pre_paint_only = requested_update == LifecycleUpdate::kPrePaint;

  WebDevToolsAgentImpl* devtools = LocalRootImpl()->DevToolsAgentImpl();
  if (devtools && !pre_paint_only)
    devtools->PaintOverlay();

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      LocalRootImpl()->GetFrame()->GetDocument()->Lifecycle());
  PageWidgetDelegate::UpdateLifecycle(*GetPage(), *LocalRootImpl()->GetFrame(),
                                      requested_update);
  UpdateLayerTreeBackgroundColor();
}

void WebFrameWidgetImpl::UpdateAllLifecyclePhasesAndCompositeForTesting() {
  if (layer_tree_view_)
    layer_tree_view_->SynchronouslyCompositeNoRasterForTesting();
}

void WebFrameWidgetImpl::Paint(WebCanvas* canvas, const WebRect& rect) {
  // Out-of-process iframes require compositing.
  NOTREACHED();
}

void WebFrameWidgetImpl::UpdateLayerTreeViewport() {
  if (!GetPage() || !layer_tree_view_)
    return;

  // Pass the limits even though this is for subframes, as the limits will be
  // needed in setting the raster scale.
  layer_tree_view_->SetPageScaleFactorAndLimits(
      1, View()->MinimumPageScaleFactor(), View()->MaximumPageScaleFactor());
}

void WebFrameWidgetImpl::UpdateLayerTreeBackgroundColor() {
  if (!layer_tree_view_)
    return;

  SkColor color = BackgroundColor();
  layer_tree_view_->SetBackgroundColor(color);
}

void WebFrameWidgetImpl::SetBackgroundColorOverride(SkColor color) {
  background_color_override_enabled_ = true;
  background_color_override_ = color;
  UpdateLayerTreeBackgroundColor();
}

void WebFrameWidgetImpl::ClearBackgroundColorOverride() {
  background_color_override_enabled_ = false;
  UpdateLayerTreeBackgroundColor();
}

void WebFrameWidgetImpl::SetBaseBackgroundColorOverride(SkColor color) {
  if (base_background_color_override_enabled_ &&
      base_background_color_override_ == color) {
    return;
  }

  base_background_color_override_enabled_ = true;
  base_background_color_override_ = color;
  // Force lifecycle update to ensure we're good to call
  // LocalFrameView::setBaseBackgroundColor().
  LocalRootImpl()
      ->GetFrameView()
      ->UpdateLifecycleToCompositingCleanPlusScrolling();
  UpdateBaseBackgroundColor();
}

void WebFrameWidgetImpl::ClearBaseBackgroundColorOverride() {
  if (!base_background_color_override_enabled_)
    return;

  base_background_color_override_enabled_ = false;
  // Force lifecycle update to ensure we're good to call
  // LocalFrameView::setBaseBackgroundColor().
  LocalRootImpl()
      ->GetFrameView()
      ->UpdateLifecycleToCompositingCleanPlusScrolling();
  UpdateBaseBackgroundColor();
}

void WebFrameWidgetImpl::LayoutAndPaintAsync(base::OnceClosure callback) {
  layer_tree_view_->LayoutAndPaintAsync(std::move(callback));
}

void WebFrameWidgetImpl::CompositeAndReadbackAsync(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  layer_tree_view_->CompositeAndReadbackAsync(std::move(callback));
}

void WebFrameWidgetImpl::ThemeChanged() {
  LocalFrameView* view = LocalRootImpl()->GetFrameView();

  WebRect damaged_rect(0, 0, size_->width, size_->height);
  view->InvalidateRect(damaged_rect);
}

WebHitTestResult WebFrameWidgetImpl::HitTestResultAt(const WebPoint& point) {
  return CoreHitTestResultAt(point);
}

WebInputEventResult WebFrameWidgetImpl::DispatchBufferedTouchEvents() {
  if (doing_drag_and_drop_)
    return WebInputEventResult::kHandledSuppressed;

  if (!GetPage())
    return WebInputEventResult::kNotHandled;

  if (LocalRootImpl()) {
    if (WebDevToolsAgentImpl* devtools = LocalRootImpl()->DevToolsAgentImpl())
      devtools->DispatchBufferedTouchEvents();
  }
  if (IgnoreInputEvents())
    return WebInputEventResult::kNotHandled;

  return LocalRootImpl()
      ->GetFrame()
      ->GetEventHandler()
      .DispatchBufferedTouchEvents();
}

WebInputEventResult WebFrameWidgetImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  TRACE_EVENT1("input", "WebFrameWidgetImpl::handleInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));
  DCHECK(!WebInputEvent::IsTouchEventType(input_event.GetType()));

  // If a drag-and-drop operation is in progress, ignore input events.
  if (doing_drag_and_drop_)
    return WebInputEventResult::kHandledSuppressed;

  // Don't handle events once we've started shutting down.
  if (!GetPage())
    return WebInputEventResult::kNotHandled;

  if (LocalRootImpl()) {
    if (WebDevToolsAgentImpl* devtools = LocalRootImpl()->DevToolsAgentImpl()) {
      if (devtools->HandleInputEvent(input_event))
        return WebInputEventResult::kHandledSuppressed;
    }
  }

  // Report the event to be NOT processed by WebKit, so that the browser can
  // handle it appropriately.
  if (IgnoreInputEvents())
    return WebInputEventResult::kNotHandled;

  // FIXME: pass event to m_localRoot's WebDevToolsAgentImpl once available.

  base::AutoReset<const WebInputEvent*> current_event_change(
      &CurrentInputEvent::current_input_event_, &input_event);

  DCHECK(Client());
  if (Client()->IsPointerLocked() &&
      WebInputEvent::IsMouseEventType(input_event.GetType())) {
    PointerLockMouseEvent(coalesced_event);
    return WebInputEventResult::kHandledSystem;
  }

  if (mouse_capture_node_ &&
      WebInputEvent::IsMouseEventType(input_event.GetType())) {
    TRACE_EVENT1("input", "captured mouse event", "type",
                 input_event.GetType());
    // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
    Node* node = mouse_capture_node_;

    // Not all platforms call mouseCaptureLost() directly.
    if (input_event.GetType() == WebInputEvent::kMouseUp)
      MouseCaptureLost();

    std::unique_ptr<UserGestureIndicator> gesture_indicator;

    AtomicString event_type;
    switch (input_event.GetType()) {
      case WebInputEvent::kMouseEnter:
        event_type = EventTypeNames::mouseover;
        break;
      case WebInputEvent::kMouseMove:
        event_type = EventTypeNames::mousemove;
        break;
      case WebInputEvent::kMouseLeave:
        event_type = EventTypeNames::mouseout;
        break;
      case WebInputEvent::kMouseDown:
        event_type = EventTypeNames::mousedown;
        gesture_indicator = Frame::NotifyUserActivation(
            node->GetDocument().GetFrame(), UserGestureToken::kNewGesture);
        mouse_capture_gesture_token_ = gesture_indicator->CurrentToken();
        break;
      case WebInputEvent::kMouseUp:
        event_type = EventTypeNames::mouseup;
        gesture_indicator = std::make_unique<UserGestureIndicator>(
            std::move(mouse_capture_gesture_token_));
        break;
      default:
        NOTREACHED();
    }

    WebMouseEvent transformed_event =
        TransformWebMouseEvent(LocalRootImpl()->GetFrameView(),
                               static_cast<const WebMouseEvent&>(input_event));
    node->DispatchMouseEvent(transformed_event, event_type,
                             transformed_event.click_count);
    return WebInputEventResult::kHandledSystem;
  }

  return PageWidgetDelegate::HandleInputEvent(*this, coalesced_event,
                                              LocalRootImpl()->GetFrame());
}

void WebFrameWidgetImpl::SetCursorVisibilityState(bool is_visible) {
  GetPage()->SetIsCursorVisible(is_visible);
}

Color WebFrameWidgetImpl::BaseBackgroundColor() const {
  return base_background_color_override_enabled_
             ? base_background_color_override_
             : base_background_color_;
}

void WebFrameWidgetImpl::SetBaseBackgroundColor(SkColor color) {
  if (base_background_color_ == color)
    return;

  base_background_color_ = color;
  UpdateBaseBackgroundColor();
}

void WebFrameWidgetImpl::UpdateBaseBackgroundColor() {
  LocalRootImpl()->GetFrameView()->SetBaseBackgroundColor(
      BaseBackgroundColor());
}

WebInputMethodController*
WebFrameWidgetImpl::GetActiveWebInputMethodController() const {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
  return local_frame ? local_frame->GetInputMethodController() : nullptr;
}

bool WebFrameWidgetImpl::ScrollFocusedEditableElementIntoView() {
  Element* element = FocusedElement();
  if (!element || !WebElement(element).IsEditable())
    return false;

  if (!element->GetLayoutObject())
    return false;

  LayoutRect rect_to_scroll;
  WebScrollIntoViewParams params;
  GetScrollParamsForFocusedEditableElement(*element, rect_to_scroll, params);
  element->GetLayoutObject()->ScrollRectToVisible(rect_to_scroll, params);
  return true;
}

void WebFrameWidgetImpl::Initialize() {
  InitializeLayerTreeView();

  if (LocalRoot()->Parent())
    SetBackgroundColorOverride(Color::kTransparent);
}

void WebFrameWidgetImpl::ScheduleAnimation() {
  if (layer_tree_view_) {
    layer_tree_view_->SetNeedsBeginFrame();
    return;
  }
  DCHECK(Client());
  Client()->ScheduleAnimation();
}

void WebFrameWidgetImpl::IntrinsicSizingInfoChanged(
    const IntrinsicSizingInfo& sizing_info) {
  WebIntrinsicSizingInfo web_sizing_info;
  web_sizing_info.size = sizing_info.size;
  web_sizing_info.aspect_ratio = sizing_info.aspect_ratio;
  web_sizing_info.has_width = sizing_info.has_width;
  web_sizing_info.has_height = sizing_info.has_height;
  Client()->IntrinsicSizingInfoChanged(web_sizing_info);
}

base::WeakPtr<CompositorMutatorImpl>
WebFrameWidgetImpl::EnsureCompositorMutator(
    scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) {
  if (!mutator_task_runner_) {
    layer_tree_view_->SetMutatorClient(
        CompositorMutatorImpl::CreateClient(&mutator_, &mutator_task_runner_));
  }

  DCHECK(mutator_task_runner_);
  *mutator_task_runner = mutator_task_runner_;
  return mutator_;
}

void WebFrameWidgetImpl::ApplyViewportDeltas(
    const WebFloatSize& visual_viewport_delta,
    const WebFloatSize& main_frame_delta,
    const WebFloatSize& elastic_overscroll_delta,
    float page_scale_delta,
    float browser_controls_delta) {
  // FIXME: To be implemented.
}

void WebFrameWidgetImpl::MouseCaptureLost() {
  TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
  mouse_capture_node_ = nullptr;
}

void WebFrameWidgetImpl::SetFocus(bool enable) {
  if (enable)
    GetPage()->GetFocusController().SetActive(true);
  GetPage()->GetFocusController().SetFocused(enable);
  if (enable) {
    LocalFrame* focused_frame = GetPage()->GetFocusController().FocusedFrame();
    if (focused_frame) {
      Element* element = focused_frame->GetDocument()->FocusedElement();
      if (element && focused_frame->Selection()
                         .ComputeVisibleSelectionInDOMTreeDeprecated()
                         .IsNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        focused_frame->GetDocument()->UpdateStyleAndLayoutTree();
        if (element->IsTextControl()) {
          element->UpdateFocusAppearance(SelectionBehaviorOnFocus::kRestore);
        } else if (HasEditableStyle(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focused_frame->Selection().SetSelectionAndEndTyping(
              SelectionInDOMTree::Builder().Collapse(position).Build());
        }
      }
    }
    ime_accept_events_ = true;
  } else {
    LocalFrame* focused_frame = FocusedLocalFrameInWidget();
    if (focused_frame) {
      // Finish an ongoing composition to delete the composition node.
      if (focused_frame->GetInputMethodController().HasComposition()) {
        // TODO(editing-dev): The use of
        // updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
        // See http://crbug.com/590369 for more details.
        focused_frame->GetDocument()
            ->UpdateStyleAndLayoutIgnorePendingStylesheets();

        focused_frame->GetInputMethodController().FinishComposingText(
            InputMethodController::kKeepSelection);
      }
      ime_accept_events_ = false;
    }
  }
}

SkColor WebFrameWidgetImpl::BackgroundColor() const {
  if (background_color_override_enabled_)
    return background_color_override_;
  if (!LocalRootImpl()->GetFrameView())
    return base_background_color_;
  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  return view->DocumentBackgroundColor().Rgb();
}

bool WebFrameWidgetImpl::SelectionBounds(WebRect& anchor_web,
                                         WebRect& focus_web) const {
  const LocalFrame* local_frame = FocusedLocalFrameInWidget();
  if (!local_frame)
    return false;

  IntRect anchor;
  IntRect focus;
  if (!local_frame->Selection().ComputeAbsoluteBounds(anchor, focus))
    return false;

  // FIXME: This doesn't apply page scale. This should probably be contents to
  // viewport. crbug.com/459293.
  anchor_web = local_frame->View()->ContentsToRootFrame(anchor);
  focus_web = local_frame->View()->ContentsToRootFrame(focus);
  return true;
}

bool WebFrameWidgetImpl::IsAcceleratedCompositingActive() const {
  return is_accelerated_compositing_active_;
}

void WebFrameWidgetImpl::WillCloseLayerTreeView() {
  if (layer_tree_view_) {
    GetPage()->WillCloseLayerTreeView(*layer_tree_view_,
                                      LocalRootImpl()->GetFrame()->View());
  }

  SetIsAcceleratedCompositingActive(false);
  mutator_ = nullptr;
  layer_tree_view_ = nullptr;
  animation_host_ = nullptr;
  layer_tree_view_closed_ = true;
}

void WebFrameWidgetImpl::SetRemoteViewportIntersection(
    const WebRect& viewport_intersection) {
  // Remote viewports are only applicable to local frames with remote ancestors.
  DCHECK(LocalRootImpl()->Parent() &&
         LocalRootImpl()->Parent()->IsWebRemoteFrame() &&
         LocalRootImpl()->GetFrame());

  LocalRootImpl()->GetFrame()->SetViewportIntersectionFromParent(
      viewport_intersection);
}

void WebFrameWidgetImpl::SetIsInert(bool inert) {
  DCHECK(LocalRootImpl()->Parent());
  DCHECK(LocalRootImpl()->Parent()->IsWebRemoteFrame());
  LocalRootImpl()->GetFrame()->SetIsInert(inert);
}

void WebFrameWidgetImpl::SetInheritedEffectiveTouchAction(
    TouchAction touch_action) {
  DCHECK(LocalRootImpl()->Parent());
  DCHECK(LocalRootImpl()->Parent()->IsWebRemoteFrame());
  LocalRootImpl()->GetFrame()->SetInheritedEffectiveTouchAction(touch_action);
}

void WebFrameWidgetImpl::UpdateRenderThrottlingStatus(bool is_throttled,
                                                      bool subtree_throttled) {
  DCHECK(LocalRootImpl()->Parent());
  DCHECK(LocalRootImpl()->Parent()->IsWebRemoteFrame());
  LocalRootImpl()->GetFrameView()->UpdateRenderThrottlingStatus(
      is_throttled, subtree_throttled);
}

void WebFrameWidgetImpl::HandleMouseLeave(LocalFrame& main_frame,
                                          const WebMouseEvent& event) {
  // FIXME: WebWidget doesn't have the method below.
  // m_client->setMouseOverURL(WebURL());
  PageWidgetEventHandler::HandleMouseLeave(main_frame, event);
}

void WebFrameWidgetImpl::HandleMouseDown(LocalFrame& main_frame,
                                         const WebMouseEvent& event) {
  WebViewImpl* view_impl = View();
  // If there is a popup open, close it as the user is clicking on the page
  // (outside of the popup). We also save it so we can prevent a click on an
  // element from immediately reopening the same popup.
  scoped_refptr<WebPagePopupImpl> page_popup;
  if (event.button == WebMouseEvent::Button::kLeft) {
    page_popup = view_impl->GetPagePopup();
    view_impl->HidePopups();
  }

  // Take capture on a mouse down on a plugin so we can send it mouse events.
  // If the hit node is a plugin but a scrollbar is over it don't start mouse
  // capture because it will interfere with the scrollbar receiving events.
  LayoutPoint point(event.PositionInWidget().x, event.PositionInWidget().y);
  if (event.button == WebMouseEvent::Button::kLeft) {
    point = LocalRootImpl()->GetFrameView()->RootFrameToContents(point);
    HitTestResult result(
        LocalRootImpl()->GetFrame()->GetEventHandler().HitTestResultAtPoint(
            point));
    result.SetToShadowHostIfInRestrictedShadowRoot();
    Node* hit_node = result.InnerNode();

    if (!result.GetScrollbar() && hit_node && hit_node->GetLayoutObject() &&
        hit_node->GetLayoutObject()->IsEmbeddedObject()) {
      mouse_capture_node_ = hit_node;
      TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
    }
  }

  PageWidgetEventHandler::HandleMouseDown(main_frame, event);

  if (event.button == WebMouseEvent::Button::kLeft && mouse_capture_node_) {
    mouse_capture_gesture_token_ =
        main_frame.GetEventHandler().TakeLastMouseDownGestureToken();
  }

  if (view_impl->GetPagePopup() && page_popup &&
      view_impl->GetPagePopup()->HasSamePopupClient(page_popup.get())) {
    // That click triggered a page popup that is the same as the one we just
    // closed.  It needs to be closed.
    view_impl->HidePopups();
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (!GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
#if defined(OS_MACOSX)
    if (event.button == WebMouseEvent::Button::kRight ||
        (event.button == WebMouseEvent::Button::kLeft &&
         event.GetModifiers() & WebMouseEvent::kControlKey))
      MouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
#endif
  }
}

void WebFrameWidgetImpl::MouseContextMenu(const WebMouseEvent& event) {
  GetPage()->GetContextMenuController().ClearContextMenu();

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(LocalRootImpl()->GetFrameView(), event);
  transformed_event.menu_source_type = kMenuSourceMouse;
  IntPoint position_in_root_frame =
      FlooredIntPoint(transformed_event.PositionInRootFrame());

  // Find the right target frame. See issue 1186900.
  HitTestResult result = HitTestResultForRootFramePos(position_in_root_frame);
  Frame* target_frame;
  if (result.InnerNodeOrImageMapImage())
    target_frame = result.InnerNodeOrImageMapImage()->GetDocument().GetFrame();
  else
    target_frame = GetPage()->GetFocusController().FocusedOrMainFrame();

  // This will need to be changed to a nullptr check when focus control
  // is refactored, at which point focusedOrMainFrame will never return a
  // RemoteFrame.
  // See https://crbug.com/341918.
  if (!target_frame->IsLocalFrame())
    return;

  LocalFrame* target_local_frame = ToLocalFrame(target_frame);

  {
    ContextMenuAllowedScope scope;
    target_local_frame->GetEventHandler().SendContextMenuEvent(
        transformed_event, nullptr);
  }
  // Actually showing the context menu is handled by the ContextMenuClient
  // implementation...
}

void WebFrameWidgetImpl::HandleMouseUp(LocalFrame& main_frame,
                                       const WebMouseEvent& event) {
  PageWidgetEventHandler::HandleMouseUp(main_frame, event);

  if (GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
  }
}

WebInputEventResult WebFrameWidgetImpl::HandleMouseWheel(
    LocalFrame& frame,
    const WebMouseWheelEvent& event) {
  // Halt an in-progress fling on a wheel tick.
  if (!event.has_precise_scrolling_deltas)
    EndActiveFlingAnimation();

  View()->HidePopups();
  return PageWidgetEventHandler::HandleMouseWheel(frame, event);
}

WebInputEventResult WebFrameWidgetImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  DCHECK(Client());
  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  bool event_cancelled = false;
  base::Optional<ContextMenuAllowedScope> maybe_context_menu_scope;

  WebViewImpl* view_impl = View();
  switch (event.GetType()) {
    case WebInputEvent::kGestureScrollBegin:
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureScrollUpdate:
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapDown:
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // When we close a popup because of a GestureTapDown, we also save it so
      // we can prevent the following GestureTap from immediately reopening the
      // same popup.
      view_impl->SetLastHiddenPagePopup(view_impl->GetPagePopup());
      View()->HidePopups();
      FALLTHROUGH;
    case WebInputEvent::kGestureTapCancel:
      View()->SetLastHiddenPagePopup(nullptr);
      break;
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureDoubleTap:
      break;
    case WebInputEvent::kGestureTwoFingerTap:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
      GetPage()->GetContextMenuController().ClearContextMenu();
      maybe_context_menu_scope.emplace();
      break;
    case WebInputEvent::kGestureFlingStart:
    case WebInputEvent::kGestureFlingCancel:
      event_result = HandleGestureFlingEvent(event);
      Client()->DidHandleGestureEvent(event, event_cancelled);
      return event_result;
    default:
      NOTREACHED();
  }
  LocalFrame* frame = LocalRootImpl()->GetFrame();
  WebGestureEvent scaled_event = TransformWebGestureEvent(frame->View(), event);
  event_result = frame->GetEventHandler().HandleGestureEvent(scaled_event);
  Client()->DidHandleGestureEvent(event, event_cancelled);
  return event_result;
}

PageWidgetEventHandler* WebFrameWidgetImpl::GetPageWidgetEventHandler() {
  return this;
}

WebInputEventResult WebFrameWidgetImpl::HandleKeyEvent(
    const WebKeyboardEvent& event) {
  DCHECK((event.GetType() == WebInputEvent::kRawKeyDown) ||
         (event.GetType() == WebInputEvent::kKeyDown) ||
         (event.GetType() == WebInputEvent::kKeyUp));

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.
  // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  Frame* focused_frame = FocusedCoreFrame();
  if (!focused_frame || !focused_frame->IsLocalFrame())
    return WebInputEventResult::kNotHandled;

  LocalFrame* frame = ToLocalFrame(focused_frame);

  WebInputEventResult result = frame->GetEventHandler().KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled) {
    if (WebInputEvent::kRawKeyDown == event.GetType()) {
      // Suppress the next keypress event unless the focused node is a plugin
      // node.  (Flash needs these keypress events to handle non-US keyboards.)
      Element* element = FocusedElement();
      if (!element || !element->GetLayoutObject() ||
          !element->GetLayoutObject()->IsEmbeddedObject())
        suppress_next_keypress_event_ = true;
    }
    return result;
  }

#if !defined(OS_MACOSX)
  const WebInputEvent::Type kContextMenuKeyTriggeringEventType =
#if defined(OS_WIN)
      WebInputEvent::kKeyUp;
#else
      WebInputEvent::kRawKeyDown;
#endif
  const WebInputEvent::Type kShiftF10TriggeringEventType =
      WebInputEvent::kRawKeyDown;

  bool is_unmodified_menu_key =
      !(event.GetModifiers() & WebInputEvent::kInputModifiers) &&
      event.windows_key_code == VKEY_APPS;
  bool is_shift_f10 = (event.GetModifiers() & WebInputEvent::kInputModifiers) ==
                          WebInputEvent::kShiftKey &&
                      event.windows_key_code == VKEY_F10;
  if ((is_unmodified_menu_key &&
       event.GetType() == kContextMenuKeyTriggeringEventType) ||
      (is_shift_f10 && event.GetType() == kShiftF10TriggeringEventType)) {
    View()->SendContextMenuEvent();
    return WebInputEventResult::kHandledSystem;
  }
#endif  // !defined(OS_MACOSX)

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult WebFrameWidgetImpl::HandleCharEvent(
    const WebKeyboardEvent& event) {
  DCHECK_EQ(event.GetType(), WebInputEvent::kChar);

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
  // handled by Webkit. A keyDown event is typically associated with a
  // keyPress(char) event and a keyUp event. We reset this flag here as it
  // only applies to the current keyPress event.
  bool suppress = suppress_next_keypress_event_;
  suppress_next_keypress_event_ = false;

  LocalFrame* frame = ToLocalFrame(FocusedCoreFrame());
  if (!frame) {
    return suppress ? WebInputEventResult::kHandledSuppressed
                    : WebInputEventResult::kNotHandled;
  }

  EventHandler& handler = frame->GetEventHandler();

  if (!event.IsCharacterKey())
    return WebInputEventResult::kHandledSuppressed;

  // Accesskeys are triggered by char events and can't be suppressed.
  // It is unclear whether a keypress should be dispatched as well
  // crbug.com/563507
  if (handler.HandleAccessKey(event))
    return WebInputEventResult::kHandledSystem;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (event.is_system_key)
    return WebInputEventResult::kNotHandled;

  if (suppress)
    return WebInputEventResult::kHandledSuppressed;

  WebInputEventResult result = handler.KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled)
    return result;

  return WebInputEventResult::kNotHandled;
}

Frame* WebFrameWidgetImpl::FocusedCoreFrame() const {
  return GetPage() ? GetPage()->GetFocusController().FocusedOrMainFrame()
                   : nullptr;
}

Element* WebFrameWidgetImpl::FocusedElement() const {
  LocalFrame* frame = GetPage()->GetFocusController().FocusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;

  return document->FocusedElement();
}

void WebFrameWidgetImpl::InitializeLayerTreeView() {
  DCHECK(Client());
  DCHECK(!mutator_);
  layer_tree_view_ = Client()->InitializeLayerTreeView();
  DCHECK(layer_tree_view_);
  if (layer_tree_view_->CompositorAnimationHost()) {
    animation_host_ = std::make_unique<CompositorAnimationHost>(
        layer_tree_view_->CompositorAnimationHost());
  }

  GetPage()->LayerTreeViewInitialized(*layer_tree_view_,
                                      LocalRootImpl()->GetFrame()->View());

  // TODO(kenrb): Currently GPU rasterization is always enabled for OOPIFs.
  // This is okay because it is only necessarily to set the trigger to false
  // for certain cases that affect the top-level frame, but it would be better
  // to be consistent with the top-level frame. Ideally the logic should
  // be moved from WebViewImpl into WebFrameWidget and used for all local
  // frame roots. https://crbug.com/712794
  layer_tree_view_->HeuristicsForGpuRasterizationUpdated(true);
}

void WebFrameWidgetImpl::SetIsAcceleratedCompositingActive(bool active) {
  // In the middle of shutting down; don't try to spin back up a compositor.
  // FIXME: compositing startup/shutdown should be refactored so that it
  // turns on explicitly rather than lazily, which causes this awkwardness.
  if (layer_tree_view_closed_)
    return;

  DCHECK(!active || layer_tree_view_);

  if (is_accelerated_compositing_active_ == active)
    return;

  if (active) {
    TRACE_EVENT0("blink",
                 "WebViewImpl::setIsAcceleratedCompositingActive(true)");
    layer_tree_view_->SetRootLayer(root_layer_);

    layer_tree_view_->SetVisible(GetPage()->IsPageVisible());
    UpdateLayerTreeBackgroundColor();
    UpdateLayerTreeViewport();
    is_accelerated_compositing_active_ = true;
  }
}

PaintLayerCompositor* WebFrameWidgetImpl::Compositor() const {
  LocalFrame* frame = LocalRootImpl()->GetFrame();
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->GetLayoutView())
    return nullptr;

  return frame->GetDocument()->GetLayoutView()->Compositor();
}

void WebFrameWidgetImpl::SetRootGraphicsLayer(GraphicsLayer* layer) {
  root_graphics_layer_ = layer;
  root_layer_ = layer ? layer->CcLayer() : nullptr;

  SetIsAcceleratedCompositingActive(layer);

  if (!layer_tree_view_)
    return;

  if (root_layer_)
    layer_tree_view_->SetRootLayer(root_layer_);
  else
    layer_tree_view_->ClearRootLayer();
}

void WebFrameWidgetImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  root_layer_ = layer;

  SetIsAcceleratedCompositingActive(!!layer);

  if (!layer_tree_view_)
    return;

  if (root_layer_)
    layer_tree_view_->SetRootLayer(root_layer_);
  else
    layer_tree_view_->ClearRootLayer();
}

WebLayerTreeView* WebFrameWidgetImpl::GetLayerTreeView() const {
  return layer_tree_view_;
}

CompositorAnimationHost* WebFrameWidgetImpl::AnimationHost() const {
  return animation_host_.get();
}

HitTestResult WebFrameWidgetImpl::CoreHitTestResultAt(
    const WebPoint& point_in_viewport) {
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      LocalRootImpl()->GetFrame()->GetDocument()->Lifecycle());
  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  IntPoint point_in_root_frame =
      view->ContentsToFrame(view->ViewportToContents(point_in_viewport));
  return HitTestResultForRootFramePos(point_in_root_frame);
}

void WebFrameWidgetImpl::SetVisibilityState(
    mojom::PageVisibilityState visibility_state) {
  if (layer_tree_view_) {
    layer_tree_view_->SetVisible(visibility_state ==
                                 mojom::PageVisibilityState::kVisible);
  }
}

HitTestResult WebFrameWidgetImpl::HitTestResultForRootFramePos(
    const LayoutPoint& pos_in_root_frame) {
  LayoutPoint doc_point(
      LocalRootImpl()->GetFrame()->View()->RootFrameToContents(
          pos_in_root_frame));
  HitTestResult result =
      LocalRootImpl()->GetFrame()->GetEventHandler().HitTestResultAtPoint(
          doc_point, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

LocalFrame* WebFrameWidgetImpl::FocusedLocalFrameAvailableForIme() const {
  if (!ime_accept_events_)
    return nullptr;
  return FocusedLocalFrameInWidget();
}

void WebFrameWidgetImpl::DidCreateLocalRootView() {
  // If this WebWidget still hasn't received its size from the embedder, block
  // the parser. This is necessary, because the parser can cause layout to
  // happen, which needs to be done with the correct size.
  if (!size_) {
    did_suspend_parsing_ = true;
    LocalRootImpl()->GetFrame()->Loader().GetDocumentLoader()->BlockParser();
  }
}

void WebFrameWidgetImpl::GetScrollParamsForFocusedEditableElement(
    const Element& element,
    LayoutRect& rect_to_scroll,
    WebScrollIntoViewParams& params) {
  LocalFrameView& frame_view = *element.GetDocument().View();
  IntRect absolute_element_bounds =
      element.GetLayoutObject()->AbsoluteBoundingBoxRect();
  IntRect absolute_caret_bounds =
      element.GetDocument().GetFrame()->Selection().AbsoluteCaretBounds();
  // Ideally, the chosen rectangle includes the element box and caret bounds
  // plus some margin on the left. If this does not work (i.e., does not fit
  // inside the frame view), then choose a subrect which includes the caret
  // bounds. It is preferrable to also include element bounds' location and left
  // align the scroll. If this cant be satisfied, the scroll will be right
  // aligned.
  IntRect maximal_rect =
      UnionRect(absolute_element_bounds, absolute_caret_bounds);

  // Set the ideal margin.
  maximal_rect.ShiftXEdgeTo(
      maximal_rect.X() -
      static_cast<int>(kIdealPaddingRatio * absolute_element_bounds.Width()));

  bool maximal_rect_fits_in_frame =
      !(frame_view.Size() - maximal_rect.Size()).IsEmpty();

  if (!maximal_rect_fits_in_frame) {
    IntRect frame_rect(maximal_rect.Location(), frame_view.Size());
    maximal_rect.Intersect(frame_rect);
    IntPoint point_forced_to_be_visible =
        absolute_caret_bounds.MaxXMaxYCorner() +
        IntSize(kCaretPadding, kCaretPadding);
    if (!maximal_rect.Contains(point_forced_to_be_visible)) {
      // Move the rect towards the point until the point is barely contained.
      maximal_rect.Move(point_forced_to_be_visible -
                        maximal_rect.MaxXMaxYCorner());
    }
  }

  params.zoom_into_rect = View()->ShouldZoomToLegibleScale(element);
  params.relative_element_bounds = NormalizeRect(
      Intersection(absolute_element_bounds, maximal_rect), maximal_rect);
  params.relative_caret_bounds = NormalizeRect(
      Intersection(absolute_caret_bounds, maximal_rect), maximal_rect);
  params.behavior = WebScrollIntoViewParams::kInstant;
  rect_to_scroll = LayoutRect(maximal_rect);
}

}  // namespace blink
