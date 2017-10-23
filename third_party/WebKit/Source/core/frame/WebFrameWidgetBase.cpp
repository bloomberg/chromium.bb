// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/WebFrameWidgetBase.h"

#include "core/dom/Element.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/WebInputEventConversion.h"
#include "core/events/WheelEvent.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/input/ContextMenuAllowedScope.h"
#include "core/input/EventHandler.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragActions.h"
#include "core/page/DragController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "platform/exported/WebActiveGestureAnimation.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGestureCurve.h"
#include "public/web/WebActiveWheelFlingParameters.h"
#include "public/web/WebWidgetClient.h"

namespace blink {

namespace {

// Helper to get LocalFrame* from WebLocalFrame*.
// TODO(dcheng): This should be moved into WebLocalFrame.
LocalFrame* ToCoreFrame(WebLocalFrame* frame) {
  return ToWebLocalFrameImpl(frame)->GetFrame();
}

}  // namespace

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
STATIC_ASSERT_ENUM(kDragOperationNone, kWebDragOperationNone);
STATIC_ASSERT_ENUM(kDragOperationCopy, kWebDragOperationCopy);
STATIC_ASSERT_ENUM(kDragOperationLink, kWebDragOperationLink);
STATIC_ASSERT_ENUM(kDragOperationGeneric, kWebDragOperationGeneric);
STATIC_ASSERT_ENUM(kDragOperationPrivate, kWebDragOperationPrivate);
STATIC_ASSERT_ENUM(kDragOperationMove, kWebDragOperationMove);
STATIC_ASSERT_ENUM(kDragOperationDelete, kWebDragOperationDelete);
STATIC_ASSERT_ENUM(kDragOperationEvery, kWebDragOperationEvery);

bool WebFrameWidgetBase::ignore_input_events_ = false;

WebFrameWidgetBase::WebFrameWidgetBase()
    : fling_modifier_(0),
      fling_source_device_(kWebGestureDeviceUninitialized) {}

WebFrameWidgetBase::~WebFrameWidgetBase() {}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnter(
    const WebDragData& web_drag_data,
    const WebPoint& point_in_viewport,
    const WebPoint& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  DCHECK(!current_drag_data_);

  current_drag_data_ = DataObject::Create(web_drag_data);
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragEnter,
                                   modifiers);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragOver(
    const WebPoint& point_in_viewport,
    const WebPoint& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragOver,
                                   modifiers);
}

