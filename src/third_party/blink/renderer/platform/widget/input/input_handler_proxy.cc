// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/input/input_handler_proxy.h"

#include <stddef.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/profiler/sample_metadata.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/metrics/event_metrics.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/platform/input/input_handler_proxy_client.h"
#include "third_party/blink/renderer/platform/widget/input/compositor_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"
#include "third_party/blink/renderer/platform/widget/input/momentum_scroll_jank_tracker.h"
#include "third_party/blink/renderer/platform/widget/input/scroll_predictor.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/latency/latency_info.h"

using perfetto::protos::pbzero::ChromeLatencyInfo;
using perfetto::protos::pbzero::TrackEvent;

namespace blink {
namespace {

cc::ScrollState CreateScrollStateForGesture(const WebGestureEvent& event) {
  cc::ScrollStateData scroll_state_data;
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
      scroll_state_data.position_x = event.PositionInWidget().x();
      scroll_state_data.position_y = event.PositionInWidget().y();
      scroll_state_data.delta_x_hint = -event.data.scroll_begin.delta_x_hint;
      scroll_state_data.delta_y_hint = -event.data.scroll_begin.delta_y_hint;
      scroll_state_data.is_beginning = true;
      // On Mac, a GestureScrollBegin in the inertial phase indicates a fling
      // start.
      scroll_state_data.is_in_inertial_phase =
          (event.data.scroll_begin.inertial_phase ==
           WebGestureEvent::InertialPhaseState::kMomentum);
      scroll_state_data.delta_granularity =
          event.data.scroll_begin.delta_hint_units;
      break;
    case WebInputEvent::Type::kGestureScrollUpdate:
      scroll_state_data.delta_x = -event.data.scroll_update.delta_x;
      scroll_state_data.delta_y = -event.data.scroll_update.delta_y;
      scroll_state_data.velocity_x = event.data.scroll_update.velocity_x;
      scroll_state_data.velocity_y = event.data.scroll_update.velocity_y;
      scroll_state_data.is_in_inertial_phase =
          event.data.scroll_update.inertial_phase ==
          WebGestureEvent::InertialPhaseState::kMomentum;
      scroll_state_data.delta_granularity =
          event.data.scroll_update.delta_units;
      break;
    case WebInputEvent::Type::kGestureScrollEnd:
      scroll_state_data.is_ending = true;
      break;
    default:
      NOTREACHED();
      break;
  }
  scroll_state_data.is_direct_manipulation =
      event.SourceDevice() == WebGestureDevice::kTouchscreen;
  return cc::ScrollState(scroll_state_data);
}

cc::ScrollState CreateScrollStateForInertialUpdate(
    const gfx::Vector2dF& delta) {
  cc::ScrollStateData scroll_state_data;
  scroll_state_data.delta_x = delta.x();
  scroll_state_data.delta_y = delta.y();
  scroll_state_data.is_in_inertial_phase = true;
  return cc::ScrollState(scroll_state_data);
}

ui::ScrollInputType GestureScrollInputType(WebGestureDevice device) {
  switch (device) {
    case WebGestureDevice::kTouchpad:
      return ui::ScrollInputType::kWheel;
    case WebGestureDevice::kTouchscreen:
      return ui::ScrollInputType::kTouchscreen;
    case WebGestureDevice::kSyntheticAutoscroll:
      return ui::ScrollInputType::kAutoscroll;
    case WebGestureDevice::kScrollbar:
      return ui::ScrollInputType::kScrollbar;
    case WebGestureDevice::kUninitialized:
      NOTREACHED();
      return ui::ScrollInputType::kMaxValue;
  }
}

cc::SnapFlingController::GestureScrollType GestureScrollEventType(
    WebInputEvent::Type web_event_type) {
  switch (web_event_type) {
    case WebInputEvent::Type::kGestureScrollBegin:
      return cc::SnapFlingController::GestureScrollType::kBegin;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return cc::SnapFlingController::GestureScrollType::kUpdate;
    case WebInputEvent::Type::kGestureScrollEnd:
      return cc::SnapFlingController::GestureScrollType::kEnd;
    default:
      NOTREACHED();
      return cc::SnapFlingController::GestureScrollType::kBegin;
  }
}

cc::SnapFlingController::GestureScrollUpdateInfo GetGestureScrollUpdateInfo(
    const WebGestureEvent& event) {
  cc::SnapFlingController::GestureScrollUpdateInfo info;
  info.delta = gfx::Vector2dF(-event.data.scroll_update.delta_x,
                              -event.data.scroll_update.delta_y);
  info.is_in_inertial_phase = event.data.scroll_update.inertial_phase ==
                              WebGestureEvent::InertialPhaseState::kMomentum;
  info.event_time = event.TimeStamp();
  return info;
}

cc::ScrollBeginThreadState RecordScrollingThread(
    bool scrolling_on_compositor_thread,
    bool blocked_on_main_thread_event_handler,
    WebGestureDevice device) {
  const char* kWheelHistogramName = "Renderer4.ScrollingThread.Wheel";
  const char* kTouchHistogramName = "Renderer4.ScrollingThread.Touch";

  auto status = cc::ScrollBeginThreadState::kScrollingOnMain;
  if (scrolling_on_compositor_thread) {
    status =
        blocked_on_main_thread_event_handler
            ? cc::ScrollBeginThreadState::kScrollingOnCompositorBlockedOnMain
            : cc::ScrollBeginThreadState::kScrollingOnCompositor;
  }

  if (device == WebGestureDevice::kTouchscreen) {
    UMA_HISTOGRAM_ENUMERATION(kTouchHistogramName, status);
  } else if (device == WebGestureDevice::kTouchpad) {
    UMA_HISTOGRAM_ENUMERATION(kWheelHistogramName, status);
  } else {
    NOTREACHED();
  }
  return status;
}

bool IsGestureScrollOrPinch(WebInputEvent::Type type) {
  switch (type) {
    case WebGestureEvent::Type::kGestureScrollBegin:
    case WebGestureEvent::Type::kGestureScrollUpdate:
    case WebGestureEvent::Type::kGestureScrollEnd:
    case WebGestureEvent::Type::kGesturePinchBegin:
    case WebGestureEvent::Type::kGesturePinchUpdate:
    case WebGestureEvent::Type::kGesturePinchEnd:
      return true;
    default:
      return false;
  }
}

}  // namespace

