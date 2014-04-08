// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_provider.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace ui {
namespace {

// Double-tap drag zoom sensitivity (speed).
const float kDoubleTapDragZoomSpeed = 0.005f;

const char* GetMotionEventActionName(MotionEvent::Action action) {
  switch(action) {
    case MotionEvent::ACTION_POINTER_DOWN: return "ACTION_POINTER_DOWN";
    case MotionEvent::ACTION_POINTER_UP:   return "ACTION_POINTER_UP";
    case MotionEvent::ACTION_DOWN:         return "ACTION_DOWN";
    case MotionEvent::ACTION_UP:           return "ACTION_UP";
    case MotionEvent::ACTION_CANCEL:       return "ACTION_CANCEL";
    case MotionEvent::ACTION_MOVE:         return "ACTION_MOVE";
  }
  return "";
}

GestureEventData CreateGesture(EventType type,
                               int motion_event_id,
                               base::TimeTicks time,
                               float x,
                               float y,
                               const GestureEventDetails& details) {
  return GestureEventData(type, motion_event_id, time, x, y, details);
}

GestureEventData CreateGesture(EventType type,
                               int motion_event_id,
                               base::TimeTicks time,
                               float x,
                               float y) {
  return GestureEventData(type, motion_event_id, time, x, y);
}

GestureEventData CreateGesture(EventType type,
                               const MotionEvent& event,
                               const GestureEventDetails& details) {
  return CreateGesture(type,
                       event.GetId(),
                       event.GetEventTime(),
                       event.GetX(),
                       event.GetY(),
                       details);
}

GestureEventData CreateGesture(EventType type,
                               const MotionEvent& event) {
  return CreateGesture(
      type, event.GetId(), event.GetEventTime(), event.GetX(), event.GetY());
}

GestureEventDetails CreateTapGestureDetails(EventType type,
                                            const MotionEvent& event) {
  // Set the tap count to 1 even for ET_GESTURE_DOUBLE_TAP, in order to be
  // consistent with double tap behavior on a mobile viewport. See
  // crbug.com/234986 for context.
  GestureEventDetails tap_details(type, 1, 0);
  tap_details.set_bounding_box(
      gfx::RectF(event.GetTouchMajor(), event.GetTouchMajor()));
  return tap_details;
}

}  // namespace

// GestureProvider:::Config

GestureProvider::Config::Config()
    : disable_click_delay(false), gesture_begin_end_types_enabled(false) {}

GestureProvider::Config::~Config() {}

// GestureProvider::ScaleGestureListener

