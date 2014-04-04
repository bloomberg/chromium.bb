// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/scale_gesture_detector.h"
#include "ui/events/gesture_detection/snap_scroll_controller.h"

namespace ui {

struct GestureEventData;

class GESTURE_DETECTION_EXPORT GestureProviderClient {
 public:
  virtual ~GestureProviderClient() {}
  virtual void OnGestureEvent(const GestureEventData& gesture) = 0;
};

// Given a stream of |MotionEvent|'s, provides gesture detection and gesture
// event dispatch.
class GESTURE_DETECTION_EXPORT GestureProvider {
 public:
  struct GESTURE_DETECTION_EXPORT Config {
    Config();
    ~Config();
    GestureDetector::Config gesture_detector_config;
    ScaleGestureDetector::Config scale_gesture_detector_config;
    SnapScrollController::Config snap_scroll_controller_config;
    bool disable_click_delay;
  };

  GestureProvider(const Config& config, GestureProviderClient* client);
  ~GestureProvider();

  // Handle the incoming MotionEvent, returning false if the event could not
  // be handled.
  bool OnTouchEvent(const MotionEvent& event);

  // Resets all gesture detectors; called on DidStartLoading().
  void ResetGestureDetectors();

  // Update whether multi-touch gestures are supported.
  void SetMultiTouchSupportEnabled(bool enabled);

  // Update whether double-tap gestures are supported. This allows
  // double-tap gesture suppression independent of whether or not the page's
  // viewport and scale would normally prevent double-tap.
  // Note: This should not be called while a double-tap gesture is in progress.
  void SetDoubleTapSupportForPlatformEnabled(bool enabled);

  // Update whether double-tap gesture detection should be suppressed due to
  // the viewport or scale of the current page. Suppressing double-tap gesture
  // detection allows for rapid and responsive single-tap gestures.
  void SetDoubleTapSupportForPageEnabled(bool enabled);

  // Whether a scroll gesture is in-progress.
  bool IsScrollInProgress() const;

  // Whether a pinch gesture is in-progress (i.e. a pinch update has been
  // forwarded and detection is still active).
  bool IsPinchInProgress() const;

  // Whether a double-tap gesture is in-progress (either double-tap or
  // double-tap drag zoom).
  bool IsDoubleTapInProgress() const;

  // Whether double-tap gesture detection is supported.
  bool IsDoubleTapSupported() const;

  // Whether the tap gesture delay is explicitly disabled (independent of
  // whether double-tap is supported), see |Config.disable_click_delay|.
  bool IsClickDelayDisabled() const;

  // May be NULL if there is no currently active touch sequence.
  const ui::MotionEvent* current_down_event() const {
    return current_down_event_.get();
  }

 private:
  void InitGestureDetectors(const Config& config);

  bool CanHandle(const MotionEvent& event) const;

  void Fling(const MotionEvent& e, float velocity_x, float velocity_y);
  void Send(const GestureEventData& gesture);
  void SendTapCancelIfNecessary(const MotionEvent& event);
  void SendTapCancelIfNecessary(const GestureEventData& event);
  bool SendLongTapIfNecessary(const MotionEvent& event);
  void EndTouchScrollIfNecessary(const MotionEvent& event,
                                 bool send_scroll_end_event);
  void UpdateDoubleTapDetectionSupport();

  GestureProviderClient* const client_;

  class GestureListenerImpl;
  friend class GestureListenerImpl;
  scoped_ptr<GestureListenerImpl> gesture_listener_;

  class ScaleGestureListenerImpl;
  friend class ScaleGestureListenerImpl;
  scoped_ptr<ScaleGestureListenerImpl> scale_gesture_listener_;

  scoped_ptr<MotionEvent> current_down_event_;

  // Whether a GESTURE_SHOW_PRESS was sent for the current touch sequence.
  // Sending a GESTURE_TAP event will forward a GESTURE_SHOW_PRESS if one has
  // not yet been sent.
  bool needs_show_press_event_;

  // Whether a sent GESTURE_TAP_DOWN event has yet to be accompanied by a
  // corresponding GESTURE_TAP, GESTURE_TAP_CANCEL or GESTURE_DOUBLE_TAP.
  bool needs_tap_ending_event_;

  // Whether the respective {SCROLL,PINCH}_BEGIN gestures have been terminated
  // with a {SCROLL,PINCH}_END.
  bool touch_scroll_in_progress_;
  bool pinch_in_progress_;

  // Whether double-tap gesture detection is currently supported.
  bool double_tap_support_for_page_;
  bool double_tap_support_for_platform_;

  // Keeps track of the current GESTURE_LONG_PRESS event. If a context menu is
  // opened after a GESTURE_LONG_PRESS, this is used to insert a
  // GESTURE_TAP_CANCEL for removing any ::active styling.
  base::TimeTicks current_longpress_time_;
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_