void WebFrameWidgetBase::DragTargetDragLeave(const WebPoint& point_in_viewport,
                                             const WebPoint& screen_point) {
  DCHECK(current_drag_data_);

  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-leave is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671152)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return;
  }

  WebPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));
  DragData drag_data(current_drag_data_.Get(), point_in_root_frame,
                     screen_point,
                     static_cast<DragOperation>(operations_allowed_));

  GetPage()->GetDragController().DragExited(&drag_data,
                                            *ToCoreFrame(LocalRoot()));

  // FIXME: why is the drag scroll timer not stopped here?

  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragTargetDrop(const WebDragData& web_drag_data,
                                        const WebPoint& point_in_viewport,
                                        const WebPoint& screen_point,
                                        int modifiers) {
  WebPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  DCHECK(current_drag_data_);
  current_drag_data_ = DataObject::Create(web_drag_data);

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview m_dragOperation state is not DragOperationNone.

  if (drag_operation_ == kWebDragOperationNone) {
    // IPC RACE CONDITION: do not allow this drop.
    DragTargetDragLeave(point_in_viewport, screen_point);
    return;
  }

  if (!IgnoreInputEvents()) {
    current_drag_data_->SetModifiers(modifiers);
    DragData drag_data(current_drag_data_.Get(), point_in_root_frame,
                       screen_point,
                       static_cast<DragOperation>(operations_allowed_));

    GetPage()->GetDragController().PerformDrag(&drag_data,
                                               *ToCoreFrame(LocalRoot()));
  }
  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragSourceEndedAt(const WebPoint& point_in_viewport,
                                           const WebPoint& screen_point,
                                           WebDragOperation operation) {
  if (IgnoreInputEvents()) {
    CancelDrag();
    return;
  }
  WebFloatPoint point_in_root_frame(
      GetPage()->GetVisualViewport().ViewportToRootFrame(point_in_viewport));

  WebMouseEvent fake_mouse_move(WebInputEvent::kMouseMove, point_in_root_frame,
                                WebFloatPoint(screen_point.x, screen_point.y),
                                WebPointerProperties::Button::kLeft, 0,
                                WebInputEvent::kNoModifiers,
                                TimeTicks::Now().InSeconds());
  fake_mouse_move.SetFrameScale(1);
  ToCoreFrame(LocalRoot())
      ->GetEventHandler()
      .DragSourceEndedAt(fake_mouse_move,
                         static_cast<DragOperation>(operation));
}

void WebFrameWidgetBase::DragSourceSystemDragEnded() {
  CancelDrag();
}

void WebFrameWidgetBase::CancelDrag() {
  // It's possible for us this to be callback while we're not doing a drag if
  // it's from a previous page that got unloaded.
  if (!doing_drag_and_drop_)
    return;
  GetPage()->GetDragController().DragEnded();
  doing_drag_and_drop_ = false;
}

void WebFrameWidgetBase::StartDragging(WebReferrerPolicy policy,
                                       const WebDragData& data,
                                       WebDragOperationsMask mask,
                                       const WebImage& drag_image,
                                       const WebPoint& drag_image_offset) {
  doing_drag_and_drop_ = true;
  Client()->StartDragging(policy, data, mask, drag_image, drag_image_offset);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnterOrOver(
    const WebPoint& point_in_viewport,
    const WebPoint& screen_point,
    DragAction drag_action,
    int modifiers) {
  DCHECK(current_drag_data_);
  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-over is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671504)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return kWebDragOperationNone;
  }

  WebPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  current_drag_data_->SetModifiers(modifiers);
  DragData drag_data(current_drag_data_.Get(), point_in_root_frame,
                     screen_point,
                     static_cast<DragOperation>(operations_allowed_));

  DragSession drag_session;
  drag_session = GetPage()->GetDragController().DragEnteredOrUpdated(
      &drag_data, *ToCoreFrame(LocalRoot()));

  DragOperation drop_effect = drag_session.operation;

  // Mask the drop effect operation against the drag source's allowed
  // operations.
  if (!(drop_effect & drag_data.DraggingSourceOperationMask()))
    drop_effect = kDragOperationNone;

  drag_operation_ = static_cast<WebDragOperation>(drop_effect);

  return drag_operation_;
}

WebPoint WebFrameWidgetBase::ViewportToRootFrame(
    const WebPoint& point_in_viewport) const {
  return GetPage()->GetVisualViewport().ViewportToRootFrame(point_in_viewport);
}

WebViewImpl* WebFrameWidgetBase::View() const {
  return ToWebLocalFrameImpl(LocalRoot())->ViewImpl();
}

Page* WebFrameWidgetBase::GetPage() const {
  return View()->GetPage();
}

void WebFrameWidgetBase::DidAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidAcquirePointerLock();

  LocalFrame* focusedFrame = FocusedLocalFrameInWidget();
  if (focusedFrame) {
    focusedFrame->GetEventHandler().ReleaseMousePointerCapture();
  }
}

void WebFrameWidgetBase::DidNotAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidNotAcquirePointerLock();
}

void WebFrameWidgetBase::DidLosePointerLock() {
  pointer_lock_gesture_token_ = nullptr;
  GetPage()->GetPointerLockController().DidLosePointerLock();
}

void WebFrameWidgetBase::RequestDecode(const PaintImage& image,
                                       WTF::Function<void(bool)> callback) {
  View()->RequestDecode(image, std::move(callback));
}

void WebFrameWidgetBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(current_drag_data_);
}

// TODO(665924): Remove direct dispatches of mouse events from
// PointerLockController, instead passing them through EventHandler.
void WebFrameWidgetBase::PointerLockMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  const WebMouseEvent& mouse_event =
      static_cast<const WebMouseEvent&>(input_event);
  WebMouseEvent transformed_event = TransformWebMouseEvent(
      ToWebLocalFrameImpl(LocalRoot())->GetFrameView(), mouse_event);

  LocalFrame* focusedFrame = FocusedLocalFrameInWidget();
  if (focusedFrame) {
    focusedFrame->GetEventHandler().ProcessPendingPointerCaptureForPointerLock(
        transformed_event);
  }

  std::unique_ptr<UserGestureIndicator> gesture_indicator;
  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::kMouseDown:
      event_type = EventTypeNames::mousedown;
      if (!GetPage() || !GetPage()->GetPointerLockController().GetElement())
        break;
      gesture_indicator =
          Frame::NotifyUserActivation(GetPage()
                                          ->GetPointerLockController()
                                          .GetElement()
                                          ->GetDocument()
                                          .GetFrame(),
                                      UserGestureToken::kNewGesture);
      pointer_lock_gesture_token_ = gesture_indicator->CurrentToken();
      break;
    case WebInputEvent::kMouseUp:
      event_type = EventTypeNames::mouseup;
      gesture_indicator = WTF::WrapUnique(
          new UserGestureIndicator(std::move(pointer_lock_gesture_token_)));
      break;
    case WebInputEvent::kMouseMove:
      event_type = EventTypeNames::mousemove;
      break;
    default:
      NOTREACHED();
  }

  if (GetPage()) {
    GetPage()->GetPointerLockController().DispatchLockedMouseEvent(
        transformed_event, event_type);
  }
}

void WebFrameWidgetBase::ShowContextMenu(WebMenuSourceType source_type) {
  if (!GetPage())
    return;

  GetPage()->GetContextMenuController().ClearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focused_frame =
            GetPage()->GetFocusController().FocusedFrame()) {
      focused_frame->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                                 source_type);
    }
  }
}

LocalFrame* WebFrameWidgetBase::FocusedLocalFrameInWidget() const {
  LocalFrame* frame = GetPage()->GetFocusController().FocusedFrame();
  return (frame && frame->LocalFrameRoot() == ToCoreFrame(LocalRoot()))
             ? frame
             : nullptr;
}

bool WebFrameWidgetBase::EndActiveFlingAnimation() {
  if (gesture_animation_) {
    gesture_animation_.reset();
    fling_source_device_ = kWebGestureDeviceUninitialized;
    if (WebLayerTreeView* layer_tree_view = GetLayerTreeView())
      layer_tree_view->DidStopFlinging();
    return true;
  }
  return false;
}