InputHandlerProxy::InputHandlerProxy(cc::InputHandler* input_handler,
                                     InputHandlerProxyClient* client,
                                     bool force_input_to_main_thread)
    : client_(client),
      input_handler_(input_handler),
      synchronous_input_handler_(nullptr),
      handling_gesture_on_impl_thread_(false),
      scroll_sequence_ignored_(false),
      current_overscroll_params_(nullptr),
      has_seen_first_gesture_scroll_update_after_begin_(false),
      last_injected_gesture_was_begin_(false),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      snap_fling_controller_(std::make_unique<cc::SnapFlingController>(this)),
      force_input_to_main_thread_(force_input_to_main_thread) {
  DCHECK(client);
  input_handler_->BindToClient(this);
  cc::ScrollElasticityHelper* scroll_elasticity_helper =
      input_handler_->CreateScrollElasticityHelper();
  if (scroll_elasticity_helper) {
    elastic_overscroll_controller_ =
        ElasticOverscrollController::Create(scroll_elasticity_helper);
  }
  compositor_event_queue_ = std::make_unique<CompositorThreadEventQueue>();
  scroll_predictor_ =
      base::FeatureList::IsEnabled(blink::features::kResamplingScrollEvents)
          ? std::make_unique<ScrollPredictor>()
          : nullptr;

  if (base::FeatureList::IsEnabled(blink::features::kSkipTouchEventFilter) &&
      GetFieldTrialParamValueByFeature(
          blink::features::kSkipTouchEventFilter,
          blink::features::kSkipTouchEventFilterFilteringProcessParamName) ==
          blink::features::
              kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer) {
    // Skipping filtering for touch events on renderer process is enabled.
    // Always skip filtering discrete events.
    skip_touch_filter_discrete_ = true;
    if (GetFieldTrialParamValueByFeature(
            blink::features::kSkipTouchEventFilter,
            blink::features::kSkipTouchEventFilterTypeParamName) ==
        blink::features::kSkipTouchEventFilterTypeParamValueAll) {
      // The experiment config also specifies to skip touchmove events.
      skip_touch_filter_all_ = true;
    }
  }
}

InputHandlerProxy::~InputHandlerProxy() {}

void InputHandlerProxy::WillShutdown() {
  elastic_overscroll_controller_.reset();
  input_handler_ = nullptr;
  client_->WillShutdown();
}

