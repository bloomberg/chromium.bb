// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_provider.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
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

GestureEventData CreateGesture(GestureEventType type,
                               base::TimeTicks time,
                               float x,
                               float y,
                               const GestureEventData::Details& details) {
  return GestureEventData(type, time, x, y, details);
}

GestureEventData CreateGesture(GestureEventType type,
                               base::TimeTicks time,
                               float x,
                               float y) {
  return CreateGesture(type, time, x, y, GestureEventData::Details());
}

GestureEventData CreateGesture(GestureEventType type,
                               const MotionEvent& event,
                               const GestureEventData::Details& details) {
  return CreateGesture(
      type, event.GetEventTime(), event.GetX(), event.GetY(), details);
}

GestureEventData CreateGesture(GestureEventType type,
                               const MotionEvent& event) {
  return CreateGesture(type, event, GestureEventData::Details());
}

float Round(float f) {
  return (f > 0.f) ? std::floor(f + 0.5f) : std::ceil(f - 0.5f);
}

}  // namespace

// GestureProvider:::Config

GestureProvider::Config::Config() : disable_click_delay(false) {}

GestureProvider::Config::~Config() {}

// GestureProvider::ScaleGestureListener

class GestureProvider::ScaleGestureListenerImpl
    : public ScaleGestureDetector::ScaleGestureListener {
 public:
  ScaleGestureListenerImpl(const ScaleGestureDetector::Config& config,
                           GestureProvider* provider)
      : scale_gesture_detector_(config, this),
        provider_(provider),
        ignore_detector_events_(false),
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
  virtual bool OnScaleBegin(const ScaleGestureDetector& detector) OVERRIDE {
    if (ignore_detector_events_)
      return false;
    pinch_event_sent_ = false;
    return true;
  }

  virtual void OnScaleEnd(const ScaleGestureDetector& detector) OVERRIDE {
    if (!pinch_event_sent_)
      return;
    provider_->Send(
        CreateGesture(GESTURE_PINCH_END, detector.GetEventTime(), 0, 0));
    pinch_event_sent_ = false;
  }

  virtual bool OnScale(const ScaleGestureDetector& detector) OVERRIDE {
    if (ignore_detector_events_)
      return false;
    if (!pinch_event_sent_) {
      pinch_event_sent_ = true;
      provider_->Send(CreateGesture(GESTURE_PINCH_BEGIN,
                                    detector.GetEventTime(),
                                    detector.GetFocusX(),
                                    detector.GetFocusY()));
    }
    GestureEventData::Details pinch_details;
    pinch_details.pinch_update.scale = detector.GetScaleFactor();
    provider_->Send(CreateGesture(GESTURE_PINCH_UPDATE,
                                  detector.GetEventTime(),
                                  detector.GetFocusX(),
                                  detector.GetFocusY(),
                                  pinch_details));
    return true;
  }

  bool IsScaleGestureDetectionInProgress() const {
    return !ignore_detector_events_ && scale_gesture_detector_.IsInProgress();
  }

  void set_ignore_detector_events(bool value) {
    // Note that returning false from OnScaleBegin / OnScale makes the
    // gesture detector not to emit further scaling notifications
    // related to this gesture. Thus, if detector events are enabled in
    // the middle of the gesture, we don't need to do anything.
    ignore_detector_events_ = value;
  }

 private:
  ScaleGestureDetector scale_gesture_detector_;

  GestureProvider* const provider_;

  // Completely silence scaling events. Used in WebView when zoom support
  // is turned off.
  bool ignore_detector_events_;

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
        px_to_dp_(1.0f / snap_scroll_controller_config.device_scale_factor),
        disable_click_delay_(disable_click_delay),
        scaled_touch_slop_(gesture_detector_config.scaled_touch_slop),
        scaled_touch_slop_square_(scaled_touch_slop_ * scaled_touch_slop_),
        double_tap_timeout_(gesture_detector_config.double_tap_timeout),
        ignore_single_tap_(false),
        seen_first_scroll_event_(false),
        double_tap_mode_(DOUBLE_TAP_MODE_NONE),
        double_tap_y_(0),
        double_tap_support_enabled_(true),
        double_tap_drag_zoom_anchor_x_(0),
        double_tap_drag_zoom_anchor_y_(0),
        last_raw_x_(0),
        last_raw_y_(0),
        accumulated_scroll_error_x_(0),
        accumulated_scroll_error_y_(0) {
    UpdateDoubleTapListener();
  }

  virtual ~GestureListenerImpl() {}

  bool OnTouchEvent(const MotionEvent& e,
                    bool is_scale_gesture_detection_in_progress) {
    snap_scroll_controller_.SetSnapScrollingMode(
        e, is_scale_gesture_detection_in_progress);

    if (is_scale_gesture_detection_in_progress)
      SetIgnoreSingleTap(true);

    if (e.GetAction() == MotionEvent::ACTION_POINTER_DOWN ||
        e.GetAction() == MotionEvent::ACTION_CANCEL) {
      EndDoubleTapDragIfNecessary(e);
    } else if (e.GetAction() == MotionEvent::ACTION_DOWN) {
      gesture_detector_.set_is_longpress_enabled(true);
    }

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

    GestureEventData::Details tap_details;
    tap_details.tap.width = tap_details.tap.height = e.GetTouchMajor();
    provider_->Send(CreateGesture(GESTURE_TAP_DOWN, e, tap_details));

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
      GestureEventData::Details scroll_details;
      scroll_details.scroll_begin.delta_x_hint = -raw_distance_x;
      scroll_details.scroll_begin.delta_y_hint = -raw_distance_y;
      provider_->Send(CreateGesture(GESTURE_SCROLL_BEGIN,
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
      GestureEventData::Details scroll_details;
      scroll_details.scroll_update.delta_x = -dx;
      scroll_details.scroll_update.delta_y = -dy;
      provider_->Send(CreateGesture(GESTURE_SCROLL_UPDATE, e2, scroll_details));
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

    provider_->Fling(
        e2.GetEventTime(), e1.GetX(), e1.GetY(), velocity_x, velocity_y);
    return true;
  }

  virtual void OnShowPress(const MotionEvent& e) OVERRIDE {
    GestureEventData::Details show_press_details;
    // TODO(jdduke): Expose minor axis length and rotation in |MotionEvent|.
    show_press_details.show_press.width = e.GetTouchMajor();
    show_press_details.show_press.height = show_press_details.show_press.width;
    provider_->Send(CreateGesture(GESTURE_SHOW_PRESS, e, show_press_details));
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
      } else if (IsDoubleTapDisabled() || disable_click_delay_) {
        // If double-tap has been disabled, there is no need to wait
        // for the double-tap timeout.
        return OnSingleTapConfirmed(e);
      } else {
        // Notify Blink about this tapUp event anyway,
        // when none of the above conditions applied.
        provider_->Send(CreateGesture(GESTURE_TAP_UNCONFIRMED, e));
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

    GestureEventData::Details tap_details;
    tap_details.tap.tap_count = 1;
    tap_details.tap.width = tap_details.tap.height = e.GetTouchMajor();
    provider_->Send(CreateGesture(GESTURE_TAP, e, tap_details));
    return true;
  }

  virtual bool OnDoubleTap(const MotionEvent& e) OVERRIDE { return false; }

  virtual bool OnDoubleTapEvent(const MotionEvent& e) OVERRIDE {
    switch (e.GetAction()) {
      case MotionEvent::ACTION_DOWN:
        // Note that this will be called before the corresponding |onDown()|
        // of the same ACTION_DOWN event.  Thus, the preceding TAP_DOWN
        // should be cancelled prior to sending a new one (in |onDown()|).
        double_tap_drag_zoom_anchor_x_ = e.GetX();
        double_tap_drag_zoom_anchor_y_ = e.GetY();
        double_tap_mode_ = DOUBLE_TAP_MODE_DRAG_DETECTION_IN_PROGRESS;
        // If a long-press fires during a double-tap, the GestureDetector
        // will stop feeding MotionEvents to |onDoubleTapEvent()|,
        // preventing double-tap drag zoom. Long press detection will be
        // re-enabled on the next ACTION_DOWN.
        gesture_detector_.set_is_longpress_enabled(false);
        break;
      case MotionEvent::ACTION_MOVE:
        if (double_tap_mode_ == DOUBLE_TAP_MODE_DRAG_DETECTION_IN_PROGRESS) {
          float distance_x = double_tap_drag_zoom_anchor_x_ - e.GetX();
          float distance_y = double_tap_drag_zoom_anchor_y_ - e.GetY();

          // Begin double-tap drag zoom mode if the move distance is
          // further than the threshold.
          if (IsDistanceGreaterThanTouchSlop(distance_x, distance_y)) {
            GestureEventData::Details scroll_details;
            scroll_details.scroll_begin.delta_x_hint = -distance_x;
            scroll_details.scroll_begin.delta_y_hint = -distance_y;
            provider_->Send(
                CreateGesture(GESTURE_SCROLL_BEGIN, e, scroll_details));
            provider_->Send(
                CreateGesture(GESTURE_PINCH_BEGIN,
                              e.GetEventTime(),
                              Round(double_tap_drag_zoom_anchor_x_),
                              Round(double_tap_drag_zoom_anchor_y_)));
            double_tap_mode_ = DOUBLE_TAP_MODE_DRAG_ZOOM;
          }
        } else if (double_tap_mode_ == DOUBLE_TAP_MODE_DRAG_ZOOM) {
          provider_->Send(CreateGesture(GESTURE_SCROLL_UPDATE, e));

          float dy = double_tap_y_ - e.GetY();
          GestureEventData::Details pinch_details;
          pinch_details.pinch_update.scale =
              std::pow(dy > 0 ? 1.0f - kDoubleTapDragZoomSpeed
                              : 1.0f + kDoubleTapDragZoomSpeed,
                       std::abs(dy * px_to_dp_));
          provider_->Send(CreateGesture(GESTURE_PINCH_UPDATE,
                                        e.GetEventTime(),
                                        Round(double_tap_drag_zoom_anchor_x_),
                                        Round(double_tap_drag_zoom_anchor_y_),
                                        pinch_details));
        }
        break;
      case MotionEvent::ACTION_UP:
        if (double_tap_mode_ != DOUBLE_TAP_MODE_DRAG_ZOOM) {
          // Normal double-tap gesture.
          provider_->Send(CreateGesture(GESTURE_DOUBLE_TAP, e));
        }
        EndDoubleTapDragIfNecessary(e);
        break;
      case MotionEvent::ACTION_CANCEL:
        EndDoubleTapDragIfNecessary(e);
        break;
      default:
        NOTREACHED() << "Invalid double-tap event.";
        break;
    }
    double_tap_y_ = e.GetY();
    return true;
  }

  virtual bool OnLongPress(const MotionEvent& e) OVERRIDE {
    DCHECK(!IsDoubleTapInProgress());
    SetIgnoreSingleTap(true);

    GestureEventData::Details long_press_details;
    long_press_details.long_press.width = e.GetTouchMajor();
    long_press_details.long_press.height = long_press_details.long_press.width;
    provider_->Send(CreateGesture(GESTURE_LONG_PRESS, e, long_press_details));

    // Returning true puts the GestureDetector in "longpress" mode, disabling
    // further scrolling.  This is undesirable, as it is quite common for a
    // longpress gesture to fire on content that won't trigger a context menu.
    return false;
  }

  void SetDoubleTapSupportForPlatformEnabled(bool enabled) {
    DCHECK(!IsDoubleTapInProgress());
    DoubleTapMode double_tap_mode =
        enabled ? DOUBLE_TAP_MODE_NONE : DOUBLE_TAP_MODE_DISABLED;
    if (double_tap_mode_ == double_tap_mode)
      return;
    double_tap_mode_ = double_tap_mode;
    UpdateDoubleTapListener();
  }

  void SetDoubleTapSupportForPageEnabled(bool enabled) {
    if (double_tap_support_enabled_ == enabled)
      return;
    double_tap_support_enabled_ = enabled;
    UpdateDoubleTapListener();
  }

  bool IsDoubleTapDisabled() const {
    return double_tap_mode_ == DOUBLE_TAP_MODE_DISABLED ||
           !double_tap_support_enabled_;
  }

  bool IsClickDelayDisabled() const { return disable_click_delay_; }

  bool IsDoubleTapInProgress() const {
    return double_tap_mode_ != DOUBLE_TAP_MODE_DISABLED &&
           double_tap_mode_ != DOUBLE_TAP_MODE_NONE;
  }

 private:
  enum DoubleTapMode {
    DOUBLE_TAP_MODE_NONE,
    DOUBLE_TAP_MODE_DRAG_DETECTION_IN_PROGRESS,
    DOUBLE_TAP_MODE_DRAG_ZOOM,
    DOUBLE_TAP_MODE_DISABLED
  };

  bool IsPointOutsideCurrentSlopRegion(float x, float y) const {
    return IsDistanceGreaterThanTouchSlop(last_raw_x_ - x, last_raw_y_ - y);
  }

  bool IsDistanceGreaterThanTouchSlop(float distance_x,
                                      float distance_y) const {
    return distance_x * distance_x + distance_y * distance_y >
           scaled_touch_slop_square_;
  }

  void SetIgnoreSingleTap(bool value) { ignore_single_tap_ = value; }

  void EndDoubleTapDragIfNecessary(const MotionEvent& event) {
    if (!IsDoubleTapInProgress())
      return;
    if (double_tap_mode_ == DOUBLE_TAP_MODE_DRAG_ZOOM) {
      provider_->Send(CreateGesture(GESTURE_PINCH_END, event));
      provider_->Send(CreateGesture(GESTURE_SCROLL_END, event));
    }
    double_tap_mode_ = DOUBLE_TAP_MODE_NONE;
    UpdateDoubleTapListener();
  }

  void UpdateDoubleTapListener() {
    if (IsDoubleTapDisabled()) {
      // Defer nulling the DoubleTapListener until the double-tap gesture is
      // complete.
      if (IsDoubleTapInProgress())
        return;
      gesture_detector_.set_doubletap_listener(NULL);
    } else {
      gesture_detector_.set_doubletap_listener(this);
    }
  }

  GestureDetector gesture_detector_;
  SnapScrollController snap_scroll_controller_;

  GestureProvider* const provider_;

  const float px_to_dp_;

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

  // Indicate current double-tap mode state.
  int double_tap_mode_;

  // On double-tap this will store the y coordinates of the touch.
  float double_tap_y_;

  // The page's viewport and scale sometimes allow us to disable double-tap
  // gesture detection,
  // according to the logic in ContentViewCore.onRenderCoordinatesUpdated().
  bool double_tap_support_enabled_;

  // x, y coordinates for an Anchor on double-tap drag zoom.
  float double_tap_drag_zoom_anchor_x_;
  float double_tap_drag_zoom_anchor_y_;

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
      pinch_in_progress_(false) {
  DCHECK(client);
  InitGestureDetectors(config);
}

GestureProvider::~GestureProvider() {}

bool GestureProvider::OnTouchEvent(const MotionEvent& event) {
  TRACE_EVENT1("input", "GestureProvider::OnTouchEvent",
               "action", GetMotionEventActionName(event.GetAction()));
  if (!CanHandle(event))
    return false;

  const bool was_touch_scrolling_ = touch_scroll_in_progress_;
  const bool in_scale_gesture =
      scale_gesture_listener_->IsScaleGestureDetectionInProgress();

  if (event.GetAction() == MotionEvent::ACTION_DOWN) {
    current_down_event_ = event.Clone();
    touch_scroll_in_progress_ = false;
    needs_show_press_event_ = true;
    current_longpress_time_ = base::TimeTicks();
    SendTapCancelIfNecessary(event);
  }

  bool handled = gesture_listener_->OnTouchEvent(event, in_scale_gesture);
  handled |= scale_gesture_listener_->OnTouchEvent(event);

  if (event.GetAction() == MotionEvent::ACTION_UP ||
      event.GetAction() == MotionEvent::ACTION_CANCEL) {
    // "Last finger raised" could be an end to movement, but it should
    // only terminate scrolling if the event did not cause a fling.
    if (was_touch_scrolling_ && !handled)
      EndTouchScrollIfNecessary(event.GetEventTime(), true);

    // We shouldn't necessarily cancel a tap on ACTION_UP, as the double-tap
    // timeout may yet trigger a SINGLE_TAP.
    if (event.GetAction() == MotionEvent::ACTION_CANCEL)
      SendTapCancelIfNecessary(event);

    current_down_event_.reset();
  }

  return true;
}

void GestureProvider::ResetGestureDetectors() {
  if (!current_down_event_)
    return;
  scoped_ptr<MotionEvent> cancel_event = current_down_event_->Cancel();
  gesture_listener_->OnTouchEvent(*cancel_event, false);
  scale_gesture_listener_->OnTouchEvent(*cancel_event);
}

void GestureProvider::CancelActiveTouchSequence() {
  if (!current_down_event_)
    return;
  OnTouchEvent(*current_down_event_->Cancel());
  current_down_event_.reset();
}

void GestureProvider::SetMultiTouchSupportEnabled(bool enabled) {
  scale_gesture_listener_->set_ignore_detector_events(!enabled);
}

void GestureProvider::SetDoubleTapSupportForPlatformEnabled(bool enabled) {
  gesture_listener_->SetDoubleTapSupportForPlatformEnabled(enabled);
}

void GestureProvider::SetDoubleTapSupportForPageEnabled(bool enabled) {
  gesture_listener_->SetDoubleTapSupportForPageEnabled(enabled);
}

bool GestureProvider::IsScrollInProgress() const {
  // TODO(wangxianzhu): Also return true when fling is active once the UI knows
  // exactly when the fling ends.
  return touch_scroll_in_progress_;
}

bool GestureProvider::IsPinchInProgress() const { return pinch_in_progress_; }

bool GestureProvider::IsDoubleTapInProgress() const {
  return gesture_listener_->IsDoubleTapInProgress();
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

  scale_gesture_listener_.reset(
      new ScaleGestureListenerImpl(config.scale_gesture_detector_config, this));
}

bool GestureProvider::CanHandle(const MotionEvent& event) const {
  return event.GetAction() == MotionEvent::ACTION_DOWN || current_down_event_;
}

void GestureProvider::Fling(base::TimeTicks time,
                            float x,
                            float y,
                            float velocity_x,
                            float velocity_y) {
  if (!velocity_x && !velocity_y) {
    EndTouchScrollIfNecessary(time, true);
    return;
  }

  if (!touch_scroll_in_progress_) {
    // The native side needs a GESTURE_SCROLL_BEGIN before GESTURE_FLING_START
    // to send the fling to the correct target. Send if it has not sent.
    // The distance traveled in one second is a reasonable scroll start hint.
    GestureEventData::Details scroll_details;
    scroll_details.scroll_begin.delta_x_hint = velocity_x;
    scroll_details.scroll_begin.delta_y_hint = velocity_y;
    Send(CreateGesture(GESTURE_SCROLL_BEGIN, time, x, y, scroll_details));
  }
  EndTouchScrollIfNecessary(time, false);

  GestureEventData::Details fling_details;
  fling_details.fling_start.velocity_x = velocity_x;
  fling_details.fling_start.velocity_y = velocity_y;
  Send(CreateGesture(GESTURE_FLING_START, time, x, y, fling_details));
}

void GestureProvider::Send(const GestureEventData& gesture) {
  DCHECK(!gesture.time.is_null());
  // The only valid events that should be sent without an active touch sequence
  // are SHOW_PRESS and TAP, potentially triggered by the double-tap
  // delay timing out.
  DCHECK(current_down_event_ || gesture.type == GESTURE_TAP ||
         gesture.type == GESTURE_SHOW_PRESS);

  switch (gesture.type) {
    case GESTURE_TAP_DOWN:
      needs_tap_ending_event_ = true;
      break;
    case GESTURE_TAP_UNCONFIRMED:
      needs_show_press_event_ = false;
      break;
    case GESTURE_TAP:
      if (needs_show_press_event_)
        Send(CreateGesture(
            GESTURE_SHOW_PRESS, gesture.time, gesture.x, gesture.y));
      needs_tap_ending_event_ = false;
      break;
    case GESTURE_DOUBLE_TAP:
      needs_tap_ending_event_ = false;
      break;
    case GESTURE_TAP_CANCEL:
      if (!needs_tap_ending_event_)
        return;
      needs_tap_ending_event_ = false;
      break;
    case GESTURE_SHOW_PRESS:
      needs_show_press_event_ = false;
      break;
    case GESTURE_LONG_PRESS:
      DCHECK(!scale_gesture_listener_->IsScaleGestureDetectionInProgress());
      current_longpress_time_ = gesture.time;
      break;
    case GESTURE_LONG_TAP:
      needs_tap_ending_event_ = false;
      current_longpress_time_ = base::TimeTicks();
      break;
    case GESTURE_SCROLL_BEGIN:
      touch_scroll_in_progress_ = true;
      SendTapCancelIfNecessary(*current_down_event_);
      break;
    case GESTURE_SCROLL_END:
      touch_scroll_in_progress_ = false;
      break;
    case GESTURE_PINCH_BEGIN:
      pinch_in_progress_ = true;
      break;
    case GESTURE_PINCH_END:
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
  Send(CreateGesture(GESTURE_TAP_CANCEL, event));
}

bool GestureProvider::SendLongTapIfNecessary(const MotionEvent& event) {
  if (event.GetAction() == MotionEvent::ACTION_UP &&
      !current_longpress_time_.is_null() &&
      !scale_gesture_listener_->IsScaleGestureDetectionInProgress()) {
    SendTapCancelIfNecessary(event);
    GestureEventData::Details long_tap_details;
    long_tap_details.long_press.width = event.GetTouchMajor();
    long_tap_details.long_press.height = long_tap_details.long_press.width;
    Send(CreateGesture(GESTURE_LONG_TAP, event, long_tap_details));
    return true;
  }
  return false;
}

void GestureProvider::EndTouchScrollIfNecessary(base::TimeTicks time,
                                                bool send_scroll_end_event) {
  if (!touch_scroll_in_progress_)
    return;
  touch_scroll_in_progress_ = false;
  if (send_scroll_end_event)
    Send(CreateGesture(GESTURE_SCROLL_END, time, 0, 0));
}

}  //  namespace ui