bool WebFrameWidgetBase::ScrollBy(const WebFloatSize& delta,
                                  const WebFloatSize& velocity) {
  DCHECK_NE(fling_source_device_, kWebGestureDeviceUninitialized);

  if (fling_source_device_ == kWebGestureDeviceTouchpad) {
    bool enable_touchpad_scroll_latching =
        RuntimeEnabledFeatures::TouchpadAndWheelScrollLatchingEnabled();
    WebMouseWheelEvent synthetic_wheel(WebInputEvent::kMouseWheel,
                                       fling_modifier_,
                                       WTF::MonotonicallyIncreasingTime());
    const float kTickDivisor = WheelEvent::kTickMultiplier;

    synthetic_wheel.delta_x = delta.width;
    synthetic_wheel.delta_y = delta.height;
    synthetic_wheel.wheel_ticks_x = delta.width / kTickDivisor;
    synthetic_wheel.wheel_ticks_y = delta.height / kTickDivisor;
    synthetic_wheel.has_precise_scrolling_deltas = true;
    synthetic_wheel.phase = WebMouseWheelEvent::kPhaseChanged;
    synthetic_wheel.SetPositionInWidget(position_on_fling_start_.x,
                                        position_on_fling_start_.y);
    synthetic_wheel.SetPositionInScreen(global_position_on_fling_start_.x,
                                        global_position_on_fling_start_.y);

    // TODO(wjmaclean): Is LocalRoot() the right frame to use here?
    if (GetPageWidgetEventHandler()->HandleMouseWheel(*ToCoreFrame(LocalRoot()),
                                                      synthetic_wheel) !=
        WebInputEventResult::kNotHandled) {
      return true;
    }

    if (!enable_touchpad_scroll_latching) {
      WebGestureEvent synthetic_scroll_begin =
          CreateGestureScrollEventFromFling(WebInputEvent::kGestureScrollBegin,
                                            kWebGestureDeviceTouchpad);
      synthetic_scroll_begin.data.scroll_begin.delta_x_hint = delta.width;
      synthetic_scroll_begin.data.scroll_begin.delta_y_hint = delta.height;
      synthetic_scroll_begin.data.scroll_begin.inertial_phase =
          WebGestureEvent::kMomentumPhase;
      GetPageWidgetEventHandler()->HandleGestureEvent(synthetic_scroll_begin);
    }

    WebGestureEvent synthetic_scroll_update = CreateGestureScrollEventFromFling(
        WebInputEvent::kGestureScrollUpdate, kWebGestureDeviceTouchpad);
    synthetic_scroll_update.data.scroll_update.delta_x = delta.width;
    synthetic_scroll_update.data.scroll_update.delta_y = delta.height;
    synthetic_scroll_update.data.scroll_update.velocity_x = velocity.width;
    synthetic_scroll_update.data.scroll_update.velocity_y = velocity.height;
    synthetic_scroll_update.data.scroll_update.inertial_phase =
        WebGestureEvent::kMomentumPhase;
    bool scroll_update_handled =
        GetPageWidgetEventHandler()->HandleGestureEvent(
            synthetic_scroll_update) != WebInputEventResult::kNotHandled;

    if (!enable_touchpad_scroll_latching) {
      WebGestureEvent synthetic_scroll_end = CreateGestureScrollEventFromFling(
          WebInputEvent::kGestureScrollEnd, kWebGestureDeviceTouchpad);
      synthetic_scroll_end.data.scroll_end.inertial_phase =
          WebGestureEvent::kMomentumPhase;
      GetPageWidgetEventHandler()->HandleGestureEvent(synthetic_scroll_end);
    }

    return scroll_update_handled;
  }

  WebGestureEvent synthetic_gesture_event = CreateGestureScrollEventFromFling(
      WebInputEvent::kGestureScrollUpdate, fling_source_device_);
  synthetic_gesture_event.data.scroll_update.delta_x = delta.width;
  synthetic_gesture_event.data.scroll_update.delta_y = delta.height;
  synthetic_gesture_event.data.scroll_update.velocity_x = velocity.width;
  synthetic_gesture_event.data.scroll_update.velocity_y = velocity.height;
  synthetic_gesture_event.data.scroll_update.inertial_phase =
      WebGestureEvent::kMomentumPhase;

  return GetPageWidgetEventHandler()->HandleGestureEvent(
             synthetic_gesture_event) != WebInputEventResult::kNotHandled;
}