void InputHandlerProxy::HandleInputEventWithLatencyInfo(
    WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info,
    EventDispositionCallback callback) {
  DCHECK(input_handler_);

  input_handler_->NotifyInputEvent();

  TRACE_EVENT("input,benchmark", "LatencyInfo.Flow",
              [&latency_info](perfetto::EventContext ctx) {
                ChromeLatencyInfo* info =
                    ctx.event()->set_chrome_latency_info();
                info->set_trace_id(latency_info.trace_id());
                info->set_step(ChromeLatencyInfo::STEP_HANDLE_INPUT_EVENT_IMPL);
                tracing::FillFlowEvent(ctx, TrackEvent::LegacyEvent::FLOW_INOUT,
                                       latency_info.trace_id());
              });

  std::unique_ptr<EventWithCallback> event_with_callback =
      std::make_unique<EventWithCallback>(std::move(event), latency_info,
                                          tick_clock_->NowTicks(),
                                          std::move(callback));

  enum {
    NO_SCROLL_PINCH = 0,
    ONGOING_SCROLL_PINCH = 1,
    SCROLL_PINCH = 2,
  };
  // Note: Other input can race ahead of gesture input as they don't have to go
  // through the queue, but we believe it's OK to do so.
  if (!IsGestureScrollOrPinch(event_with_callback->event().GetType())) {
    base::ScopedSampleMetadata metadata("Input.GestureScrollOrPinch",
                                        NO_SCROLL_PINCH);
    DispatchSingleInputEvent(std::move(event_with_callback),
                             tick_clock_->NowTicks());
    return;
  }

  base::ScopedSampleMetadata metadata(
      "Input.GestureScrollOrPinch", currently_active_gesture_device_.has_value()
                                        ? ONGOING_SCROLL_PINCH
                                        : SCROLL_PINCH);
  const auto& gesture_event =
      static_cast<const WebGestureEvent&>(event_with_callback->event());
  const bool is_first_gesture_scroll_update =
      !has_seen_first_gesture_scroll_update_after_begin_ &&
      gesture_event.GetType() == WebGestureEvent::Type::kGestureScrollUpdate;

  if (gesture_event.GetType() == WebGestureEvent::Type::kGestureScrollBegin) {
    has_seen_first_gesture_scroll_update_after_begin_ = false;
  } else if (gesture_event.GetType() ==
             WebGestureEvent::Type::kGestureScrollUpdate) {
    has_seen_first_gesture_scroll_update_after_begin_ = true;
  }

  if (currently_active_gesture_device_.has_value()) {
    // Scroll updates should typically be queued and wait until a
    // BeginImplFrame to dispatch. However, the first scroll update to be
    // generated from a *blocking* touch sequence will have waited for the
    // touch event to be ACK'ed by the renderer as unconsumed. Queueing here
    // again until BeginImplFrame means we'll likely add a whole frame of
    // latency to so we flush the queue immediately. This happens only for the
    // first scroll update because once a scroll starts touch events are
    // dispatched non-blocking so scroll updates don't wait for a touch ACK.
    // The |is_source_touch_event_set_non_blocking| bit is set based on the
    // renderer's reply that a blocking touch stream should be made
    // non-blocking. Note: unlike wheel events below, the first GSU in a touch
    // may have come from a non-blocking touch sequence, e.g. if the earlier
    // touchstart determined we're in a |touch-action: pan-y| region. Because
    // of this, we can't simply look at the first GSU like wheels do.
    bool is_from_blocking_touch =
        gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen &&
        gesture_event.is_source_touch_event_set_non_blocking;

    // TODO(bokan): This was added in https://crrev.com/c/557463 before async
    // wheel events. It's not clear to me why flushing on a scroll end would
    // help or why this is specific to wheel events but I suspect it's no
    // longer needed now that wheel scrolling uses non-blocking events.
    bool is_scroll_end_from_wheel =
        gesture_event.SourceDevice() == WebGestureDevice::kTouchpad &&
        gesture_event.GetType() == WebGestureEvent::Type::kGestureScrollEnd;

    // Wheel events have the same issue as the blocking touch issue above.
    // However, all wheel events are initially sent blocking and become non-
    // blocking on the first unconsumed event. We can therefore simply look for
    // the first scroll update in a wheel gesture.
    bool is_first_wheel_scroll_update =
        gesture_event.SourceDevice() == WebGestureDevice::kTouchpad &&
        is_first_gesture_scroll_update;

    // |synchronous_input_handler_| is WebView only. WebView has different
    // mechanisms and we want to forward all events immediately.
    if (is_from_blocking_touch || is_scroll_end_from_wheel ||
        is_first_wheel_scroll_update || synchronous_input_handler_) {
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
  const ui::LatencyInfo& original_latency_info =
      event_with_callback->latency_info();
  ui::LatencyInfo monitored_latency_info = original_latency_info;
  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor =
      input_handler_->CreateLatencyInfoSwapPromiseMonitor(
          &monitored_latency_info);
  auto scoped_event_metrics_monitor =
      input_handler_->GetScopedEventMetricsMonitor(cc::EventMetrics::Create(
          event_with_callback->event().GetTypeAsUiEventType(),
          event_with_callback->event().TimeStamp(),
          event_with_callback->event().GetScrollInputType()));

  current_overscroll_params_.reset();

  WebInputEventAttribution attribution =
      PerformEventAttribution(event_with_callback->event());
  InputHandlerProxy::EventDisposition disposition = RouteToTypeSpecificHandler(
      event_with_callback.get(), original_latency_info, attribution);

  const WebInputEvent& event = event_with_callback->event();
  const WebGestureEvent::Type type = event.GetType();
  switch (type) {
    case WebGestureEvent::Type::kGestureScrollBegin:
    case WebGestureEvent::Type::kGesturePinchBegin:
      if (disposition == DID_HANDLE ||
          disposition == DID_HANDLE_SHOULD_BUBBLE) {
        currently_active_gesture_device_ =
            static_cast<const WebGestureEvent&>(event).SourceDevice();
      }
      break;

    case WebGestureEvent::Type::kGestureScrollEnd:
    case WebGestureEvent::Type::kGesturePinchEnd:
      if (!handling_gesture_on_impl_thread_)
        currently_active_gesture_device_ = base::nullopt;
      break;
    default:
      break;
  }

  // Handle jank tracking during the momentum phase of a scroll gesture. The
  // class filters non-momentum events internally.
  switch (type) {
    case WebGestureEvent::Type::kGestureScrollBegin:
      momentum_scroll_jank_tracker_ =
          std::make_unique<MomentumScrollJankTracker>();
      break;
    case WebGestureEvent::Type::kGestureScrollUpdate:
      // It's possible to get a scroll update without a begin. Ignore these
      // cases.
      if (momentum_scroll_jank_tracker_) {
        momentum_scroll_jank_tracker_->OnDispatchedInputEvent(
            event_with_callback.get(), now);
      }
      break;
    case WebGestureEvent::Type::kGestureScrollEnd:
      momentum_scroll_jank_tracker_.reset();
      break;
    default:
      break;
  }

  // Will run callback for every original events.
  event_with_callback->RunCallbacks(disposition, monitored_latency_info,
                                    std::move(current_overscroll_params_),
                                    attribution);
}

void InputHandlerProxy::DispatchQueuedInputEvents() {
  // Calling |NowTicks()| is expensive so we only want to do it once.
  base::TimeTicks now = tick_clock_->NowTicks();
  while (!compositor_event_queue_->empty())
    DispatchSingleInputEvent(compositor_event_queue_->Pop(), now);
}

// This function handles creating synthetic Gesture events. It is currently used
// for creating Gesture event equivalents for mouse events on a composited
// scrollbar. (See InputHandlerProxy::HandleInputEvent)
void InputHandlerProxy::InjectScrollbarGestureScroll(
    const WebInputEvent::Type type,
    const gfx::PointF& position_in_widget,
    const cc::InputHandlerPointerResult& pointer_result,
    const ui::LatencyInfo& latency_info,
    const base::TimeTicks original_timestamp) {
  gfx::Vector2dF scroll_delta(pointer_result.scroll_offset.x(),
                              pointer_result.scroll_offset.y());

  std::unique_ptr<WebGestureEvent> synthetic_gesture_event =
      WebGestureEvent::GenerateInjectedScrollGesture(
          type, original_timestamp, WebGestureDevice::kScrollbar,
          position_in_widget, scroll_delta, pointer_result.scroll_units);

  // This will avoid hit testing and directly scroll the scroller with the
  // provided element_id.
  if (type == WebInputEvent::Type::kGestureScrollBegin) {
    synthetic_gesture_event->data.scroll_begin.scrollable_area_element_id =
        pointer_result.target_scroller.GetStableId();
  }

  WebScopedInputEvent web_scoped_gesture_event(
      synthetic_gesture_event.release());

  // Send in a LatencyInfo with SCROLLBAR type so that the end to end latency
  // is calculated specifically for scrollbars.
  ui::LatencyInfo scrollbar_latency_info(latency_info);
  scrollbar_latency_info.set_source_event_type(ui::SourceEventType::SCROLLBAR);

  // This latency_info should not have already been scheduled for rendering -
  // i.e. it should be the original latency_info that was associated with the
  // input event that caused this scroll injection. If it has already been
  // scheduled it won't get queued to be shipped off with the CompositorFrame
  // when the gesture is handled.
  DCHECK(!scrollbar_latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, nullptr));

  if (type == WebInputEvent::Type::kGestureScrollBegin) {
    last_injected_gesture_was_begin_ = true;
  } else {
    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      // For injected GSUs, add a scroll update component to the latency info
      // so that it is properly classified as a scroll. If the last injected
      // gesture was a GSB, then this GSU is the first scroll update - mark
      // the LatencyInfo as such.
      scrollbar_latency_info.AddLatencyNumberWithTimestamp(
          (last_injected_gesture_was_begin_)
              ? ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT
              : ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          original_timestamp);
    }

    last_injected_gesture_was_begin_ = false;
  }

  std::unique_ptr<EventWithCallback> gesture_event_with_callback_update =
      std::make_unique<EventWithCallback>(
          std::move(web_scoped_gesture_event), scrollbar_latency_info,
          original_timestamp, original_timestamp, nullptr);

  bool needs_animate_input = compositor_event_queue_->empty();
  compositor_event_queue_->Queue(std::move(gesture_event_with_callback_update),
                                 original_timestamp);

  if (needs_animate_input)
    input_handler_->SetNeedsAnimateInput();
}