class GestureProvider::ScaleGestureListenerImpl
    : public ScaleGestureDetector::ScaleGestureListener {
 public:
  ScaleGestureListenerImpl(const ScaleGestureDetector::Config& config,
                           float device_scale_factor,
                           GestureProvider* provider)
      : scale_gesture_detector_(config, this),
        provider_(provider),
        px_to_dp_(1.0f / device_scale_factor),
        ignore_multitouch_events_(false),
        pinch_event_sent_(false) {}

  bool OnTouchEvent(const MotionEvent& event) {
    // TODO: Need to deal with multi-touch transition.
    const bool in_scale_gesture = IsScaleGestureDetectionInProgress();
    bool handled = scale_gesture_detector_.OnTouchEvent(event);
    if (!in_scale_gesture &&
        (event.GetAction() == MotionEvent::ACTION_UP ||
         event.GetAction() == MotionEvent::ACTION_CANCEL)) {
      return false;
    }
    return handled;
  }

  // ScaleGestureDetector::ScaleGestureListener implementation.
  virtual bool OnScaleBegin(const ScaleGestureDetector& detector,
                            const MotionEvent& e) OVERRIDE {
    if (ignore_multitouch_events_ && !detector.InDoubleTapMode())
      return false;
    pinch_event_sent_ = false;
    return true;
  }

  virtual void OnScaleEnd(const ScaleGestureDetector& detector,
                          const MotionEvent& e) OVERRIDE {
    if (!pinch_event_sent_)
      return;
    provider_->Send(CreateGesture(
        ET_GESTURE_PINCH_END, e.GetId(), detector.GetEventTime(), 0, 0));
    pinch_event_sent_ = false;
  }

  virtual bool OnScale(const ScaleGestureDetector& detector,
                       const MotionEvent& e) OVERRIDE {
    if (ignore_multitouch_events_ && !detector.InDoubleTapMode())
      return false;
    if (!pinch_event_sent_) {
      pinch_event_sent_ = true;
      provider_->Send(CreateGesture(ET_GESTURE_PINCH_BEGIN,
                                    e.GetId(),
                                    detector.GetEventTime(),
                                    detector.GetFocusX(),
                                    detector.GetFocusY()));
    }

    float scale = detector.GetScaleFactor();
    if (scale == 1)
      return true;

    if (detector.InDoubleTapMode()) {
      // Relative changes in the double-tap scale factor computed by |detector|
      // diminish as the touch moves away from the original double-tap focus.
      // For historical reasons, Chrome has instead adopted a scale factor
      // computation that is invariant to the focal distance, where
      // the scale delta remains constant if the touch velocity is constant.
      float dy =
          (detector.GetCurrentSpanY() - detector.GetPreviousSpanY()) * 0.5f;
      scale = std::pow(scale > 1 ? 1.0f + kDoubleTapDragZoomSpeed
                                 : 1.0f - kDoubleTapDragZoomSpeed,
                       std::abs(dy * px_to_dp_));
    }
    GestureEventDetails pinch_details(ET_GESTURE_PINCH_UPDATE, scale, 0);
    provider_->Send(CreateGesture(ET_GESTURE_PINCH_UPDATE,
                                  e.GetId(),
                                  detector.GetEventTime(),
                                  detector.GetFocusX(),
                                  detector.GetFocusY(),
                                  pinch_details));
    return true;
  }

  void SetDoubleTapEnabled(bool enabled) {
    DCHECK(!IsDoubleTapInProgress());
    scale_gesture_detector_.SetQuickScaleEnabled(enabled);
  }

  void SetMultiTouchEnabled(bool value) {
    // Note that returning false from OnScaleBegin / OnScale makes the
    // gesture detector not to emit further scaling notifications
    // related to this gesture. Thus, if detector events are enabled in
    // the middle of the gesture, we don't need to do anything.
    ignore_multitouch_events_ = value;
  }

  bool IsDoubleTapInProgress() const {
    return IsScaleGestureDetectionInProgress() && InDoubleTapMode();
  }

  bool IsScaleGestureDetectionInProgress() const {
    return scale_gesture_detector_.IsInProgress();
  }

 private:
  bool InDoubleTapMode() const {
    return scale_gesture_detector_.InDoubleTapMode();
  }

  ScaleGestureDetector scale_gesture_detector_;

  GestureProvider* const provider_;

  // TODO(jdduke): Remove this when all MotionEvent's use DIPs.
  const float px_to_dp_;

  // Completely silence multi-touch (pinch) scaling events. Used in WebView when
  // zoom support is turned off.
  bool ignore_multitouch_events_;

  // Whether any pinch zoom event has been sent to native.
  bool pinch_event_sent_;

  DISALLOW_COPY_AND_ASSIGN(ScaleGestureListenerImpl);
};

// GestureProvider::GestureListener

