// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/input_handler_proxy.h"

#include <stddef.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "third_party/WebKit/public/platform/WebTouchEvent.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/compositor_thread_event_queue.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/event_with_callback.h"
#include "ui/events/blink/input_handler_proxy_client.h"
#include "ui/events/blink/input_scroll_elasticity_controller.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/latency/latency_info.h"

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace {

const int32_t kEventDispositionUndefined = -1;

// Maximum time between a fling event's timestamp and the first |Animate| call
// for the fling curve to use the fling timestamp as the initial animation time.
// Two frames allows a minor delay between event creation and the first animate.
const double kMaxSecondsFromFlingTimestampToFirstAnimate = 2. / 60.;

// Threshold for determining whether a fling scroll delta should have caused the
// client to scroll.
const float kScrollEpsilon = 0.1f;

// Minimum fling velocity required for the active fling and new fling for the
// two to accumulate.
const double kMinBoostFlingSpeedSquare = 350. * 350.;

// Minimum velocity for the active touch scroll to preserve (boost) an active
// fling for which cancellation has been deferred.
const double kMinBoostTouchScrollSpeedSquare = 150 * 150.;

// Timeout window after which the active fling will be cancelled if no animation
// ticks, scrolls or flings of sufficient velocity relative to the current fling
// are received. The default value on Android native views is 40ms, but we use a
// slightly increased value to accomodate small IPC message delays.
const double kFlingBoostTimeoutDelaySeconds = 0.05;

const size_t kTenSeconds = 10 * 1000 * 1000;

gfx::Vector2dF ToClientScrollIncrement(const WebFloatSize& increment) {
  return gfx::Vector2dF(-increment.width, -increment.height);
}

double InSecondsF(const base::TimeTicks& time) {
  return (time - base::TimeTicks()).InSecondsF();
}

bool ShouldSuppressScrollForFlingBoosting(
    const gfx::Vector2dF& current_fling_velocity,
    const WebGestureEvent& scroll_update_event,
    double time_since_last_boost_event,
    double time_since_last_fling_animate) {
  DCHECK_EQ(WebInputEvent::kGestureScrollUpdate, scroll_update_event.GetType());

  gfx::Vector2dF dx(scroll_update_event.data.scroll_update.delta_x,
                    scroll_update_event.data.scroll_update.delta_y);
  if (gfx::DotProduct(current_fling_velocity, dx) <= 0)
    return false;

  if (time_since_last_fling_animate > kFlingBoostTimeoutDelaySeconds)
    return false;

  if (time_since_last_boost_event < 0.001)
    return true;

  // TODO(jdduke): Use |scroll_update_event.data.scrollUpdate.velocity{X,Y}|.
  // The scroll must be of sufficient velocity to maintain the active fling.
  const gfx::Vector2dF scroll_velocity =
      gfx::ScaleVector2d(dx, 1. / time_since_last_boost_event);
  if (scroll_velocity.LengthSquared() < kMinBoostTouchScrollSpeedSquare)
    return false;

  return true;
}

bool ShouldBoostFling(const gfx::Vector2dF& current_fling_velocity,
                      const WebGestureEvent& fling_start_event) {
  DCHECK_EQ(WebInputEvent::kGestureFlingStart, fling_start_event.GetType());

  gfx::Vector2dF new_fling_velocity(
      fling_start_event.data.fling_start.velocity_x,
      fling_start_event.data.fling_start.velocity_y);

  if (gfx::DotProduct(current_fling_velocity, new_fling_velocity) <= 0)
    return false;

  if (current_fling_velocity.LengthSquared() < kMinBoostFlingSpeedSquare)
    return false;

  if (new_fling_velocity.LengthSquared() < kMinBoostFlingSpeedSquare)
    return false;

  return true;
}

WebGestureEvent ObtainGestureScrollBegin(const WebGestureEvent& event) {
  WebGestureEvent scroll_begin_event = event;
  scroll_begin_event.SetType(WebInputEvent::kGestureScrollBegin);
  scroll_begin_event.data.scroll_begin.delta_x_hint = 0;
  scroll_begin_event.data.scroll_begin.delta_y_hint = 0;
  return scroll_begin_event;
}

cc::ScrollState CreateScrollStateForGesture(const WebGestureEvent& event) {
  cc::ScrollStateData scroll_state_data;
  switch (event.GetType()) {
    case WebInputEvent::kGestureScrollBegin:
      scroll_state_data.position_x = event.x;
      scroll_state_data.position_y = event.y;
      scroll_state_data.delta_x_hint = -event.data.scroll_begin.delta_x_hint;
      scroll_state_data.delta_y_hint = -event.data.scroll_begin.delta_y_hint;
      scroll_state_data.is_beginning = true;
      // On Mac, a GestureScrollBegin in the inertial phase indicates a fling
      // start.
      scroll_state_data.is_in_inertial_phase =
          (event.data.scroll_begin.inertial_phase ==
           WebGestureEvent::kMomentumPhase);
      break;
    case WebInputEvent::kGestureFlingStart:
      scroll_state_data.velocity_x = event.data.fling_start.velocity_x;
      scroll_state_data.velocity_y = event.data.fling_start.velocity_y;
      scroll_state_data.is_in_inertial_phase = true;
      break;
    case WebInputEvent::kGestureScrollUpdate:
      scroll_state_data.delta_x = -event.data.scroll_update.delta_x;
      scroll_state_data.delta_y = -event.data.scroll_update.delta_y;
      scroll_state_data.velocity_x = event.data.scroll_update.velocity_x;
      scroll_state_data.velocity_y = event.data.scroll_update.velocity_y;
      scroll_state_data.is_in_inertial_phase =
          event.data.scroll_update.inertial_phase ==
          WebGestureEvent::kMomentumPhase;
      break;
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureFlingCancel:
      scroll_state_data.is_ending = true;
      break;
    default:
      NOTREACHED();
      break;
  }
  return cc::ScrollState(scroll_state_data);
}

cc::InputHandler::ScrollInputType GestureScrollInputType(
    blink::WebGestureDevice device) {
  return device == blink::kWebGestureDeviceTouchpad
             ? cc::InputHandler::WHEEL
             : cc::InputHandler::TOUCHSCREEN;
}

enum ScrollingThreadStatus {
  SCROLLING_ON_COMPOSITOR,
  SCROLLING_ON_COMPOSITOR_BLOCKED_ON_MAIN,
  SCROLLING_ON_MAIN,
  LAST_SCROLLING_THREAD_STATUS_VALUE = SCROLLING_ON_MAIN,
};

}  // namespace