bool HasModifier(const WebInputEvent& event) {
#if defined(OS_MACOSX)
  // Mac uses the "Option" key (which is mapped to the enum "kAltKey").
  return event.GetModifiers() & WebInputEvent::kAltKey;
#else
  return event.GetModifiers() & WebInputEvent::kShiftKey;
#endif
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::RouteToTypeSpecificHandler(
    EventWithCallback* event_with_callback,
    const ui::LatencyInfo& original_latency_info,
    const WebInputEventAttribution& original_attribution) {
  DCHECK(input_handler_);

  if (force_input_to_main_thread_)
    return DID_NOT_HANDLE;

  const WebInputEvent& event = event_with_callback->event();
  if (event.IsGestureScroll() &&
      (snap_fling_controller_->FilterEventForSnap(
          GestureScrollEventType(event.GetType())))) {
    return DROP_EVENT;
  }

  switch (event.GetType()) {
    case WebInputEvent::Type::kMouseWheel:
      return HandleMouseWheel(static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::Type::kGestureScrollBegin:
      return HandleGestureScrollBegin(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::Type::kGestureScrollUpdate:
      return HandleGestureScrollUpdate(
          static_cast<const WebGestureEvent&>(event), original_attribution);

    case WebInputEvent::Type::kGestureScrollEnd:
      return HandleGestureScrollEnd(static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::Type::kGesturePinchBegin: {
      DCHECK(!gesture_pinch_in_progress_);
      input_handler_->PinchGestureBegin();
      gesture_pinch_in_progress_ = true;
      return DID_HANDLE;
    }

    case WebInputEvent::Type::kGesturePinchEnd: {
      DCHECK(gesture_pinch_in_progress_);
      gesture_pinch_in_progress_ = false;
      const WebGestureEvent& gesture_event =
          static_cast<const WebGestureEvent&>(event);
      input_handler_->PinchGestureEnd(
          gfx::ToFlooredPoint(gesture_event.PositionInWidget()),
          gesture_event.SourceDevice() == WebGestureDevice::kTouchpad);
      return DID_HANDLE;
    }

    case WebInputEvent::Type::kGesturePinchUpdate: {
      DCHECK(gesture_pinch_in_progress_);
      const WebGestureEvent& gesture_event =
          static_cast<const WebGestureEvent&>(event);
      input_handler_->PinchGestureUpdate(
          gesture_event.data.pinch_update.scale,
          gfx::ToFlooredPoint(gesture_event.PositionInWidget()));
      return DID_HANDLE;
    }

    case WebInputEvent::Type::kTouchStart:
      return HandleTouchStart(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::Type::kTouchMove:
      return HandleTouchMove(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::Type::kTouchEnd:
      return HandleTouchEnd(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::Type::kMouseDown: {
      // Only for check scrollbar captured
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);

      if (mouse_event.button == WebMouseEvent::Button::kLeft) {
        CHECK(input_handler_);
        // TODO(arakeri): Pass in the modifier instead of a bool once the
        // refactor (crbug.com/1022097) is done. For details, see
        // crbug.com/1016955.
        cc::InputHandlerPointerResult pointer_result =
            input_handler_->MouseDown(
                gfx::PointF(mouse_event.PositionInWidget()),
                HasModifier(event));
        if (pointer_result.type == cc::PointerResultType::kScrollbarScroll) {
          // Since a kScrollbarScroll is about to commence, ensure that any
          // existing ongoing scroll is ended.
          if (currently_active_gesture_device_.has_value()) {
            DCHECK_NE(*currently_active_gesture_device_,
                      WebGestureDevice::kUninitialized);
            if (gesture_pinch_in_progress_) {
              input_handler_->PinchGestureEnd(
                  gfx::ToFlooredPoint(mouse_event.PositionInWidget()), true);
            }
            if (handling_gesture_on_impl_thread_) {
              input_handler_->RecordScrollEnd(
                  GestureScrollInputType(*currently_active_gesture_device_));
              InputHandlerScrollEnd();
            }
          }

          // Generate GSB and GSU events and add them to the
          // CompositorThreadEventQueue.
          // Note that the latency info passed in to
          // InjectScrollbarGestureScroll is the original LatencyInfo, not the
          // one that may be currently monitored. The currently monitored one
          // may be modified by the call to InjectScrollbarGestureScroll, as
          // it will SetNeedsAnimateInput if the CompositorThreadEventQueue is
          // currently empty.
          InjectScrollbarGestureScroll(WebInputEvent::Type::kGestureScrollBegin,
                                       mouse_event.PositionInWidget(),
                                       pointer_result, original_latency_info,
                                       mouse_event.TimeStamp());

          // Don't need to inject GSU if the scroll offset is zero (this can
          // be the case where mouse down occurs on the thumb).
          if (!pointer_result.scroll_offset.IsZero()) {
            InjectScrollbarGestureScroll(
                WebInputEvent::Type::kGestureScrollUpdate,
                mouse_event.PositionInWidget(), pointer_result,
                original_latency_info, mouse_event.TimeStamp());
          }

          if (event_with_callback) {
            event_with_callback
                ->SetScrollbarManipulationHandledOnCompositorThread();
          }
        }
      }

      return DID_NOT_HANDLE;
    }
    case WebInputEvent::Type::kMouseUp: {
      // Only for release scrollbar captured
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);

      if (mouse_event.button == WebMouseEvent::Button::kLeft) {
        CHECK(input_handler_);
        cc::InputHandlerPointerResult pointer_result = input_handler_->MouseUp(
            gfx::PointF(mouse_event.PositionInWidget()));
        if (pointer_result.type == cc::PointerResultType::kScrollbarScroll) {
          // Generate a GSE and add it to the CompositorThreadEventQueue.
          InjectScrollbarGestureScroll(WebInputEvent::Type::kGestureScrollEnd,
                                       mouse_event.PositionInWidget(),
                                       pointer_result, original_latency_info,
                                       mouse_event.TimeStamp());

          if (event_with_callback) {
            event_with_callback
                ->SetScrollbarManipulationHandledOnCompositorThread();
          }
        }
      }
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::Type::kMouseMove: {
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);
      // TODO(davemoore): This should never happen, but bug #326635 showed some
      // surprising crashes.
      CHECK(input_handler_);
      cc::InputHandlerPointerResult pointer_result =
          input_handler_->MouseMoveAt(
              gfx::Point(mouse_event.PositionInWidget().x(),
                         mouse_event.PositionInWidget().y()));
      if (pointer_result.type == cc::PointerResultType::kScrollbarScroll) {
        // Generate a GSU event and add it to the CompositorThreadEventQueue if
        // delta is non zero.
        if (!pointer_result.scroll_offset.IsZero()) {
          InjectScrollbarGestureScroll(
              WebInputEvent::Type::kGestureScrollUpdate,
              mouse_event.PositionInWidget(), pointer_result,
              original_latency_info, mouse_event.TimeStamp());
        }

        if (event_with_callback) {
          event_with_callback
              ->SetScrollbarManipulationHandledOnCompositorThread();
        }
      }
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::Type::kMouseLeave: {
      CHECK(input_handler_);
      input_handler_->MouseLeave();
      return DID_NOT_HANDLE;
    }
    // Fling gestures are handled only in the browser process and not sent to
    // the renderer.
    case WebInputEvent::Type::kGestureFlingStart:
    case WebInputEvent::Type::kGestureFlingCancel:
      NOTREACHED();
      break;

    default:
      break;
  }

  return DID_NOT_HANDLE;
}

WebInputEventAttribution InputHandlerProxy::PerformEventAttribution(
    const WebInputEvent& event) {
  if (WebInputEvent::IsKeyboardEventType(event.GetType())) {
    // Keyboard events should be dispatched to the focused frame.
    return WebInputEventAttribution(WebInputEventAttribution::kFocusedFrame);
  } else if (WebInputEvent::IsMouseEventType(event.GetType()) ||
             event.GetType() == WebInputEvent::Type::kMouseWheel) {
    // Mouse events are dispatched based on their location in the DOM tree.
    // Perform frame attribution via cc.
    // TODO(acomminos): handle pointer locks, or provide a hint to the renderer
    //                  to check pointer lock state
    gfx::PointF point =
        static_cast<const WebMouseEvent&>(event).PositionInWidget();
    return WebInputEventAttribution(
        WebInputEventAttribution::kTargetedFrame,
        input_handler_->FindFrameElementIdAtPoint(point));
  } else {
    // TODO(acomminos): implement for more event types (pointer, touch)
    return WebInputEventAttribution(WebInputEventAttribution::kUnknown);
  }
}

void InputHandlerProxy::RecordMainThreadScrollingReasons(
    WebGestureDevice device,
    uint32_t reasons) {
  static const char* kGestureHistogramName =
      "Renderer4.MainThreadGestureScrollReason";
  static const char* kWheelHistogramName =
      "Renderer4.MainThreadWheelScrollReason";

  if (device != WebGestureDevice::kTouchpad &&
      device != WebGestureDevice::kTouchscreen) {
    return;
  }

  // NonCompositedScrollReasons should only be set on the main thread.
  DCHECK(
      !cc::MainThreadScrollingReason::HasNonCompositedScrollReasons(reasons));

  // This records whether a scroll is handled on the main or compositor
  // threads. Note: scrolls handled on the compositor but blocked on main due
  // to event handlers are still considered compositor scrolls.
  const bool is_compositor_scroll =
      reasons == cc::MainThreadScrollingReason::kNotScrollingOnMain;

  base::Optional<EventDisposition> disposition =
      (device == WebGestureDevice::kTouchpad ? mouse_wheel_result_
                                             : touch_result_);

  // Scrolling can be handled on the compositor thread but it might be blocked
  // on the main thread waiting for non-passive event handlers to process the
  // wheel/touch events (i.e. were they preventDefaulted?).
  bool blocked_on_main_thread_handler =
      disposition.has_value() && disposition == DID_NOT_HANDLE;

  auto scroll_start_state = RecordScrollingThread(
      is_compositor_scroll, blocked_on_main_thread_handler, device);
  input_handler_->RecordScrollBegin(GestureScrollInputType(device),
                                    scroll_start_state);

  if (blocked_on_main_thread_handler) {
    // We should also collect main thread scrolling reasons if a scroll event
    // scrolls on impl thread but is blocked by main thread event handlers.
    reasons |= (device == WebGestureDevice::kTouchpad
                    ? cc::MainThreadScrollingReason::kWheelEventHandlerRegion
                    : cc::MainThreadScrollingReason::kTouchEventHandlerRegion);
  }

  // Note: This is slightly different from |is_compositor_scroll| above because
  // at this point, we've also included wheel handler region reasons which will
  // scroll on the compositor but require blocking on the main thread. The
  // histograms below don't consider this "not scrolling on main".
  const bool is_unblocked_compositor_scroll =
      reasons == cc::MainThreadScrollingReason::kNotScrollingOnMain;

  // UMA_HISTOGRAM_ENUMERATION requires that the enum_max must be strictly
  // greater than the sample value. kMainThreadScrollingReasonCount doesn't
  // include the NotScrollingOnMain enum but the histograms do so adding
  // the +1 is necessary.
  // TODO(dcheng): Fix https://crbug.com/705169 so this isn't needed.
  constexpr uint32_t kMainThreadScrollingReasonEnumMax =
      cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount + 1;
  if (is_unblocked_compositor_scroll) {
    if (device == WebGestureDevice::kTouchscreen) {
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
      if (device == WebGestureDevice::kTouchscreen) {
        UMA_HISTOGRAM_ENUMERATION(kGestureHistogramName, i + 1,
                                  kMainThreadScrollingReasonEnumMax);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kWheelHistogramName, i + 1,
                                  kMainThreadScrollingReasonEnumMax);
      }
    }
  }
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleMouseWheel(
    const WebMouseWheelEvent& wheel_event) {
  InputHandlerProxy::EventDisposition result = DROP_EVENT;

  if (wheel_event.dispatch_type ==
      WebInputEvent::DispatchType::kEventNonBlocking) {
    // The first wheel event in the sequence should be cancellable.
    DCHECK(wheel_event.phase != WebMouseWheelEvent::kPhaseBegan);
    // Noncancellable wheel events should have phase info.
    DCHECK(wheel_event.phase != WebMouseWheelEvent::kPhaseNone ||
           wheel_event.momentum_phase != WebMouseWheelEvent::kPhaseNone);

    DCHECK(mouse_wheel_result_.has_value());
    // TODO(bokan): This should never happen but after changing
    // mouse_event_result_ to a base::Optional, crashes indicate that it does
    // so |if| maintains prior behavior. https://crbug.com/1069760.
    if (mouse_wheel_result_.has_value()) {
      result = mouse_wheel_result_.value();
      if (wheel_event.phase == WebMouseWheelEvent::kPhaseEnded ||
          wheel_event.phase == WebMouseWheelEvent::kPhaseCancelled ||
          wheel_event.momentum_phase == WebMouseWheelEvent::kPhaseEnded ||
          wheel_event.momentum_phase == WebMouseWheelEvent::kPhaseCancelled) {
        mouse_wheel_result_.reset();
      } else {
        return result;
      }
    }
  }

  gfx::PointF position_in_widget = wheel_event.PositionInWidget();
  if (input_handler_->HasBlockingWheelEventHandlerAt(
          gfx::Point(position_in_widget.x(), position_in_widget.y()))) {
    result = DID_NOT_HANDLE;
  } else {
    cc::EventListenerProperties properties =
        input_handler_->GetEventListenerProperties(
            cc::EventListenerClass::kMouseWheel);
    switch (properties) {
      case cc::EventListenerProperties::kBlockingAndPassive:
      case cc::EventListenerProperties::kPassive:
        result = DID_HANDLE_NON_BLOCKING;
        break;
      case cc::EventListenerProperties::kNone:
        result = DROP_EVENT;
        break;
      default:
        // If properties is kBlocking, and the event falls outside wheel event
        // handler region, we should handle it the same as kNone.
        result = DROP_EVENT;
    }
  }

  mouse_wheel_result_ = result;
  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureScrollBegin(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "InputHandlerProxy::HandleGestureScrollBegin");

  if (scroll_predictor_)
    scroll_predictor_->ResetOnGestureScrollBegin(gesture_event);

  // When a GSB is being handled, end any pre-existing gesture scrolls that are
  // in progress.
  if (currently_active_gesture_device_.has_value() &&
      handling_gesture_on_impl_thread_) {
    // TODO(arakeri): Once crbug.com/1074209 is fixed, delete calls to
    // RecordScrollEnd.
    input_handler_->RecordScrollEnd(
        GestureScrollInputType(*currently_active_gesture_device_));
    InputHandlerScrollEnd();
  }

  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  cc::InputHandler::ScrollStatus scroll_status;
  cc::ElementIdType element_id_type =
      gesture_event.data.scroll_begin.scrollable_area_element_id;
  if (element_id_type) {
    scroll_state.data()->set_current_native_scrolling_element(
        cc::ElementId(element_id_type));
  }
  if (gesture_event.data.scroll_begin.delta_hint_units ==
      ui::ScrollGranularity::kScrollByPage) {
    scroll_status.thread = cc::InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        cc::MainThreadScrollingReason::kContinuingMainThreadScroll;
  } else if (gesture_event.data.scroll_begin.target_viewport) {
    scroll_status = input_handler_->RootScrollBegin(
        &scroll_state, GestureScrollInputType(gesture_event.SourceDevice()));
  } else {
    scroll_status = input_handler_->ScrollBegin(
        &scroll_state, GestureScrollInputType(gesture_event.SourceDevice()));
  }
  RecordMainThreadScrollingReasons(gesture_event.SourceDevice(),
                                   scroll_status.main_thread_scrolling_reasons);

  InputHandlerProxy::EventDisposition result = DID_NOT_HANDLE;
  scroll_sequence_ignored_ = false;
  in_inertial_scrolling_ = false;
  switch (scroll_status.thread) {
    case cc::InputHandler::SCROLL_ON_IMPL_THREAD:
      TRACE_EVENT_INSTANT0("input", "Handle On Impl", TRACE_EVENT_SCOPE_THREAD);
      handling_gesture_on_impl_thread_ = true;
      if (input_handler_->IsCurrentlyScrollingViewport())
        client_->DidStartScrollingViewport();

      if (scroll_status.bubble)
        result = DID_HANDLE_SHOULD_BUBBLE;
      else
        result = DID_HANDLE;
      break;
    case cc::InputHandler::SCROLL_UNKNOWN:
    case cc::InputHandler::SCROLL_ON_MAIN_THREAD:
      TRACE_EVENT_INSTANT0("input", "Handle On Main", TRACE_EVENT_SCOPE_THREAD);
      result = DID_NOT_HANDLE;
      break;
    case cc::InputHandler::SCROLL_IGNORED:
      TRACE_EVENT_INSTANT0("input", "Ignore Scroll", TRACE_EVENT_SCOPE_THREAD);
      scroll_sequence_ignored_ = true;
      result = DROP_EVENT;
      break;
  }
  if (elastic_overscroll_controller_ && result != DID_NOT_HANDLE) {
    HandleScrollElasticityOverscroll(gesture_event,
                                     cc::InputHandlerScrollResult());
  }

  return result;
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleGestureScrollUpdate(
    const WebGestureEvent& gesture_event,
    const WebInputEventAttribution& original_attribution) {
  TRACE_EVENT2("input", "InputHandlerProxy::HandleGestureScrollUpdate", "dx",
               -gesture_event.data.scroll_update.delta_x, "dy",
               -gesture_event.data.scroll_update.delta_y);

  if (scroll_sequence_ignored_) {
    TRACE_EVENT_INSTANT0("input", "Scroll Sequence Ignored",
                         TRACE_EVENT_SCOPE_THREAD);
    return DROP_EVENT;
  }

  if (!handling_gesture_on_impl_thread_ && !gesture_pinch_in_progress_)
    return DID_NOT_HANDLE;

  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  in_inertial_scrolling_ = scroll_state.is_in_inertial_phase();

  TRACE_EVENT_INSTANT1(
      "input", "DeltaUnits", TRACE_EVENT_SCOPE_THREAD, "unit",
      static_cast<int>(gesture_event.data.scroll_update.delta_units));

  if (snap_fling_controller_->HandleGestureScrollUpdate(
          GetGestureScrollUpdateInfo(gesture_event))) {
    handling_gesture_on_impl_thread_ = false;
    return DROP_EVENT;
  }

  if (input_handler_->ScrollingShouldSwitchtoMainThread()) {
    TRACE_EVENT_INSTANT0("input", "Move Scroll To Main Thread",
                         TRACE_EVENT_SCOPE_THREAD);
    handling_gesture_on_impl_thread_ = false;
    currently_active_gesture_device_ = base::nullopt;
    client_->GenerateScrollBeginAndSendToMainThread(gesture_event,
                                                    original_attribution);

    // TODO(bokan): |!gesture_pinch_in_progress_| was put here by
    // https://crrev.com/2720903005 but it's not clear to me how this is
    // supposed to work - we already generated and sent a GSB to the main
    // thread above so it's odd to continue handling on the compositor thread
    // if a pinch was in progress. It probably makes more sense to bake this
    // condition into ScrollingShouldSwitchToMainThread().
    if (!gesture_pinch_in_progress_)
      return DID_NOT_HANDLE;
  }

  base::TimeTicks event_time = gesture_event.TimeStamp();
  base::TimeDelta delay = base::TimeTicks::Now() - event_time;

  cc::InputHandlerScrollResult scroll_result =
      input_handler_->ScrollUpdate(&scroll_state, delay);

  HandleOverscroll(gesture_event.PositionInWidget(), scroll_result);

  if (elastic_overscroll_controller_)
    HandleScrollElasticityOverscroll(gesture_event, scroll_result);

  return scroll_result.did_scroll ? DID_HANDLE : DROP_EVENT;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureScrollEnd(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "InputHandlerProxy::HandleGestureScrollEnd");
  input_handler_->RecordScrollEnd(
      GestureScrollInputType(gesture_event.SourceDevice()));

  if (scroll_sequence_ignored_) {
    DCHECK(!currently_active_gesture_device_.has_value());
    return DROP_EVENT;
  }

  if (!handling_gesture_on_impl_thread_) {
    DCHECK(!currently_active_gesture_device_.has_value());
    return DID_NOT_HANDLE;
  }

  if (!currently_active_gesture_device_.has_value() ||
      (currently_active_gesture_device_.value() !=
       gesture_event.SourceDevice()))
    return DROP_EVENT;

  InputHandlerScrollEnd();
  if (elastic_overscroll_controller_) {
    HandleScrollElasticityOverscroll(gesture_event,
                                     cc::InputHandlerScrollResult());
  }

  return DID_HANDLE;
}

void InputHandlerProxy::InputHandlerScrollEnd() {
  input_handler_->ScrollEnd(/*should_snap=*/true);
  handling_gesture_on_impl_thread_ = false;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HitTestTouchEvent(
    const WebTouchEvent& touch_event,
    bool* is_touching_scrolling_layer,
    cc::TouchAction* white_listed_touch_action) {
  TRACE_EVENT1("input", "InputHandlerProxy::HitTestTouchEvent",
               "Needs whitelisted TouchAction",
               static_cast<bool>(white_listed_touch_action));
  *is_touching_scrolling_layer = false;
  EventDisposition result = DROP_EVENT;
  for (size_t i = 0; i < touch_event.touches_length; ++i) {
    if (touch_event.touch_start_or_first_touch_move)
      DCHECK(white_listed_touch_action);
    else
      DCHECK(!white_listed_touch_action);

    if (touch_event.GetType() == WebInputEvent::Type::kTouchStart &&
        touch_event.touches[i].state != WebTouchPoint::State::kStatePressed) {
      continue;
    }

    cc::TouchAction touch_action = cc::TouchAction::kAuto;
    cc::InputHandler::TouchStartOrMoveEventListenerType event_listener_type =
        input_handler_->EventListenerTypeForTouchStartOrMoveAt(
            gfx::Point(touch_event.touches[i].PositionInWidget().x(),
                       touch_event.touches[i].PositionInWidget().y()),
            &touch_action);
    if (white_listed_touch_action && touch_action != cc::TouchAction::kAuto) {
      TRACE_EVENT_INSTANT1("input", "Adding TouchAction",
                           TRACE_EVENT_SCOPE_THREAD, "TouchAction",
                           cc::TouchActionToString(touch_action));
      *white_listed_touch_action &= touch_action;
    }

    if (event_listener_type !=
        cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER) {
      TRACE_EVENT_INSTANT1("input", "HaveHandler", TRACE_EVENT_SCOPE_THREAD,
                           "Type", event_listener_type);

      *is_touching_scrolling_layer =
          event_listener_type ==
          cc::InputHandler::TouchStartOrMoveEventListenerType::
              HANDLER_ON_SCROLLING_LAYER;

      // A non-passive touch start / move will always set the whitelisted touch
      // action to TouchAction::kNone, and in that case we do not ack the event
      // from the compositor.
      if (white_listed_touch_action &&
          *white_listed_touch_action != cc::TouchAction::kNone) {
        TRACE_EVENT_INSTANT0("input",
                             "NonBlocking due to whitelisted touchaction",
                             TRACE_EVENT_SCOPE_THREAD);
        result = DID_HANDLE_NON_BLOCKING;
      } else {
        TRACE_EVENT_INSTANT0("input", "DidNotHandle due to no touchaction",
                             TRACE_EVENT_SCOPE_THREAD);
        result = DID_NOT_HANDLE;
      }
      break;
    }
  }

  // If |result| is DROP_EVENT it wasn't processed above.
  if (result == DROP_EVENT) {
    auto event_listener_class = input_handler_->GetEventListenerProperties(
        cc::EventListenerClass::kTouchStartOrMove);
    TRACE_EVENT_INSTANT1("input", "DropEvent", TRACE_EVENT_SCOPE_THREAD,
                         "listener", event_listener_class);
    switch (event_listener_class) {
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

  // Depending on which arm of the SkipTouchEventFilter experiment we're on, we
  // may need to simulate a passive listener instead of dropping touch events.
  if (result == DROP_EVENT &&
      (skip_touch_filter_all_ ||
       (skip_touch_filter_discrete_ &&
        touch_event.GetType() == WebInputEvent::Type::kTouchStart))) {
    TRACE_EVENT_INSTANT0("input", "Non blocking due to skip filter",
                         TRACE_EVENT_SCOPE_THREAD);
    result = DID_HANDLE_NON_BLOCKING;
  }

  // Merge |touch_result_| and |result| so the result has the highest
  // priority value according to the sequence; (DROP_EVENT,
  // DID_HANDLE_NON_BLOCKING, DID_NOT_HANDLE).
  if (!touch_result_.has_value() || touch_result_ == DROP_EVENT ||
      result == DID_NOT_HANDLE) {
    TRACE_EVENT_INSTANT2(
        "input", "Update touch_result_", TRACE_EVENT_SCOPE_THREAD, "old",
        (touch_result_ ? touch_result_.value() : -1), "new", result);
    touch_result_ = result;
  }

  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchStart(
    const WebTouchEvent& touch_event) {
  TRACE_EVENT0("input", "InputHandlerProxy::HandleTouchStart");

  bool is_touching_scrolling_layer;
  cc::TouchAction white_listed_touch_action = cc::TouchAction::kAuto;
  EventDisposition result = HitTestTouchEvent(
      touch_event, &is_touching_scrolling_layer, &white_listed_touch_action);
  TRACE_EVENT_INSTANT1("input", "HitTest", TRACE_EVENT_SCOPE_THREAD,
                       "disposition", result);

  // If |result| is still DROP_EVENT look at the touch end handler as we may
  // not want to discard the entire touch sequence. Note this code is
  // explicitly after the assignment of the |touch_result_| in
  // HitTestTouchEvent so the touch moves are not sent to the main thread
  // un-necessarily.
  if (result == DROP_EVENT && input_handler_->GetEventListenerProperties(
                                  cc::EventListenerClass::kTouchEndOrCancel) !=
                                  cc::EventListenerProperties::kNone) {
    TRACE_EVENT_INSTANT0("input", "NonBlocking due to TouchEnd handler",
                         TRACE_EVENT_SCOPE_THREAD);
    result = DID_HANDLE_NON_BLOCKING;
  }

  bool is_in_inertial_scrolling_on_impl =
      in_inertial_scrolling_ && handling_gesture_on_impl_thread_;
  if (is_in_inertial_scrolling_on_impl && is_touching_scrolling_layer) {
    // If the touchstart occurs during a fling, it will be ACK'd immediately
    // and it and its following touch moves will be dispatched as non-blocking.
    // Due to tap suppression on the browser side, this will reset the
    // browser-side touch action (see comment in
    // TouchActionFilter::FilterGestureEvent for GestureScrollBegin). Ensure we
    // send back a white_listed_touch_action that matches this non-blocking
    // behavior rather than treating it as if it'll block.
    TRACE_EVENT_INSTANT0("input", "NonBlocking due to fling",
                         TRACE_EVENT_SCOPE_THREAD);
    white_listed_touch_action = cc::TouchAction::kAuto;
    result = DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING;
  }

  TRACE_EVENT_INSTANT2("input", "Whitelisted TouchAction",
                       TRACE_EVENT_SCOPE_THREAD, "TouchAction",
                       cc::TouchActionToString(white_listed_touch_action),
                       "disposition", result);
  client_->SetWhiteListedTouchAction(white_listed_touch_action,
                                     touch_event.unique_touch_event_id, result);

  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchMove(
    const WebTouchEvent& touch_event) {
  TRACE_EVENT2("input", "InputHandlerProxy::HandleTouchMove", "touch_result",
               touch_result_.has_value() ? touch_result_.value() : -1,
               "is_start_or_first",
               touch_event.touch_start_or_first_touch_move);
  // Hit test if this is the first touch move or we don't have any results
  // from a previous hit test.
  if (!touch_result_.has_value() ||
      touch_event.touch_start_or_first_touch_move) {
    bool is_touching_scrolling_layer;
    cc::TouchAction white_listed_touch_action = cc::TouchAction::kAuto;
    EventDisposition result = HitTestTouchEvent(
        touch_event, &is_touching_scrolling_layer, &white_listed_touch_action);
    TRACE_EVENT_INSTANT2("input", "Whitelisted TouchAction",
                         TRACE_EVENT_SCOPE_THREAD, "TouchAction",
                         cc::TouchActionToString(white_listed_touch_action),
                         "disposition", result);
    client_->SetWhiteListedTouchAction(
        white_listed_touch_action, touch_event.unique_touch_event_id, result);
    return result;
  }
  return touch_result_.value();
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchEnd(
    const WebTouchEvent& touch_event) {
  TRACE_EVENT1("input", "InputHandlerProxy::HandleTouchEnd", "num_touches",
               touch_event.touches_length);
  if (touch_event.touches_length == 1)
    touch_result_.reset();
  return DID_NOT_HANDLE;
}

void InputHandlerProxy::Animate(base::TimeTicks time) {
  if (elastic_overscroll_controller_)
    elastic_overscroll_controller_->Animate(time);

  snap_fling_controller_->Animate(time);
}

void InputHandlerProxy::ReconcileElasticOverscrollAndRootScroll() {
  if (elastic_overscroll_controller_)
    elastic_overscroll_controller_->ReconcileStretchAndScroll();
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

void InputHandlerProxy::DeliverInputForBeginFrame(
    const viz::BeginFrameArgs& args) {
  if (!scroll_predictor_)
    DispatchQueuedInputEvents();

  // Resampling GSUs and dispatch queued input events.
  while (!compositor_event_queue_->empty()) {
    std::unique_ptr<EventWithCallback> event_with_callback =
        scroll_predictor_->ResampleScrollEvents(compositor_event_queue_->Pop(),
                                                args.frame_time);

    DispatchSingleInputEvent(std::move(event_with_callback), args.frame_time);
  }
}

void InputHandlerProxy::DeliverInputForHighLatencyMode() {
  // When prediction enabled, do not handle input after commit complete.
  if (!scroll_predictor_)
    DispatchQueuedInputEvents();
}

void InputHandlerProxy::SetSynchronousInputHandler(
    SynchronousInputHandler* synchronous_input_handler) {
  synchronous_input_handler_ = synchronous_input_handler;
  if (synchronous_input_handler_)
    input_handler_->RequestUpdateForSynchronousInputHandler();
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
  input_handler_->PinchGestureEnd(anchor, false);
}

bool InputHandlerProxy::GetSnapFlingInfoAndSetAnimatingSnapTarget(
    const gfx::Vector2dF& natural_displacement,
    gfx::Vector2dF* initial_offset,
    gfx::Vector2dF* target_offset) const {
  return input_handler_->GetSnapFlingInfoAndSetAnimatingSnapTarget(
      natural_displacement, initial_offset, target_offset);
}

gfx::Vector2dF InputHandlerProxy::ScrollByForSnapFling(
    const gfx::Vector2dF& delta) {
  cc::ScrollState scroll_state = CreateScrollStateForInertialUpdate(delta);

  cc::InputHandlerScrollResult scroll_result =
      input_handler_->ScrollUpdate(&scroll_state, base::TimeDelta());
  return scroll_result.current_visual_offset;
}

void InputHandlerProxy::ScrollEndForSnapFling(bool did_finish) {
  input_handler_->ScrollEndForSnapFling(did_finish);
}

void InputHandlerProxy::RequestAnimationForSnapFling() {
  RequestAnimation();
}

void InputHandlerProxy::HandleOverscroll(
    const gfx::PointF& causal_event_viewport_point,
    const cc::InputHandlerScrollResult& scroll_result) {
  DCHECK(client_);
  if (!scroll_result.did_overscroll_root)
    return;

  TRACE_EVENT2("input", "InputHandlerProxy::DidOverscroll", "dx",
               scroll_result.unused_scroll_delta.x(), "dy",
               scroll_result.unused_scroll_delta.y());

  // Bundle overscroll message with triggering event response, saving an IPC.
  current_overscroll_params_ = std::make_unique<DidOverscrollParams>();
  current_overscroll_params_->accumulated_overscroll =
      scroll_result.accumulated_root_overscroll;
  current_overscroll_params_->latest_overscroll_delta =
      scroll_result.unused_scroll_delta;
  current_overscroll_params_->causal_event_viewport_point =
      causal_event_viewport_point;
  current_overscroll_params_->overscroll_behavior =
      scroll_result.overscroll_behavior;
  return;
}

void InputHandlerProxy::RequestAnimation() {
  input_handler_->SetNeedsAnimateInput();
}

void InputHandlerProxy::HandleScrollElasticityOverscroll(
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  DCHECK(elastic_overscroll_controller_);
  elastic_overscroll_controller_->ObserveGestureEventAndResult(gesture_event,
                                                               scroll_result);
}

void InputHandlerProxy::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

}  // namespace blink