class GestureProvider::GestureListenerImpl
    : public GestureDetector::GestureListener,
      public GestureDetector::DoubleTapListener {
 public:
  GestureListenerImpl(
      const GestureDetector::Config& gesture_detector_config,
      const SnapScrollController::Config& snap_scroll_controller_config,
      bool disable_click_delay,
      GestureProvider* provider)
      : gesture_detector_(gesture_detector_config, this, this),
        snap_scroll_controller_(snap_scroll_controller_config),
        provider_(provider),
        disable_click_delay_(disable_click_delay),
        scaled_touch_slop_(gesture_detector_config.scaled_touch_slop),
        scaled_touch_slop_square_(scaled_touch_slop_ * scaled_touch_slop_),
        double_tap_timeout_(gesture_detector_config.double_tap_timeout),
        ignore_single_tap_(false),
        seen_first_scroll_event_(false),
        last_raw_x_(0),
        last_raw_y_(0),
        accumulated_scroll_error_x_(0),
        accumulated_scroll_error_y_(0) {}

  virtual ~GestureListenerImpl() {}

  bool OnTouchEvent(const MotionEvent& e,
                    bool is_scale_gesture_detection_in_progress) {
    snap_scroll_controller_.SetSnapScrollingMode(
        e, is_scale_gesture_detection_in_progress);

    if (is_scale_gesture_detection_in_progress)
      SetIgnoreSingleTap(true);

    if (e.GetAction() == MotionEvent::ACTION_DOWN)
      gesture_detector_.set_is_longpress_enabled(true);

    return gesture_detector_.OnTouchEvent(e);
  }

  // GestureDetector::GestureListener implementation.
  virtual bool OnDown(const MotionEvent& e) OVERRIDE {
    current_down_time_ = e.GetEventTime();
    ignore_single_tap_ = false;
    seen_first_scroll_event_ = false;
    last_raw_x_ = e.GetRawX();
    last_raw_y_ = e.GetRawY();
    accumulated_scroll_error_x_ = 0;
    accumulated_scroll_error_y_ = 0;

    GestureEventDetails tap_details(ET_GESTURE_TAP_DOWN, 0, 0);
    tap_details.set_bounding_box(
        gfx::RectF(e.GetTouchMajor(), e.GetTouchMajor()));
    provider_->Send(CreateGesture(ET_GESTURE_TAP_DOWN, e, tap_details));

    // Return true to indicate that we want to handle touch.
    return true;
  }

  virtual bool OnScroll(const MotionEvent& e1,
                        const MotionEvent& e2,
                        float raw_distance_x,
                        float raw_distance_y) OVERRIDE {
    float distance_x = raw_distance_x;
    float distance_y = raw_distance_y;
    if (!seen_first_scroll_event_) {
      // Remove the touch slop region from the first scroll event to avoid a
      // jump.
      seen_first_scroll_event_ = true;
      double distance =
          std::sqrt(distance_x * distance_x + distance_y * distance_y);
      double epsilon = 1e-3;
      if (distance > epsilon) {
        double ratio = std::max(0., distance - scaled_touch_slop_) / distance;
        distance_x *= ratio;
        distance_y *= ratio;
      }
    }
    snap_scroll_controller_.UpdateSnapScrollMode(distance_x, distance_y);
    if (snap_scroll_controller_.IsSnappingScrolls()) {
      if (snap_scroll_controller_.IsSnapHorizontal()) {
        distance_y = 0;
      } else {
        distance_x = 0;
      }
    }

    last_raw_x_ = e2.GetRawX();
    last_raw_y_ = e2.GetRawY();
    if (!provider_->IsScrollInProgress()) {
      // Note that scroll start hints are in distance traveled, where
      // scroll deltas are in the opposite direction.
      GestureEventDetails scroll_details(
          ET_GESTURE_SCROLL_BEGIN, -raw_distance_x, -raw_distance_y);
      provider_->Send(CreateGesture(ET_GESTURE_SCROLL_BEGIN,
                                    e2.GetId(),
                                    e2.GetEventTime(),
                                    e1.GetX(),
                                    e1.GetY(),
                                    scroll_details));
    }

    // distance_x and distance_y is the scrolling offset since last OnScroll.
    // Because we are passing integers to Blink, this could introduce
    // rounding errors. The rounding errors will accumulate overtime.
    // To solve this, we should be adding back the rounding errors each time
    // when we calculate the new offset.
    // TODO(jdduke): Determine if we can simpy use floating point deltas, as
    // WebGestureEvent also takes floating point deltas for GestureScrollUpdate.
    int dx = (int)(distance_x + accumulated_scroll_error_x_);
    int dy = (int)(distance_y + accumulated_scroll_error_y_);
    accumulated_scroll_error_x_ += (distance_x - dx);
    accumulated_scroll_error_y_ += (distance_y - dy);

    if (dx || dy) {
      GestureEventDetails scroll_details(ET_GESTURE_SCROLL_UPDATE, -dx, -dy);
      provider_->Send(
          CreateGesture(ET_GESTURE_SCROLL_UPDATE, e2, scroll_details));
    }

    return true;
  }

  virtual bool OnFling(const MotionEvent& e1,
                       const MotionEvent& e2,
                       float velocity_x,
                       float velocity_y) OVERRIDE {
    if (snap_scroll_controller_.IsSnappingScrolls()) {
      if (snap_scroll_controller_.IsSnapHorizontal()) {
        velocity_y = 0;
      } else {
        velocity_x = 0;
      }
    }

    provider_->Fling(e2, velocity_x, velocity_y);
    return true;
  }

  virtual void OnShowPress(const MotionEvent& e) OVERRIDE {
    GestureEventDetails show_press_details(ET_GESTURE_SHOW_PRESS, 0, 0);
    // TODO(jdduke): Expose minor axis length and rotation in |MotionEvent|.
    show_press_details.set_bounding_box(
        gfx::RectF(e.GetTouchMajor(), e.GetTouchMajor()));
    provider_->Send(
        CreateGesture(ET_GESTURE_SHOW_PRESS, e, show_press_details));
  }

  virtual bool OnSingleTapUp(const MotionEvent& e) OVERRIDE {
    if (IsPointOutsideCurrentSlopRegion(e.GetRawX(), e.GetRawY())) {
      provider_->SendTapCancelIfNecessary(e);
      ignore_single_tap_ = true;
      return true;
    }
    // This is a hack to address the issue where user hovers
    // over a link for longer than double_tap_timeout_, then
    // OnSingleTapConfirmed() is not triggered. But we still
    // want to trigger the tap event at UP. So we override
    // OnSingleTapUp() in this case. This assumes singleTapUp
    // gets always called before singleTapConfirmed.
    if (!ignore_single_tap_) {
      if (e.GetEventTime() - current_down_time_ > double_tap_timeout_) {
        return OnSingleTapConfirmed(e);
      } else if (!IsDoubleTapEnabled() || disable_click_delay_) {
        // If double-tap has been disabled, there is no need to wait
        // for the double-tap timeout.
        return OnSingleTapConfirmed(e);
      } else {
        // Notify Blink about this tapUp event anyway, when none of the above
        // conditions applied.
        provider_->Send(CreateGesture(
            ET_GESTURE_TAP_UNCONFIRMED,
            e,
            CreateTapGestureDetails(ET_GESTURE_TAP_UNCONFIRMED, e)));
      }
    }

    return provider_->SendLongTapIfNecessary(e);
  }

  // GestureDetector::DoubleTapListener implementation.
  virtual bool OnSingleTapConfirmed(const MotionEvent& e) OVERRIDE {
    // Long taps in the edges of the screen have their events delayed by
    // ContentViewHolder for tab swipe operations. As a consequence of the delay
    // this method might be called after receiving the up event.
    // These corner cases should be ignored.
    if (ignore_single_tap_)
      return true;

    ignore_single_tap_ = true;

    provider_->Send(CreateGesture(
        ET_GESTURE_TAP, e, CreateTapGestureDetails(ET_GESTURE_TAP, e)));
    return true;
  }

  virtual bool OnDoubleTap(const MotionEvent& e) OVERRIDE { return false; }

  virtual bool OnDoubleTapEvent(const MotionEvent& e) OVERRIDE {
    switch (e.GetAction()) {
      case MotionEvent::ACTION_DOWN:
        gesture_detector_.set_is_longpress_enabled(false);
        break;

      case MotionEvent::ACTION_UP:
        if (!provider_->IsPinchInProgress() &&
            !provider_->IsScrollInProgress()) {
          provider_->Send(
              CreateGesture(ET_GESTURE_DOUBLE_TAP,
                            e,
                            CreateTapGestureDetails(ET_GESTURE_DOUBLE_TAP, e)));
          return true;
        }
        break;
      default:
        break;
    }
    return false;
  }

  virtual bool OnLongPress(const MotionEvent& e) OVERRIDE {
    DCHECK(!IsDoubleTapInProgress());
    SetIgnoreSingleTap(true);

    GestureEventDetails long_press_details(ET_GESTURE_LONG_PRESS, 0, 0);
    long_press_details.set_bounding_box(
        gfx::RectF(e.GetTouchMajor(), e.GetTouchMajor()));
    provider_->Send(
        CreateGesture(ET_GESTURE_LONG_PRESS, e, long_press_details));

    // Returning true puts the GestureDetector in "longpress" mode, disabling
    // further scrolling.  This is undesirable, as it is quite common for a
    // longpress gesture to fire on content that won't trigger a context menu.
    return false;
  }

  void SetDoubleTapEnabled(bool enabled) {
    DCHECK(!IsDoubleTapInProgress());
    if (enabled) {
      gesture_detector_.set_doubletap_listener(this);
    } else {
      // TODO(jdduke): Send GESTURE_TAP if GESTURE_TAP_UNCONFIRMED already sent.
      gesture_detector_.set_doubletap_listener(NULL);
    }
  }

  bool IsClickDelayDisabled() const { return disable_click_delay_; }

  bool IsDoubleTapInProgress() const {
    return gesture_detector_.is_double_tapping();
  }

 private:
  bool IsPointOutsideCurrentSlopRegion(float x, float y) const {
    return IsDistanceGreaterThanTouchSlop(last_raw_x_ - x, last_raw_y_ - y);
  }

  bool IsDistanceGreaterThanTouchSlop(float distance_x,
                                      float distance_y) const {
    return distance_x * distance_x + distance_y * distance_y >
           scaled_touch_slop_square_;
  }

  void SetIgnoreSingleTap(bool value) { ignore_single_tap_ = value; }

  bool IsDoubleTapEnabled() const {
    return gesture_detector_.has_doubletap_listener();
  }

  GestureDetector gesture_detector_;
  SnapScrollController snap_scroll_controller_;

  GestureProvider* const provider_;

  // Whether the click delay should always be disabled by sending clicks for
  // double-tap gestures.
  const bool disable_click_delay_;

  const int scaled_touch_slop_;

  // Cache of square of the scaled touch slop so we don't have to calculate it
  // on every touch.
  const int scaled_touch_slop_square_;

  const base::TimeDelta double_tap_timeout_;

  base::TimeTicks current_down_time_;

  // TODO(klobag): This is to avoid a bug in GestureDetector. With multi-touch,
  // always_in_tap_region_ is not reset. So when the last finger is up,
  // OnSingleTapUp() will be mistakenly fired.
  bool ignore_single_tap_;

  // Used to remove the touch slop from the initial scroll event in a scroll
  // gesture.
  bool seen_first_scroll_event_;

  // Used to track the last rawX/Y coordinates for moves.  This gives absolute
  // scroll distance.
  // Useful for full screen tracking.
  float last_raw_x_;
  float last_raw_y_;

  // Used to track the accumulated scroll error over time. This is used to
  // remove the
  // rounding error we introduced by passing integers to webkit.
  float accumulated_scroll_error_x_;
  float accumulated_scroll_error_y_;

  DISALLOW_COPY_AND_ASSIGN(GestureListenerImpl);
};