namespace ui {

InputHandlerProxy::InputHandlerProxy(
    cc::InputHandler* input_handler,
    InputHandlerProxyClient* client,
    bool touchpad_and_wheel_scroll_latching_enabled)
    : client_(client),
      input_handler_(input_handler),
      deferred_fling_cancel_time_seconds_(0),
      synchronous_input_handler_(nullptr),
      allow_root_animate_(true),
#ifndef NDEBUG
      expect_scroll_update_end_(false),
#endif
      gesture_scroll_on_impl_thread_(false),
      gesture_pinch_on_impl_thread_(false),
      scroll_sequence_ignored_(false),
      fling_may_be_active_on_main_thread_(false),
      disallow_horizontal_fling_scroll_(false),
      disallow_vertical_fling_scroll_(false),
      has_fling_animation_started_(false),
      smooth_scroll_enabled_(false),
      touchpad_and_wheel_scroll_latching_enabled_(
          touchpad_and_wheel_scroll_latching_enabled),
      touch_result_(kEventDispositionUndefined),
      mouse_wheel_result_(kEventDispositionUndefined),
      current_overscroll_params_(nullptr),
      has_ongoing_compositor_scroll_fling_pinch_(false),
      tick_clock_(base::MakeUnique<base::DefaultTickClock>()) {
  DCHECK(client);
  input_handler_->BindToClient(this,
                               touchpad_and_wheel_scroll_latching_enabled_);
  cc::ScrollElasticityHelper* scroll_elasticity_helper =
      input_handler_->CreateScrollElasticityHelper();
  if (scroll_elasticity_helper) {
    scroll_elasticity_controller_.reset(
        new InputScrollElasticityController(scroll_elasticity_helper));
  }
  compositor_event_queue_ =
      base::FeatureList::IsEnabled(features::kVsyncAlignedInputEvents)
          ? base::MakeUnique<CompositorThreadEventQueue>()
          : nullptr;
}

InputHandlerProxy::~InputHandlerProxy() {}

void InputHandlerProxy::WillShutdown() {
  scroll_elasticity_controller_.reset();
  input_handler_ = NULL;
  client_->WillShutdown();
}

void InputHandlerProxy::HandleInputEventWithLatencyInfo(
    WebScopedInputEvent event,
    const LatencyInfo& latency_info,
    EventDispositionCallback callback) {
  DCHECK(input_handler_);

  TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventImpl");

  std::unique_ptr<EventWithCallback> event_with_callback =
      base::MakeUnique<EventWithCallback>(std::move(event), latency_info,
                                          tick_clock_->NowTicks(),
                                          std::move(callback));

  // Note: Other input can race ahead of gesture input as they don't have to go
  // through the queue, but we believe it's OK to do so.
  if (!compositor_event_queue_ ||
      !IsGestureScrollOrFlingOrPinch(event_with_callback->event().GetType())) {
    DispatchSingleInputEvent(std::move(event_with_callback),
                             tick_clock_->NowTicks());
    return;
  }

  if (has_ongoing_compositor_scroll_fling_pinch_) {
    const auto& gesture_event = ToWebGestureEvent(event_with_callback->event());
    bool is_from_set_non_blocking_touch =
        gesture_event.source_device == blink::kWebGestureDeviceTouchscreen &&
        gesture_event.is_source_touch_event_set_non_blocking;
    bool is_scroll_end_from_wheel =
        gesture_event.source_device == blink::kWebGestureDeviceTouchpad &&
        gesture_event.GetType() == blink::WebGestureEvent::kGestureScrollEnd;
    if (is_from_set_non_blocking_touch || is_scroll_end_from_wheel) {
      // Gesture events was already delayed by blocking events in rAF aligned
      // queue. We want to avoid additional one frame delay by flushing the
      // VSync queue immediately.
      // The first GSU latency was tracked by:
      // |smoothness.tough_scrolling_cases:first_gesture_scroll_update_latency|.
      compositor_event_queue_->Queue(std::move(event_with_callback),
                                     tick_clock_->NowTicks());
      DispatchQueuedInputEvents();
      return;
    }

    bool needs_animate_input = compositor_event_queue_->empty();
    compositor_event_queue_->Queue(std::move(event_with_callback),
                                   tick_clock_->NowTicks());
    if (needs_animate_input)
      input_handler_->SetNeedsAnimateInput();
    return;
  }

  // We have to dispatch the event to know whether the gesture sequence will be
  // handled by the compositor or not.
  DispatchSingleInputEvent(std::move(event_with_callback),
                           tick_clock_->NowTicks());
}

void InputHandlerProxy::DispatchSingleInputEvent(
    std::unique_ptr<EventWithCallback> event_with_callback,
    const base::TimeTicks now) {
  if (compositor_event_queue_ &&
      IsGestureScrollOrFlingOrPinch(event_with_callback->event().GetType())) {
    // Report the coalesced count only for continuous events to avoid the noise
    // from non-continuous events.
    if (IsContinuousGestureEvent(event_with_callback->event().GetType())) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.CompositorThreadEventQueue.Continuous.HeadQueueingTime",
          (now - event_with_callback->creation_timestamp()).InMicroseconds(), 1,
          kTenSeconds, 50);

      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.CompositorThreadEventQueue.Continuous.TailQueueingTime",
          (now - event_with_callback->last_coalesced_timestamp())
              .InMicroseconds(),
          1, kTenSeconds, 50);

      UMA_HISTOGRAM_COUNTS_1000(
          "Event.CompositorThreadEventQueue.CoalescedCount",
          static_cast<int>(event_with_callback->coalesced_count()));
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.CompositorThreadEventQueue.NonContinuous.QueueingTime",
          (now - event_with_callback->creation_timestamp()).InMicroseconds(), 1,
          kTenSeconds, 50);
    }
  }

  ui::LatencyInfo monitored_latency_info = event_with_callback->latency_info();
  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor =
      input_handler_->CreateLatencyInfoSwapPromiseMonitor(
          &monitored_latency_info);

  current_overscroll_params_.reset();
  InputHandlerProxy::EventDisposition disposition =
      HandleInputEvent(event_with_callback->event());

  switch (event_with_callback->event().GetType()) {
    case blink::WebGestureEvent::kGestureScrollBegin:
    case blink::WebGestureEvent::kGestureFlingStart:
    case blink::WebGestureEvent::kGesturePinchBegin:
    case blink::WebGestureEvent::kGestureScrollUpdate:
    case blink::WebGestureEvent::kGesturePinchUpdate:
      has_ongoing_compositor_scroll_fling_pinch_ = disposition == DID_HANDLE;
      break;

    case blink::WebGestureEvent::kGestureScrollEnd:
    case blink::WebGestureEvent::kGestureFlingCancel:
    case blink::WebGestureEvent::kGesturePinchEnd:
      has_ongoing_compositor_scroll_fling_pinch_ = false;
      break;
    default:
      break;
  }

  // Will run callback for every original events.
  event_with_callback->RunCallbacks(disposition, monitored_latency_info,
                                    std::move(current_overscroll_params_));
}