WebInputEventResult WebFrameWidgetBase::HandleGestureFlingEvent(
    const WebGestureEvent& event) {
  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  switch (event.GetType()) {
    case WebInputEvent::kGestureFlingStart: {
      if (event.source_device != kWebGestureDeviceSyntheticAutoscroll)
        EndActiveFlingAnimation();
      position_on_fling_start_ = WebPoint(event.x, event.y);
      global_position_on_fling_start_ =
          WebPoint(event.global_x, event.global_y);
      fling_modifier_ = event.GetModifiers();
      fling_source_device_ = event.source_device;
      DCHECK_NE(fling_source_device_, kWebGestureDeviceUninitialized);
      std::unique_ptr<WebGestureCurve> fling_curve =
          Platform::Current()->CreateFlingAnimationCurve(
              event.source_device,
              WebFloatPoint(event.data.fling_start.velocity_x,
                            event.data.fling_start.velocity_y),
              WebSize());
      DCHECK(fling_curve);
      gesture_animation_ = WebActiveGestureAnimation::CreateWithTimeOffset(
          std::move(fling_curve), this, event.TimeStampSeconds());
      ScheduleAnimation();

      WebGestureEvent scaled_event =
          TransformWebGestureEvent(ToCoreFrame(LocalRoot())->View(), event);
      // Plugins may need to see GestureFlingStart to balance
      // GestureScrollBegin (since the former replaces GestureScrollEnd when
      // transitioning to a fling).
      // TODO(dtapuska): Why isn't the response used?
      ToCoreFrame(LocalRoot())
          ->GetEventHandler()
          .HandleGestureScrollEvent(scaled_event);

      event_result = WebInputEventResult::kHandledSystem;
      break;
    }
    case WebInputEvent::kGestureFlingCancel:
      if (EndActiveFlingAnimation())
        event_result = WebInputEventResult::kHandledSuppressed;

      break;
    default:
      NOTREACHED();
  }
  return event_result;
}

void WebFrameWidgetBase::TransferActiveWheelFlingAnimation(
    const WebActiveWheelFlingParameters& parameters) {
  TRACE_EVENT0("blink",
               "WebFrameWidgetBase::TransferActiveWheelFlingAnimation");
  DCHECK(!gesture_animation_);
  position_on_fling_start_ = parameters.point;
  global_position_on_fling_start_ = parameters.global_point;
  fling_modifier_ = parameters.modifiers;
  std::unique_ptr<WebGestureCurve> curve =
      Platform::Current()->CreateFlingAnimationCurve(
          parameters.source_device, WebFloatPoint(parameters.delta),
          parameters.cumulative_scroll);
  DCHECK(curve);
  gesture_animation_ = WebActiveGestureAnimation::CreateWithTimeOffset(
      std::move(curve), this, parameters.start_time);
  DCHECK_NE(parameters.source_device, kWebGestureDeviceUninitialized);
  fling_source_device_ = parameters.source_device;
  ScheduleAnimation();
}

WebGestureEvent WebFrameWidgetBase::CreateGestureScrollEventFromFling(
    WebInputEvent::Type type,
    WebGestureDevice source_device) const {
  WebGestureEvent gesture_event(type, fling_modifier_,
                                WTF::MonotonicallyIncreasingTime());
  gesture_event.source_device = source_device;
  gesture_event.x = position_on_fling_start_.x;
  gesture_event.y = position_on_fling_start_.y;
  gesture_event.global_x = global_position_on_fling_start_.x;
  gesture_event.global_y = global_position_on_fling_start_.y;
  return gesture_event;
}

bool WebFrameWidgetBase::IsFlinging() const {
  return !!gesture_animation_;
}

void WebFrameWidgetBase::UpdateGestureAnimation(
    double last_frame_time_monotonic) {
  if (!gesture_animation_)
    return;

  if (gesture_animation_->Animate(last_frame_time_monotonic)) {
    ScheduleAnimation();
  } else {
    DCHECK_NE(fling_source_device_, kWebGestureDeviceUninitialized);
    WebGestureDevice last_fling_source_device = fling_source_device_;
    EndActiveFlingAnimation();

    if (last_fling_source_device != kWebGestureDeviceSyntheticAutoscroll) {
      WebGestureEvent end_scroll_event = CreateGestureScrollEventFromFling(
          WebInputEvent::kGestureScrollEnd, last_fling_source_device);
      ToCoreFrame(LocalRoot())
          ->GetEventHandler()
          .HandleGestureScrollEnd(end_scroll_event);
    }
  }
}

}  // namespace blink