// GestureProvider

GestureProvider::GestureProvider(const Config& config,
                                 GestureProviderClient* client)
    : client_(client),
      needs_show_press_event_(false),
      needs_tap_ending_event_(false),
      touch_scroll_in_progress_(false),
      pinch_in_progress_(false),
      double_tap_support_for_page_(true),
      double_tap_support_for_platform_(true),
      gesture_begin_end_types_enabled_(config.gesture_begin_end_types_enabled) {
  DCHECK(client);
  InitGestureDetectors(config);
}

GestureProvider::~GestureProvider() {}

bool GestureProvider::OnTouchEvent(const MotionEvent& event) {
  TRACE_EVENT1("input", "GestureProvider::OnTouchEvent",
               "action", GetMotionEventActionName(event.GetAction()));
  if (!CanHandle(event))
    return false;

  const bool in_scale_gesture =
      scale_gesture_listener_->IsScaleGestureDetectionInProgress();

  OnTouchEventHandlingBegin(event);
  gesture_listener_->OnTouchEvent(event, in_scale_gesture);
  scale_gesture_listener_->OnTouchEvent(event);
  OnTouchEventHandlingEnd(event);
  return true;
}

void GestureProvider::ResetGestureDetectors() {
  if (!current_down_event_)
    return;
  scoped_ptr<MotionEvent> cancel_event = current_down_event_->Cancel();
  gesture_listener_->OnTouchEvent(*cancel_event, false);
  scale_gesture_listener_->OnTouchEvent(*cancel_event);
}