void InputHandlerProxy::DispatchQueuedInputEvents() {
  if (!compositor_event_queue_)
    return;

  // Calling |NowTicks()| is expensive so we only want to do it once.
  base::TimeTicks now = tick_clock_->NowTicks();
  while (!compositor_event_queue_->empty())
    DispatchSingleInputEvent(compositor_event_queue_->Pop(), now);
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleInputEvent(
    const WebInputEvent& event) {
  DCHECK(input_handler_);

  if (FilterInputEventForFlingBoosting(event))
    return DID_HANDLE;

  switch (event.GetType()) {
    case WebInputEvent::kMouseWheel:
      return HandleMouseWheel(static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::kGestureScrollBegin:
      return HandleGestureScrollBegin(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kGestureScrollUpdate:
      return HandleGestureScrollUpdate(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kGestureScrollEnd:
      return HandleGestureScrollEnd(static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kGesturePinchBegin: {
      DCHECK(!gesture_pinch_on_impl_thread_);
      const WebGestureEvent& gesture_event =
          static_cast<const WebGestureEvent&>(event);
      if (gesture_event.source_device == blink::kWebGestureDeviceTouchpad &&
          input_handler_->GetEventListenerProperties(
              cc::EventListenerClass::kMouseWheel) !=
              cc::EventListenerProperties::kNone) {
        return DID_NOT_HANDLE;
      } else {
        input_handler_->PinchGestureBegin();
        gesture_pinch_on_impl_thread_ = true;
        return DID_HANDLE;
      }
    }

    case WebInputEvent::kGesturePinchEnd:
      if (gesture_pinch_on_impl_thread_) {
        gesture_pinch_on_impl_thread_ = false;
        input_handler_->PinchGestureEnd();
        return DID_HANDLE;
      } else {
        return DID_NOT_HANDLE;
      }

    case WebInputEvent::kGesturePinchUpdate: {
      if (gesture_pinch_on_impl_thread_) {
        const WebGestureEvent& gesture_event =
            static_cast<const WebGestureEvent&>(event);
        if (gesture_event.data.pinch_update.zoom_disabled)
          return DROP_EVENT;
        input_handler_->PinchGestureUpdate(
            gesture_event.data.pinch_update.scale,
            gfx::Point(gesture_event.x, gesture_event.y));
        return DID_HANDLE;
      } else {
        return DID_NOT_HANDLE;
      }
    }

    case WebInputEvent::kGestureFlingStart:
      return HandleGestureFlingStart(
          *static_cast<const WebGestureEvent*>(&event));

    case WebInputEvent::kGestureFlingCancel:
      if (CancelCurrentFling())
        return DID_HANDLE;
      else if (!fling_may_be_active_on_main_thread_)
        return DROP_EVENT;
      return DID_NOT_HANDLE;

    case WebInputEvent::kTouchStart:
      return HandleTouchStart(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::kTouchMove:
      return HandleTouchMove(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::kTouchEnd:
      return HandleTouchEnd(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::kMouseDown: {
      // Only for check scrollbar captured
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);

      if (mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
        CHECK(input_handler_);
        input_handler_->MouseDown();
      }
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::kMouseUp: {
      // Only for release scrollbar captured
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);

      if (mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
        CHECK(input_handler_);
        input_handler_->MouseUp();
      }
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::kMouseMove: {
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);
      // TODO(davemoore): This should never happen, but bug #326635 showed some
      // surprising crashes.
      CHECK(input_handler_);
      input_handler_->MouseMoveAt(gfx::Point(mouse_event.PositionInWidget().x,
                                             mouse_event.PositionInWidget().y));
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::kMouseLeave: {
      CHECK(input_handler_);
      input_handler_->MouseLeave();
      return DID_NOT_HANDLE;
    }

    default:
      if (WebInputEvent::IsKeyboardEventType(event.GetType())) {
        // Only call |CancelCurrentFling()| if a fling was active, as it will
        // otherwise disrupt an in-progress touch scroll.
        if (fling_curve_)
          CancelCurrentFling();
      }
      break;
  }

  return DID_NOT_HANDLE;
}

void InputHandlerProxy::RecordMainThreadScrollingReasons(
    blink::WebGestureDevice device,
    uint32_t reasons) {
  static const char* kGestureHistogramName =
      "Renderer4.MainThreadGestureScrollReason";
  static const char* kWheelHistogramName =
      "Renderer4.MainThreadWheelScrollReason";

  if (device != blink::kWebGestureDeviceTouchpad &&
      device != blink::kWebGestureDeviceTouchscreen) {
    return;
  }

  // NonCompositedScrollReasons should only be set on the main thread.
  DCHECK(
      !cc::MainThreadScrollingReason::HasNonCompositedScrollReasons(reasons));

  // UMA_HISTOGRAM_ENUMERATION requires that the enum_max must be strictly
  // greater than the sample value. kMainThreadScrollingReasonCount doesn't
  // include the NotScrollingOnMain enum but the histograms do so adding
  // the +1 is necessary.
  // TODO(dcheng): Fix https://crbug.com/705169 so this isn't needed.
  constexpr uint32_t kMainThreadScrollingReasonEnumMax =
      cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount + 1;
  if (reasons == cc::MainThreadScrollingReason::kNotScrollingOnMain) {
    if (device == blink::kWebGestureDeviceTouchscreen) {
      UMA_HISTOGRAM_ENUMERATION(
          kGestureHistogramName,
          cc::MainThreadScrollingReason::kNotScrollingOnMain,
          kMainThreadScrollingReasonEnumMax);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          kWheelHistogramName,
          cc::MainThreadScrollingReason::kNotScrollingOnMain,
          kMainThreadScrollingReasonEnumMax);
    }
  }

  for (uint32_t i = 0;
       i < cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount;
       ++i) {
    unsigned val = 1 << i;
    if (reasons & val) {
      if (val == cc::MainThreadScrollingReason::kHandlingScrollFromMainThread) {
        // We only want to record "Handling scroll from main thread" reason if
        // it's the only reason. If it's not the only reason, the "real" reason
        // for scrolling on main is something else, and we only want to pay
        // attention to that reason.
        if (reasons & ~val)
          continue;
      }
      if (device == blink::kWebGestureDeviceTouchscreen) {
        UMA_HISTOGRAM_ENUMERATION(kGestureHistogramName, i + 1,
                                  kMainThreadScrollingReasonEnumMax);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kWheelHistogramName, i + 1,
                                  kMainThreadScrollingReasonEnumMax);
      }
    }
  }
}

void InputHandlerProxy::RecordScrollingThreadStatus(
    blink::WebGestureDevice device,
    uint32_t reasons) {
  if (device != blink::kWebGestureDeviceTouchpad &&
      device != blink::kWebGestureDeviceTouchscreen) {
    return;
  }

  ScrollingThreadStatus scrolling_thread_status = SCROLLING_ON_MAIN;
  if (reasons == cc::MainThreadScrollingReason::kNotScrollingOnMain) {
    int32_t event_disposition_result =
        (device == blink::kWebGestureDeviceTouchpad ? mouse_wheel_result_
                                                    : touch_result_);
    switch (event_disposition_result) {
      case kEventDispositionUndefined:
      case DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING:
      case DID_HANDLE_NON_BLOCKING:
      case DROP_EVENT:
        scrolling_thread_status = SCROLLING_ON_COMPOSITOR;
        break;
      case DID_NOT_HANDLE:
        scrolling_thread_status = SCROLLING_ON_COMPOSITOR_BLOCKED_ON_MAIN;
        break;
      default:
        NOTREACHED();
        scrolling_thread_status = SCROLLING_ON_COMPOSITOR;
    }
  }

  // UMA_HISTOGRAM_ENUMERATION requires that the enum_max must be strictly
  // greater than the sample value.
  const uint32_t kScrolingThreadStatusEnumMax =
      ScrollingThreadStatus::LAST_SCROLLING_THREAD_STATUS_VALUE + 1;

  if (device == blink::kWebGestureDeviceTouchscreen) {
    UMA_HISTOGRAM_ENUMERATION("Renderer4.GestureScrollingThreadStatus",
                              scrolling_thread_status,
                              kScrolingThreadStatusEnumMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Renderer4.WheelScrollingThreadStatus",
                              scrolling_thread_status,
                              kScrolingThreadStatusEnumMax);
  }
}

bool InputHandlerProxy::ShouldAnimate(bool has_precise_scroll_deltas) const {
#if defined(OS_MACOSX)
  // Mac does not smooth scroll wheel events (crbug.com/574283).
  return false;
#else
  return smooth_scroll_enabled_ && !has_precise_scroll_deltas;
#endif
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleMouseWheel(
    const WebMouseWheelEvent& wheel_event) {
  // Only call |CancelCurrentFling()| if a fling was active, as it will
  // otherwise disrupt an in-progress touch scroll.
  if (!wheel_event.has_precise_scrolling_deltas && fling_curve_)
    CancelCurrentFling();

  InputHandlerProxy::EventDisposition result = DROP_EVENT;

  if (wheel_event.dispatch_type == WebInputEvent::kEventNonBlocking) {
    // The first wheel event in the sequence should be cancellable.
    DCHECK(wheel_event.phase != WebMouseWheelEvent::kPhaseBegan);

    DCHECK(mouse_wheel_result_ != kEventDispositionUndefined);
    result = static_cast<EventDisposition>(mouse_wheel_result_);

    if (wheel_event.phase == WebMouseWheelEvent::kPhaseEnded ||
        wheel_event.phase == WebMouseWheelEvent::kPhaseCancelled ||
        wheel_event.momentum_phase == WebMouseWheelEvent::kPhaseEnded ||
        wheel_event.momentum_phase == WebMouseWheelEvent::kPhaseCancelled) {
      mouse_wheel_result_ = kEventDispositionUndefined;
    }
    if (mouse_wheel_result_ != kEventDispositionUndefined)
      return result;
  }

  cc::EventListenerProperties properties =
      input_handler_->GetEventListenerProperties(
          cc::EventListenerClass::kMouseWheel);
  switch (properties) {
    case cc::EventListenerProperties::kPassive:
      result = DID_HANDLE_NON_BLOCKING;
      break;
    case cc::EventListenerProperties::kBlockingAndPassive:
    case cc::EventListenerProperties::kBlocking:
      result = DID_NOT_HANDLE;
      break;
    case cc::EventListenerProperties::kNone:
      result = DROP_EVENT;
      break;
    default:
      NOTREACHED();
      result = DROP_EVENT;
  }

  mouse_wheel_result_ = result;
  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::FlingScrollByMouseWheel(
    const WebMouseWheelEvent& wheel_event,
    cc::EventListenerProperties listener_properties) {
  DCHECK(listener_properties == cc::EventListenerProperties::kPassive ||
         listener_properties == cc::EventListenerProperties::kNone);

  DCHECK(!wheel_event.rails_mode);
  gfx::Vector2dF scroll_delta(-wheel_event.delta_x, -wheel_event.delta_y);

  DCHECK(!wheel_event.scroll_by_page);
  DCHECK(wheel_event.has_precise_scrolling_deltas);
  if (touchpad_and_wheel_scroll_latching_enabled_) {
    if (gesture_scroll_on_impl_thread_) {
      TRACE_EVENT_INSTANT2("input",
                           "InputHandlerProxy::handle_input wheel scroll",
                           TRACE_EVENT_SCOPE_THREAD, "deltaX", scroll_delta.x(),
                           "deltaY", scroll_delta.y());

      cc::ScrollStateData scroll_state_update_data;
      scroll_state_update_data.delta_x = scroll_delta.x();
      scroll_state_update_data.delta_y = scroll_delta.y();
      scroll_state_update_data.position_x = wheel_event.PositionInWidget().x;
      scroll_state_update_data.position_y = wheel_event.PositionInWidget().y;
      cc::ScrollState scroll_state_update(scroll_state_update_data);

      cc::InputHandlerScrollResult scroll_result =
          input_handler_->ScrollBy(&scroll_state_update);

      if (!scroll_result.did_scroll &&
          input_handler_->ScrollingShouldSwitchtoMainThread()) {
        gesture_scroll_on_impl_thread_ = false;
        return DID_NOT_HANDLE;
      }

      HandleOverscroll(gfx::Point(wheel_event.PositionInWidget().x,
                                  wheel_event.PositionInWidget().y),
                       scroll_result, false);
      if (scroll_result.did_scroll) {
        return listener_properties == cc::EventListenerProperties::kPassive
                   ? DID_HANDLE_NON_BLOCKING
                   : DID_HANDLE;
      }

      return DROP_EVENT;
    } else {
      return DID_NOT_HANDLE;
    }
  } else {  // !touchpad_and_wheel_scroll_latching_enabled_
    cc::ScrollStateData scroll_state_begin_data;
    scroll_state_begin_data.position_x = wheel_event.PositionInWidget().x;
    scroll_state_begin_data.position_y = wheel_event.PositionInWidget().y;
    scroll_state_begin_data.is_beginning = true;
    cc::ScrollState scroll_state_begin(scroll_state_begin_data);
    cc::InputHandler::ScrollStatus scroll_status = input_handler_->ScrollBegin(
        &scroll_state_begin, cc::InputHandler::WHEEL);

    RecordMainThreadScrollingReasons(
        blink::kWebGestureDeviceTouchpad,
        scroll_status.main_thread_scrolling_reasons);

    mouse_wheel_result_ =
        (listener_properties == cc::EventListenerProperties::kPassive)
            ? DID_HANDLE_NON_BLOCKING
            : DROP_EVENT;
    RecordScrollingThreadStatus(blink::kWebGestureDeviceTouchpad,
                                scroll_status.main_thread_scrolling_reasons);

    switch (scroll_status.thread) {
      case cc::InputHandler::SCROLL_ON_IMPL_THREAD: {
        TRACE_EVENT_INSTANT2("input",
                             "InputHandlerProxy::handle_input wheel scroll",
                             TRACE_EVENT_SCOPE_THREAD, "deltaX",
                             scroll_delta.x(), "deltaY", scroll_delta.y());

        cc::ScrollStateData scroll_state_update_data;
        scroll_state_update_data.delta_x = scroll_delta.x();
        scroll_state_update_data.delta_y = scroll_delta.y();
        scroll_state_update_data.position_x = wheel_event.PositionInWidget().x;
        scroll_state_update_data.position_y = wheel_event.PositionInWidget().y;
        cc::ScrollState scroll_state_update(scroll_state_update_data);

        cc::InputHandlerScrollResult scroll_result =
            input_handler_->ScrollBy(&scroll_state_update);
        HandleOverscroll(gfx::Point(wheel_event.PositionInWidget().x,
                                    wheel_event.PositionInWidget().y),
                         scroll_result, false);

        cc::ScrollStateData scroll_state_end_data;
        scroll_state_end_data.is_ending = true;
        cc::ScrollState scroll_state_end(scroll_state_end_data);
        input_handler_->ScrollEnd(&scroll_state_end);

        if (scroll_result.did_scroll) {
          return listener_properties == cc::EventListenerProperties::kPassive
                     ? DID_HANDLE_NON_BLOCKING
                     : DID_HANDLE;
        }
        return DROP_EVENT;
      }
      case cc::InputHandler::SCROLL_IGNORED:
        // TODO(jamesr): This should be DROP_EVENT, but in cases where we fail
        // to properly sync scrollability it's safer to send the event to the
        // main thread. Change back to DROP_EVENT once we have synchronization
        // bugs sorted out.
        return DID_NOT_HANDLE;
      case cc::InputHandler::SCROLL_UNKNOWN:
      case cc::InputHandler::SCROLL_ON_MAIN_THREAD:
        return DID_NOT_HANDLE;
      default:
        NOTREACHED();
        return DID_NOT_HANDLE;
    }
  }
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureScrollBegin(
    const WebGestureEvent& gesture_event) {
  if (gesture_scroll_on_impl_thread_)
    CancelCurrentFling();

#ifndef NDEBUG
  DCHECK(!expect_scroll_update_end_);
  expect_scroll_update_end_ = true;
#endif
  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  cc::InputHandler::ScrollStatus scroll_status;
  if (gesture_event.data.scroll_begin.delta_hint_units ==
      blink::WebGestureEvent::ScrollUnits::kPage) {
    scroll_status.thread = cc::InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        cc::MainThreadScrollingReason::kContinuingMainThreadScroll;
  } else if (gesture_event.data.scroll_begin.target_viewport) {
    scroll_status = input_handler_->RootScrollBegin(
        &scroll_state, GestureScrollInputType(gesture_event.source_device));
  } else if (ShouldAnimate(gesture_event.data.scroll_begin.delta_hint_units !=
                           blink::WebGestureEvent::ScrollUnits::kPixels)) {
    DCHECK(!scroll_state.is_in_inertial_phase());
    scroll_status = input_handler_->ScrollAnimatedBegin(&scroll_state);
  } else {
    scroll_status = input_handler_->ScrollBegin(
        &scroll_state, GestureScrollInputType(gesture_event.source_device));
  }
  UMA_HISTOGRAM_ENUMERATION("Renderer4.CompositorScrollHitTestResult",
                            scroll_status.thread,
                            cc::InputHandler::LAST_SCROLL_STATUS + 1);

  RecordMainThreadScrollingReasons(gesture_event.source_device,
                                   scroll_status.main_thread_scrolling_reasons);

  RecordScrollingThreadStatus(gesture_event.source_device,
                              scroll_status.main_thread_scrolling_reasons);

  InputHandlerProxy::EventDisposition result = DID_NOT_HANDLE;
  switch (scroll_status.thread) {
    case cc::InputHandler::SCROLL_ON_IMPL_THREAD:
      TRACE_EVENT_INSTANT0("input",
                           "InputHandlerProxy::handle_input gesture scroll",
                           TRACE_EVENT_SCOPE_THREAD);
      gesture_scroll_on_impl_thread_ = true;
      result = DID_HANDLE;
      break;
    case cc::InputHandler::SCROLL_UNKNOWN:
    case cc::InputHandler::SCROLL_ON_MAIN_THREAD:
      result = DID_NOT_HANDLE;
      break;
    case cc::InputHandler::SCROLL_IGNORED:
      scroll_sequence_ignored_ = true;
      result = DROP_EVENT;
      break;
  }
  if (scroll_elasticity_controller_ && result != DID_NOT_HANDLE)
    HandleScrollElasticityOverscroll(gesture_event,
                                     cc::InputHandlerScrollResult());

  return result;
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleGestureScrollUpdate(
    const WebGestureEvent& gesture_event) {
#ifndef NDEBUG
  DCHECK(expect_scroll_update_end_);
#endif

  if (scroll_sequence_ignored_)
    return DROP_EVENT;

  if (!gesture_scroll_on_impl_thread_ && !gesture_pinch_on_impl_thread_)
    return DID_NOT_HANDLE;

  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  gfx::Point scroll_point(gesture_event.x, gesture_event.y);
  gfx::Vector2dF scroll_delta(-gesture_event.data.scroll_update.delta_x,
                              -gesture_event.data.scroll_update.delta_y);

  if (ShouldAnimate(gesture_event.data.scroll_update.delta_units !=
                    blink::WebGestureEvent::ScrollUnits::kPixels)) {
    DCHECK(!scroll_state.is_in_inertial_phase());
    base::TimeTicks event_time =
        base::TimeTicks() +
        base::TimeDelta::FromSecondsD(gesture_event.TimeStampSeconds());
    base::TimeDelta delay = base::TimeTicks::Now() - event_time;
    switch (input_handler_->ScrollAnimated(scroll_point, scroll_delta, delay)
                .thread) {
      case cc::InputHandler::SCROLL_ON_IMPL_THREAD:
        return DID_HANDLE;
      case cc::InputHandler::SCROLL_IGNORED:
        return DROP_EVENT;
      default:
        return DID_NOT_HANDLE;
    }
  }
  cc::InputHandlerScrollResult scroll_result =
      input_handler_->ScrollBy(&scroll_state);

  if (!scroll_result.did_scroll &&
      input_handler_->ScrollingShouldSwitchtoMainThread() &&
      gesture_event.source_device == blink::kWebGestureDeviceTouchpad &&
      touchpad_and_wheel_scroll_latching_enabled_) {
    gesture_scroll_on_impl_thread_ = false;
    client_->GenerateScrollBeginAndSendToMainThread(gesture_event);

    if (!gesture_pinch_on_impl_thread_)
      return DID_NOT_HANDLE;
  }

  HandleOverscroll(scroll_point, scroll_result, true);

  if (scroll_elasticity_controller_)
    HandleScrollElasticityOverscroll(gesture_event, scroll_result);

  return scroll_result.did_scroll ? DID_HANDLE : DROP_EVENT;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureScrollEnd(
  const WebGestureEvent& gesture_event) {
#ifndef NDEBUG
  DCHECK(expect_scroll_update_end_);
  expect_scroll_update_end_ = false;
#endif
  if (ShouldAnimate(gesture_event.data.scroll_end.delta_units !=
                    blink::WebGestureEvent::ScrollUnits::kPixels)) {
    // Do nothing if the scroll is being animated; the scroll animation will
    // generate the ScrollEnd when it is done.
  } else {
    cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
    input_handler_->ScrollEnd(&scroll_state);
  }

  if (scroll_sequence_ignored_) {
    scroll_sequence_ignored_ = false;
    return DROP_EVENT;
  }

  if (!gesture_scroll_on_impl_thread_)
    return DID_NOT_HANDLE;

  if (scroll_elasticity_controller_)
    HandleScrollElasticityOverscroll(gesture_event,
                                     cc::InputHandlerScrollResult());

  gesture_scroll_on_impl_thread_ = false;
  return DID_HANDLE;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureFlingStart(
    const WebGestureEvent& gesture_event) {
  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  cc::InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      cc::MainThreadScrollingReason::kNotScrollingOnMain;
  switch (gesture_event.source_device) {
    case blink::kWebGestureDeviceTouchpad:
      if (gesture_event.data.fling_start.target_viewport) {
        scroll_status = input_handler_->RootScrollBegin(
            &scroll_state, cc::InputHandler::NON_BUBBLING_GESTURE);
      } else {
        scroll_status = input_handler_->ScrollBegin(
            &scroll_state, cc::InputHandler::NON_BUBBLING_GESTURE);
      }
      break;
    case blink::kWebGestureDeviceTouchscreen:
    case blink::kWebGestureDeviceSyntheticAutoscroll:
      if (!gesture_scroll_on_impl_thread_) {
        scroll_status.thread = cc::InputHandler::SCROLL_ON_MAIN_THREAD;
        scroll_status.main_thread_scrolling_reasons =
            cc::MainThreadScrollingReason::kContinuingMainThreadScroll;
      } else {
        scroll_status = input_handler_->FlingScrollBegin();
      }
      break;
    case blink::kWebGestureDeviceUninitialized:
    case blink::kWebGestureDeviceCount:
      NOTREACHED();
      return DID_NOT_HANDLE;
  }

#ifndef NDEBUG
  expect_scroll_update_end_ = false;
#endif

  switch (scroll_status.thread) {
    case cc::InputHandler::SCROLL_ON_IMPL_THREAD: {
      if (!touchpad_and_wheel_scroll_latching_enabled_ &&
          gesture_event.source_device == blink::kWebGestureDeviceTouchpad) {
        scroll_state.set_is_ending(true);
        input_handler_->ScrollEnd(&scroll_state);
      }

      const float vx = gesture_event.data.fling_start.velocity_x;
      const float vy = gesture_event.data.fling_start.velocity_y;
      current_fling_velocity_ = gfx::Vector2dF(vx, vy);
      DCHECK(!current_fling_velocity_.IsZero());
      fling_curve_ = client_->CreateFlingAnimationCurve(
          gesture_event.source_device, WebFloatPoint(vx, vy), blink::WebSize());
      disallow_horizontal_fling_scroll_ = !vx;
      disallow_vertical_fling_scroll_ = !vy;
      TRACE_EVENT_ASYNC_BEGIN2("input,benchmark,rail",
                               "InputHandlerProxy::HandleGestureFling::started",
                               this, "vx", vx, "vy", vy);
      // Note that the timestamp will only be used to kickstart the animation if
      // its sufficiently close to the timestamp of the first call |Animate()|.
      has_fling_animation_started_ = false;
      fling_parameters_.start_time = gesture_event.TimeStampSeconds();
      fling_parameters_.delta = WebFloatPoint(vx, vy);
      fling_parameters_.point = WebPoint(gesture_event.x, gesture_event.y);
      fling_parameters_.global_point =
          WebPoint(gesture_event.global_x, gesture_event.global_y);
      fling_parameters_.modifiers = gesture_event.GetModifiers();
      fling_parameters_.source_device = gesture_event.source_device;
      RequestAnimation();
      return DID_HANDLE;
    }
    case cc::InputHandler::SCROLL_UNKNOWN:
    case cc::InputHandler::SCROLL_ON_MAIN_THREAD: {
      TRACE_EVENT_INSTANT0("input,rail",
                           "InputHandlerProxy::HandleGestureFling::"
                           "scroll_on_main_thread",
                           TRACE_EVENT_SCOPE_THREAD);
      gesture_scroll_on_impl_thread_ = false;
      fling_may_be_active_on_main_thread_ = true;
      return DID_NOT_HANDLE;
    }
    case cc::InputHandler::SCROLL_IGNORED: {
      TRACE_EVENT_INSTANT0(
          "input,rail",
          "InputHandlerProxy::HandleGestureFling::ignored",
          TRACE_EVENT_SCOPE_THREAD);
      gesture_scroll_on_impl_thread_ = false;
      if (gesture_event.source_device == blink::kWebGestureDeviceTouchpad) {
        // We still pass the curve to the main thread if there's nothing
        // scrollable, in case something
        // registers a handler before the curve is over.
        return DID_NOT_HANDLE;
      }
      return DROP_EVENT;
    }
  }
  return DID_NOT_HANDLE;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HitTestTouchEvent(
    const blink::WebTouchEvent& touch_event,
    bool* is_touching_scrolling_layer,
    cc::TouchAction* white_listed_touch_action) {
  *is_touching_scrolling_layer = false;
  EventDisposition result = DROP_EVENT;
  for (size_t i = 0; i < touch_event.touches_length; ++i) {
    if (touch_event.touch_start_or_first_touch_move)
      DCHECK(white_listed_touch_action);
    else
      DCHECK(!white_listed_touch_action);

    if (touch_event.GetType() == WebInputEvent::kTouchStart &&
        touch_event.touches[i].state != WebTouchPoint::kStatePressed) {
      continue;
    }

    cc::TouchAction touch_action = cc::kTouchActionAuto;
    cc::InputHandler::TouchStartOrMoveEventListenerType event_listener_type =
        input_handler_->EventListenerTypeForTouchStartOrMoveAt(
            gfx::Point(touch_event.touches[i].PositionInWidget().x,
                       touch_event.touches[i].PositionInWidget().y),
            &touch_action);
    if (white_listed_touch_action)
      *white_listed_touch_action &= touch_action;

    if (event_listener_type !=
        cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER) {
      *is_touching_scrolling_layer =
          event_listener_type ==
          cc::InputHandler::TouchStartOrMoveEventListenerType::
              HANDLER_ON_SCROLLING_LAYER;
      result = DID_NOT_HANDLE;
      break;
    }
  }

  // If |result| is DROP_EVENT it wasn't processed above.
  if (result == DROP_EVENT) {
    switch (input_handler_->GetEventListenerProperties(
        cc::EventListenerClass::kTouchStartOrMove)) {
      case cc::EventListenerProperties::kPassive:
        result = DID_HANDLE_NON_BLOCKING;
        break;
      case cc::EventListenerProperties::kBlocking:
        // The touch area rects above already have checked whether it hits
        // a blocking region. Since it does not the event can be dropped.
        result = DROP_EVENT;
        break;
      case cc::EventListenerProperties::kBlockingAndPassive:
        // There is at least one passive listener that needs to possibly
        // be notified so it can't be dropped.
        result = DID_HANDLE_NON_BLOCKING;
        break;
      case cc::EventListenerProperties::kNone:
        result = DROP_EVENT;
        break;
      default:
        NOTREACHED();
        result = DROP_EVENT;
        break;
    }
  }

  // Merge |touch_result_| and |result| so the result has the highest
  // priority value according to the sequence; (DROP_EVENT,
  // DID_HANDLE_NON_BLOCKING, DID_NOT_HANDLE).
  if (touch_result_ == kEventDispositionUndefined ||
      touch_result_ == DROP_EVENT || result == DID_NOT_HANDLE)
    touch_result_ = result;
  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchStart(
    const blink::WebTouchEvent& touch_event) {
  bool is_touching_scrolling_layer;
  cc::TouchAction white_listed_touch_action = cc::kTouchActionAuto;
  EventDisposition result = HitTestTouchEvent(
      touch_event, &is_touching_scrolling_layer, &white_listed_touch_action);
  client_->SetWhiteListedTouchAction(white_listed_touch_action,
                                     touch_event.unique_touch_event_id);

  // If |result| is still DROP_EVENT look at the touch end handler as
  // we may not want to discard the entire touch sequence. Note this
  // code is explicitly after the assignment of the |touch_result_|
  // so the touch moves are not sent to the main thread un-necessarily.
  if (result == DROP_EVENT &&
      input_handler_->GetEventListenerProperties(
          cc::EventListenerClass::kTouchEndOrCancel) !=
          cc::EventListenerProperties::kNone) {
    result = DID_HANDLE_NON_BLOCKING;
  }

  bool is_flinging_on_impl =
      fling_curve_ && !fling_may_be_active_on_main_thread_;
  if (is_flinging_on_impl && is_touching_scrolling_layer)
    result = DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING;

  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchMove(
    const blink::WebTouchEvent& touch_event) {
  // Hit test if this is the first touch move or we don't have any results
  // from a previous hit test.
  if (touch_result_ == kEventDispositionUndefined ||
      touch_event.touch_start_or_first_touch_move) {
    bool is_touching_scrolling_layer;
    cc::TouchAction white_listed_touch_action = cc::kTouchActionAuto;
    EventDisposition result = HitTestTouchEvent(
        touch_event, &is_touching_scrolling_layer, &white_listed_touch_action);
    client_->SetWhiteListedTouchAction(white_listed_touch_action,
                                       touch_event.unique_touch_event_id);
    return result;
  }
  return static_cast<EventDisposition>(touch_result_);
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchEnd(
    const blink::WebTouchEvent& touch_event) {
  if (touch_event.touches_length == 1)
    touch_result_ = kEventDispositionUndefined;
  return DID_NOT_HANDLE;
}

bool InputHandlerProxy::FilterInputEventForFlingBoosting(
    const WebInputEvent& event) {
  if (!WebInputEvent::IsGestureEventType(event.GetType()))
    return false;

  if (!fling_curve_) {
    DCHECK(!deferred_fling_cancel_time_seconds_);
    return false;
  }

  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);
  if (gesture_event.GetType() == WebInputEvent::kGestureFlingCancel) {
    if (gesture_event.data.fling_cancel.prevent_boosting)
      return false;

    if (current_fling_velocity_.LengthSquared() < kMinBoostFlingSpeedSquare)
      return false;

    TRACE_EVENT_INSTANT0("input",
                         "InputHandlerProxy::FlingBoostStart",
                         TRACE_EVENT_SCOPE_THREAD);
    deferred_fling_cancel_time_seconds_ =
        event.TimeStampSeconds() + kFlingBoostTimeoutDelaySeconds;
    return true;
  }

  // A fling is either inactive or is "free spinning", i.e., has yet to be
  // interrupted by a touch gesture, in which case there is nothing to filter.
  if (!deferred_fling_cancel_time_seconds_)
    return false;

  // Gestures from a different source should immediately interrupt the fling.
  if (gesture_event.source_device != fling_parameters_.source_device) {
    CancelCurrentFling();
    return false;
  }

  switch (gesture_event.GetType()) {
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureTapDown:
      return false;

    case WebInputEvent::kGestureScrollBegin:
      if (!input_handler_->IsCurrentlyScrollingLayerAt(
              gfx::Point(gesture_event.x, gesture_event.y),
              fling_parameters_.source_device ==
                      blink::kWebGestureDeviceTouchpad
                  ? cc::InputHandler::NON_BUBBLING_GESTURE
                  : cc::InputHandler::TOUCHSCREEN)) {
        CancelCurrentFling();
        return false;
      }

      // TODO(jdduke): Use |gesture_event.data.scrollBegin.delta{X,Y}Hint| to
      // determine if the ScrollBegin should immediately cancel the fling.
      ExtendBoostedFlingTimeout(gesture_event);
      return true;

    case WebInputEvent::kGestureScrollUpdate: {
      const double time_since_last_boost_event =
          event.TimeStampSeconds() - last_fling_boost_event_.TimeStampSeconds();
      const double time_since_last_fling_animate = std::max(
          0.0, event.TimeStampSeconds() - InSecondsF(last_fling_animate_time_));
      if (ShouldSuppressScrollForFlingBoosting(current_fling_velocity_,
                                               gesture_event,
                                               time_since_last_boost_event,
                                               time_since_last_fling_animate)) {
        ExtendBoostedFlingTimeout(gesture_event);
        return true;
      }

      CancelCurrentFling();
      return false;
    }

    case WebInputEvent::kGestureScrollEnd:
      // Clear the last fling boost event *prior* to fling cancellation,
      // preventing insertion of a synthetic GestureScrollBegin.
      last_fling_boost_event_ = WebGestureEvent();
      CancelCurrentFling();
      return true;

    case WebInputEvent::kGestureFlingStart: {
      DCHECK_EQ(fling_parameters_.source_device, gesture_event.source_device);

      bool fling_boosted =
          fling_parameters_.modifiers == gesture_event.GetModifiers() &&
          ShouldBoostFling(current_fling_velocity_, gesture_event);

      gfx::Vector2dF new_fling_velocity(
          gesture_event.data.fling_start.velocity_x,
          gesture_event.data.fling_start.velocity_y);
      DCHECK(!new_fling_velocity.IsZero());

      if (fling_boosted)
        current_fling_velocity_ += new_fling_velocity;
      else
        current_fling_velocity_ = new_fling_velocity;

      WebFloatPoint velocity(current_fling_velocity_.x(),
                             current_fling_velocity_.y());
      deferred_fling_cancel_time_seconds_ = 0;
      disallow_horizontal_fling_scroll_ = !velocity.x;
      disallow_vertical_fling_scroll_ = !velocity.y;
      last_fling_boost_event_ = WebGestureEvent();
      fling_curve_ = client_->CreateFlingAnimationCurve(
          gesture_event.source_device, velocity, blink::WebSize());
      fling_parameters_.start_time = gesture_event.TimeStampSeconds();
      fling_parameters_.delta = velocity;
      fling_parameters_.point = WebPoint(gesture_event.x, gesture_event.y);
      fling_parameters_.global_point =
          WebPoint(gesture_event.global_x, gesture_event.global_y);

      TRACE_EVENT_INSTANT2("input",
                           fling_boosted ? "InputHandlerProxy::FlingBoosted"
                                         : "InputHandlerProxy::FlingReplaced",
                           TRACE_EVENT_SCOPE_THREAD,
                           "vx",
                           current_fling_velocity_.x(),
                           "vy",
                           current_fling_velocity_.y());

      // The client expects balanced calls between a consumed GestureFlingStart
      // and |DidStopFlinging()|.
      client_->DidStopFlinging();
      return true;
    }

    default:
      // All other types of gestures (taps, presses, etc...) will complete the
      // deferred fling cancellation.
      CancelCurrentFling();
      return false;
  }
}

void InputHandlerProxy::ExtendBoostedFlingTimeout(
    const blink::WebGestureEvent& event) {
  TRACE_EVENT_INSTANT0("input",
                       "InputHandlerProxy::ExtendBoostedFlingTimeout",
                       TRACE_EVENT_SCOPE_THREAD);
  deferred_fling_cancel_time_seconds_ =
      event.TimeStampSeconds() + kFlingBoostTimeoutDelaySeconds;
  last_fling_boost_event_ = event;
}

void InputHandlerProxy::Animate(base::TimeTicks time) {
  // If using synchronous animate, then only expect Animate attempts started by
  // the synchronous system. Don't let the InputHandler try to Animate also.
  DCHECK(!input_handler_->IsCurrentlyScrollingViewport() ||
         allow_root_animate_);

  if (scroll_elasticity_controller_)
    scroll_elasticity_controller_->Animate(time);

  if (!fling_curve_)
    return;

  last_fling_animate_time_ = time;
  double monotonic_time_sec = InSecondsF(time);

  if (deferred_fling_cancel_time_seconds_ &&
      monotonic_time_sec > deferred_fling_cancel_time_seconds_) {
    CancelCurrentFling();
    return;
  }

  client_->DidAnimateForInput();

  if (!has_fling_animation_started_) {
    has_fling_animation_started_ = true;
    // Guard against invalid, future or sufficiently stale start times, as there
    // are no guarantees fling event and animation timestamps are compatible.
    if (!fling_parameters_.start_time ||
        monotonic_time_sec <= fling_parameters_.start_time ||
        monotonic_time_sec >= fling_parameters_.start_time +
                                  kMaxSecondsFromFlingTimestampToFirstAnimate) {
      fling_parameters_.start_time = monotonic_time_sec;
      RequestAnimation();
      return;
    }
  }

  bool fling_is_active = fling_curve_->Apply(
      monotonic_time_sec - fling_parameters_.start_time, this);

  if (disallow_vertical_fling_scroll_ && disallow_horizontal_fling_scroll_)
    fling_is_active = false;

  if (fling_is_active) {
    RequestAnimation();
  } else {
    TRACE_EVENT_INSTANT0("input",
                         "InputHandlerProxy::animate::flingOver",
                         TRACE_EVENT_SCOPE_THREAD);
    if (fling_parameters_.source_device !=
        blink::kWebGestureDeviceSyntheticAutoscroll)
      CancelCurrentFling();
  }
}

void InputHandlerProxy::MainThreadHasStoppedFlinging() {
  fling_may_be_active_on_main_thread_ = false;
  client_->DidStopFlinging();
}

void InputHandlerProxy::ReconcileElasticOverscrollAndRootScroll() {
  if (scroll_elasticity_controller_)
    scroll_elasticity_controller_->ReconcileStretchAndScroll();
}

void InputHandlerProxy::UpdateRootLayerStateForSynchronousInputHandler(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (synchronous_input_handler_) {
    synchronous_input_handler_->UpdateRootLayerState(
        total_scroll_offset, max_scroll_offset, scrollable_size,
        page_scale_factor, min_page_scale_factor, max_page_scale_factor);
  }
}

void InputHandlerProxy::DeliverInputForBeginFrame() {
  DispatchQueuedInputEvents();
}

void InputHandlerProxy::SetOnlySynchronouslyAnimateRootFlings(
    SynchronousInputHandler* synchronous_input_handler) {
  allow_root_animate_ = !synchronous_input_handler;
  synchronous_input_handler_ = synchronous_input_handler;
  if (synchronous_input_handler_)
    input_handler_->RequestUpdateForSynchronousInputHandler();
}

void InputHandlerProxy::SynchronouslyAnimate(base::TimeTicks time) {
  // When this function is used, SetOnlySynchronouslyAnimate() should have been
  // previously called. IOW you should either be entirely in synchronous mode or
  // not.
  DCHECK(synchronous_input_handler_);
  DCHECK(!allow_root_animate_);
  base::AutoReset<bool> reset(&allow_root_animate_, true);
  Animate(time);
}

void InputHandlerProxy::SynchronouslySetRootScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  DCHECK(synchronous_input_handler_);
  input_handler_->SetSynchronousInputHandlerRootScrollOffset(root_offset);
}

void InputHandlerProxy::SynchronouslyZoomBy(float magnify_delta,
                                            const gfx::Point& anchor) {
  DCHECK(synchronous_input_handler_);
  input_handler_->PinchGestureBegin();
  input_handler_->PinchGestureUpdate(magnify_delta, anchor);
  input_handler_->PinchGestureEnd();
}

void InputHandlerProxy::HandleOverscroll(
    const gfx::Point& causal_event_viewport_point,
    const cc::InputHandlerScrollResult& scroll_result,
    bool bundle_overscroll_params_with_ack) {
  DCHECK(client_);
  if (!scroll_result.did_overscroll_root)
    return;

  TRACE_EVENT2("input",
               "InputHandlerProxy::DidOverscroll",
               "dx",
               scroll_result.unused_scroll_delta.x(),
               "dy",
               scroll_result.unused_scroll_delta.y());

  if (fling_curve_) {
    static const int kFlingOverscrollThreshold = 1;
    disallow_horizontal_fling_scroll_ |=
        std::abs(scroll_result.accumulated_root_overscroll.x()) >=
        kFlingOverscrollThreshold;
    disallow_vertical_fling_scroll_ |=
        std::abs(scroll_result.accumulated_root_overscroll.y()) >=
        kFlingOverscrollThreshold;
  }

  if (bundle_overscroll_params_with_ack) {
    // Bundle overscroll message with triggering event response, saving an IPC.
    current_overscroll_params_.reset(new DidOverscrollParams());
    current_overscroll_params_->accumulated_overscroll =
        scroll_result.accumulated_root_overscroll;
    current_overscroll_params_->latest_overscroll_delta =
        scroll_result.unused_scroll_delta;
    current_overscroll_params_->current_fling_velocity =
        ToClientScrollIncrement(current_fling_velocity_);
    current_overscroll_params_->causal_event_viewport_point =
        gfx::PointF(causal_event_viewport_point);
    return;
  }

  client_->DidOverscroll(scroll_result.accumulated_root_overscroll,
                         scroll_result.unused_scroll_delta,
                         ToClientScrollIncrement(current_fling_velocity_),
                         gfx::PointF(causal_event_viewport_point));
}

bool InputHandlerProxy::CancelCurrentFling() {
  if (CancelCurrentFlingWithoutNotifyingClient()) {
    client_->DidStopFlinging();
    return true;
  }
  return false;
}

bool InputHandlerProxy::CancelCurrentFlingWithoutNotifyingClient() {
  bool had_fling_animation = !!fling_curve_;
  if (had_fling_animation &&
      (fling_parameters_.source_device == blink::kWebGestureDeviceTouchscreen ||
       (touchpad_and_wheel_scroll_latching_enabled_ &&
        fling_parameters_.source_device == blink::kWebGestureDeviceTouchpad))) {
    cc::ScrollStateData scroll_state_data;
    scroll_state_data.is_ending = true;
    cc::ScrollState scroll_state(scroll_state_data);
    input_handler_->ScrollEnd(&scroll_state);
    TRACE_EVENT_ASYNC_END0(
        "input",
        "InputHandlerProxy::HandleGestureFling::started",
        this);
  }

  TRACE_EVENT_INSTANT1("input",
                       "InputHandlerProxy::CancelCurrentFling",
                       TRACE_EVENT_SCOPE_THREAD,
                       "had_fling_animation",
                       had_fling_animation);
  fling_curve_.reset();
  has_fling_animation_started_ = false;
  gesture_scroll_on_impl_thread_ = false;
  current_fling_velocity_ = gfx::Vector2dF();
  fling_parameters_ = blink::WebActiveWheelFlingParameters();

  if (deferred_fling_cancel_time_seconds_) {
    deferred_fling_cancel_time_seconds_ = 0;

    WebGestureEvent last_fling_boost_event = last_fling_boost_event_;
    last_fling_boost_event_ = WebGestureEvent();
    if (last_fling_boost_event.GetType() ==
            WebInputEvent::kGestureScrollBegin ||
        last_fling_boost_event.GetType() ==
            WebInputEvent::kGestureScrollUpdate) {
      // Synthesize a GestureScrollBegin, as the original was suppressed.
      HandleInputEvent(ObtainGestureScrollBegin(last_fling_boost_event));
    }
  }

  return had_fling_animation;
}

void InputHandlerProxy::RequestAnimation() {
  // When a SynchronousInputHandler is present, root flings should go through
  // it to allow it to control when or if the root fling is animated. Non-root
  // flings always go through the normal InputHandler.
  if (synchronous_input_handler_ &&
      input_handler_->IsCurrentlyScrollingViewport())
    synchronous_input_handler_->SetNeedsSynchronousAnimateInput();
  else
    input_handler_->SetNeedsAnimateInput();
}

bool InputHandlerProxy::TouchpadFlingScroll(
    const WebFloatSize& increment) {
  InputHandlerProxy::EventDisposition disposition;
  cc::EventListenerProperties properties =
      input_handler_->GetEventListenerProperties(
          cc::EventListenerClass::kMouseWheel);
  switch (properties) {
    case cc::EventListenerProperties::kBlockingAndPassive:
    case cc::EventListenerProperties::kBlocking:
      disposition = DID_NOT_HANDLE;
      break;
    case cc::EventListenerProperties::kPassive:
    case cc::EventListenerProperties::kNone: {
      WebMouseWheelEvent synthetic_wheel(WebInputEvent::kMouseWheel,
                                         fling_parameters_.modifiers,
                                         InSecondsF(base::TimeTicks::Now()));
      synthetic_wheel.delta_x = increment.width;
      synthetic_wheel.delta_y = increment.height;
      synthetic_wheel.has_precise_scrolling_deltas = true;
      synthetic_wheel.phase = WebMouseWheelEvent::kPhaseChanged;
      synthetic_wheel.SetPositionInWidget(fling_parameters_.point.x,
                                          fling_parameters_.point.y);
      synthetic_wheel.SetPositionInScreen(fling_parameters_.global_point.x,
                                          fling_parameters_.global_point.y);

      disposition = FlingScrollByMouseWheel(synthetic_wheel, properties);

      // Send the event over to the main thread.
      if (disposition == DID_HANDLE_NON_BLOCKING) {
        client_->DispatchNonBlockingEventToMainThread(
            ui::WebInputEventTraits::Clone(synthetic_wheel), ui::LatencyInfo());
      }
      break;
    }
    default:
      NOTREACHED();
      return false;
  }

  switch (disposition) {
    case DID_HANDLE:
    case DID_HANDLE_NON_BLOCKING:
      return true;
    case DROP_EVENT:
      break;
    case DID_NOT_HANDLE:
      TRACE_EVENT_INSTANT0("input",
                           "InputHandlerProxy::scrollBy::AbortFling",
                           TRACE_EVENT_SCOPE_THREAD);
      // If we got a DID_NOT_HANDLE, that means we need to deliver wheels on the
      // main thread. In this case we need to schedule a commit and transfer the
      // fling curve over to the main thread and run the rest of the wheels from
      // there. This can happen when flinging a page that contains a scrollable
      // subarea that we can't scroll on the thread if the fling starts outside
      // the subarea but then is flung "under" the pointer.
      client_->TransferActiveWheelFlingAnimation(fling_parameters_);
      fling_may_be_active_on_main_thread_ = true;
      CancelCurrentFlingWithoutNotifyingClient();
      break;
    case DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING:
      NOTREACHED();
      return false;
  }

  return false;
}

bool InputHandlerProxy::ScrollBy(const WebFloatSize& increment,
                                 const WebFloatSize& velocity) {
  WebFloatSize clipped_increment;
  WebFloatSize clipped_velocity;
  if (!disallow_horizontal_fling_scroll_) {
    clipped_increment.width = increment.width;
    clipped_velocity.width = velocity.width;
  }
  if (!disallow_vertical_fling_scroll_) {
    clipped_increment.height = increment.height;
    clipped_velocity.height = velocity.height;
  }

  current_fling_velocity_ = clipped_velocity;

  // Early out if the increment is zero, but avoid early termination if the
  // velocity is still non-zero.
  if (clipped_increment == WebFloatSize())
    return clipped_velocity != WebFloatSize();

  TRACE_EVENT2("input",
               "InputHandlerProxy::scrollBy",
               "x",
               clipped_increment.width,
               "y",
               clipped_increment.height);

  bool did_scroll = false;

  switch (fling_parameters_.source_device) {
    case blink::kWebGestureDeviceTouchpad:
      did_scroll = TouchpadFlingScroll(clipped_increment);
      break;
    case blink::kWebGestureDeviceTouchscreen:
    case blink::kWebGestureDeviceSyntheticAutoscroll: {
      clipped_increment = ToClientScrollIncrement(clipped_increment);
      cc::ScrollStateData scroll_state_data;
      scroll_state_data.delta_x = clipped_increment.width;
      scroll_state_data.delta_y = clipped_increment.height;
      scroll_state_data.velocity_x = clipped_velocity.width;
      scroll_state_data.velocity_y = clipped_velocity.height;
      scroll_state_data.is_in_inertial_phase = true;
      cc::ScrollState scroll_state(scroll_state_data);
      cc::InputHandlerScrollResult scroll_result =
          input_handler_->ScrollBy(&scroll_state);
      HandleOverscroll(fling_parameters_.point, scroll_result, false);
      did_scroll = scroll_result.did_scroll;
    } break;
    case blink::kWebGestureDeviceUninitialized:
    case blink::kWebGestureDeviceCount:
      NOTREACHED();
      return false;
  }

  if (did_scroll) {
    fling_parameters_.cumulative_scroll.width += clipped_increment.width;
    fling_parameters_.cumulative_scroll.height += clipped_increment.height;
  }

  // It's possible the provided |increment| is sufficiently small as to not
  // trigger a scroll, e.g., with a trivial time delta between fling updates.
  // Return true in this case to prevent early fling termination.
  if (std::abs(clipped_increment.width) < kScrollEpsilon &&
      std::abs(clipped_increment.height) < kScrollEpsilon)
    return true;

  return did_scroll;
}

void InputHandlerProxy::HandleScrollElasticityOverscroll(
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  DCHECK(scroll_elasticity_controller_);
  // Send the event and its disposition to the elasticity controller to update
  // the over-scroll animation. Note that the call to the elasticity controller
  // is made asynchronously, to minimize divergence between main thread and
  // impl thread event handling paths.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&InputScrollElasticityController::ObserveGestureEventAndResult,
                 scroll_elasticity_controller_->GetWeakPtr(), gesture_event,
                 scroll_result));
}

void InputHandlerProxy::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  tick_clock_ = std::move(tick_clock);
}

}  // namespace ui