void GestureProvider::SetMultiTouchSupportEnabled(bool enabled) {
  scale_gesture_listener_->SetMultiTouchEnabled(!enabled);
}

void GestureProvider::SetDoubleTapSupportForPlatformEnabled(bool enabled) {
  double_tap_support_for_platform_ = enabled;
  UpdateDoubleTapDetectionSupport();
}

void GestureProvider::SetDoubleTapSupportForPageEnabled(bool enabled) {
  double_tap_support_for_page_ = enabled;
  UpdateDoubleTapDetectionSupport();
}

bool GestureProvider::IsScrollInProgress() const {
  // TODO(wangxianzhu): Also return true when fling is active once the UI knows
  // exactly when the fling ends.
  return touch_scroll_in_progress_;
}

bool GestureProvider::IsPinchInProgress() const { return pinch_in_progress_; }

bool GestureProvider::IsDoubleTapInProgress() const {
  return gesture_listener_->IsDoubleTapInProgress() ||
         scale_gesture_listener_->IsDoubleTapInProgress();
}

bool GestureProvider::IsDoubleTapSupported() const {
  return double_tap_support_for_page_ && double_tap_support_for_platform_;
}

bool GestureProvider::IsClickDelayDisabled() const {
  return gesture_listener_->IsClickDelayDisabled();
}

void GestureProvider::InitGestureDetectors(const Config& config) {
  TRACE_EVENT0("input", "GestureProvider::InitGestureDetectors");
  gesture_listener_.reset(
      new GestureListenerImpl(config.gesture_detector_config,
                              config.snap_scroll_controller_config,
                              config.disable_click_delay,
                              this));

  scale_gesture_listener_.reset(new ScaleGestureListenerImpl(
      config.scale_gesture_detector_config,
      config.snap_scroll_controller_config.device_scale_factor,
      this));

  UpdateDoubleTapDetectionSupport();
}

bool GestureProvider::CanHandle(const MotionEvent& event) const {
  return event.GetAction() == MotionEvent::ACTION_DOWN || current_down_event_;
}

void GestureProvider::Fling(const MotionEvent& event,
                            float velocity_x,
                            float velocity_y) {
  if (!velocity_x && !velocity_y) {
    EndTouchScrollIfNecessary(event, true);
    return;
  }

  if (!touch_scroll_in_progress_) {
    // The native side needs a ET_GESTURE_SCROLL_BEGIN before
    // ET_SCROLL_FLING_START to send the fling to the correct target. Send if it
    // has not sent.  The distance traveled in one second is a reasonable scroll
    // start hint.
    GestureEventDetails scroll_details(
        ET_GESTURE_SCROLL_BEGIN, velocity_x, velocity_y);
    Send(CreateGesture(ET_GESTURE_SCROLL_BEGIN, event, scroll_details));
  }
  EndTouchScrollIfNecessary(event, false);

  GestureEventDetails fling_details(
      ET_SCROLL_FLING_START, velocity_x, velocity_y);
  Send(CreateGesture(
      ET_SCROLL_FLING_START, event, fling_details));
}

void GestureProvider::Send(const GestureEventData& gesture) {
  DCHECK(!gesture.time.is_null());
  // The only valid events that should be sent without an active touch sequence
  // are SHOW_PRESS and TAP, potentially triggered by the double-tap
  // delay timing out.
  DCHECK(current_down_event_ || gesture.type == ET_GESTURE_TAP ||
         gesture.type == ET_GESTURE_SHOW_PRESS);

  switch (gesture.type) {
    case ET_GESTURE_TAP_DOWN:
      needs_tap_ending_event_ = true;
      needs_show_press_event_ = true;
      break;
    case ET_GESTURE_TAP_UNCONFIRMED:
      needs_show_press_event_ = false;
      break;
    case ET_GESTURE_TAP:
      if (needs_show_press_event_)
        Send(CreateGesture(ET_GESTURE_SHOW_PRESS,
                           gesture.motion_event_id,
                           gesture.time,
                           gesture.x,
                           gesture.y));
      needs_tap_ending_event_ = false;
      break;
    case ET_GESTURE_DOUBLE_TAP:
      needs_tap_ending_event_ = false;
      break;
    case ET_GESTURE_TAP_CANCEL:
      if (!needs_tap_ending_event_)
        return;
      needs_tap_ending_event_ = false;
      break;
    case ET_GESTURE_SHOW_PRESS:
      needs_show_press_event_ = false;
      break;
    case ET_GESTURE_LONG_PRESS:
      DCHECK(!scale_gesture_listener_->IsScaleGestureDetectionInProgress());
      current_longpress_time_ = gesture.time;
      break;
    case ET_GESTURE_LONG_TAP:
      needs_tap_ending_event_ = false;
      current_longpress_time_ = base::TimeTicks();
      break;
    case ET_GESTURE_SCROLL_BEGIN:
      touch_scroll_in_progress_ = true;
      SendTapCancelIfNecessary(gesture);
      break;
    case ET_GESTURE_SCROLL_END:
      touch_scroll_in_progress_ = false;
      break;
    case ET_GESTURE_PINCH_BEGIN:
      if (!touch_scroll_in_progress_)
        Send(CreateGesture(ET_GESTURE_SCROLL_BEGIN,
                           gesture.motion_event_id,
                           gesture.time,
                           gesture.x,
                           gesture.y));
      pinch_in_progress_ = true;
      break;
    case ET_GESTURE_PINCH_END:
      pinch_in_progress_ = false;
      break;
    default:
      break;
  };

  client_->OnGestureEvent(gesture);
}

void GestureProvider::SendTapCancelIfNecessary(const MotionEvent& event) {
  if (!needs_tap_ending_event_)
    return;
  current_longpress_time_ = base::TimeTicks();
  Send(CreateGesture(ET_GESTURE_TAP_CANCEL, event));
}

void GestureProvider::SendTapCancelIfNecessary(
    const GestureEventData& gesture) {
  if (!needs_tap_ending_event_)
    return;
  current_longpress_time_ = base::TimeTicks();
  Send(CreateGesture(ET_GESTURE_TAP_CANCEL,
                     gesture.motion_event_id,
                     gesture.time,
                     gesture.x,
                     gesture.y));
}

bool GestureProvider::SendLongTapIfNecessary(const MotionEvent& event) {
  if (event.GetAction() == MotionEvent::ACTION_UP &&
      !current_longpress_time_.is_null() &&
      !scale_gesture_listener_->IsScaleGestureDetectionInProgress()) {
    SendTapCancelIfNecessary(event);
    GestureEventDetails long_tap_details(ET_GESTURE_LONG_TAP, 0, 0);
    long_tap_details.set_bounding_box(
        gfx::RectF(event.GetTouchMajor(), event.GetTouchMajor()));
    Send(CreateGesture(ET_GESTURE_LONG_TAP, event, long_tap_details));
    return true;
  }
  return false;
}

void GestureProvider::EndTouchScrollIfNecessary(const MotionEvent& event,
                                                bool send_scroll_end_event) {
  if (!touch_scroll_in_progress_)
    return;
  touch_scroll_in_progress_ = false;
  if (send_scroll_end_event)
    Send(CreateGesture(ET_GESTURE_SCROLL_END, event));
}

void GestureProvider::UpdateDoubleTapDetectionSupport() {
  if (IsDoubleTapInProgress())
    return;

  const bool supports_double_tap = IsDoubleTapSupported();
  gesture_listener_->SetDoubleTapEnabled(supports_double_tap);
  scale_gesture_listener_->SetDoubleTapEnabled(supports_double_tap);
}

void GestureProvider::OnTouchEventHandlingBegin(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::ACTION_DOWN:
      current_down_event_ = event.Clone();
      touch_scroll_in_progress_ = false;
      needs_show_press_event_ = false;
      current_longpress_time_ = base::TimeTicks();
      SendTapCancelIfNecessary(event);
      if (gesture_begin_end_types_enabled_)
        Send(CreateGesture(ET_GESTURE_BEGIN, event));
      break;
    case MotionEvent::ACTION_POINTER_DOWN:
      if (gesture_begin_end_types_enabled_)
        Send(CreateGesture(ET_GESTURE_BEGIN, event));
      break;
    case MotionEvent::ACTION_POINTER_UP:
    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_CANCEL:
    case MotionEvent::ACTION_MOVE:
      break;
  }
}

void GestureProvider::OnTouchEventHandlingEnd(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_CANCEL:
      // Note: This call will have no effect if a fling was just generated, as
      // |Fling()| will have already signalled an end to touch-scrolling.
      EndTouchScrollIfNecessary(event, true);

      // We shouldn't necessarily cancel a tap on ACTION_UP, as the double-tap
      // timeout may yet trigger a SINGLE_TAP.
      if (event.GetAction() == MotionEvent::ACTION_CANCEL)
        SendTapCancelIfNecessary(event);

      UpdateDoubleTapDetectionSupport();

      if (gesture_begin_end_types_enabled_)
        Send(CreateGesture(ET_GESTURE_END, event));

      current_down_event_.reset();
      break;
    case MotionEvent::ACTION_POINTER_UP:
      if (gesture_begin_end_types_enabled_)
        Send(CreateGesture(ET_GESTURE_END, event));
      break;
    case MotionEvent::ACTION_DOWN:
    case MotionEvent::ACTION_POINTER_DOWN:
    case MotionEvent::ACTION_MOVE:
      break;
  }
}

}  //  namespace ui
