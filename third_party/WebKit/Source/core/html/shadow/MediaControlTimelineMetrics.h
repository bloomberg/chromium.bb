// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlTimelineMetrics_h
#define MediaControlTimelineMetrics_h

#include "platform/Histogram.h"
#include "platform/wtf/Time.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

// Helpers for tracking and reporting media control timeline metrics to UMA.
class MediaControlTimelineMetrics {
 public:
  // Start tracking a pointer gesture. |fromThumb| indicates whether the user
  // started dragging from the thumb, as opposed to pressing down their pointer
  // on some other part of the timeline track (causing time to jump).
  void startGesture(bool fromThumb);
  // Finish tracking and report a pointer gesture.
  void recordEndGesture(int timelineWidth, double mediaDurationSeconds);

  // Start tracking a keydown. Ok to call multiple times if key repeats.
  void startKey();
  // Finish tracking and report a keyup. Call only once even if key repeats.
  void recordEndKey(int timelineWidth, int keyCode);

  // Track an incremental input event caused by the current pointer gesture or
  // pressed key. Each sequence of calls to this should usually be sandwiched by
  // startGesture/Key and recordEndGesture/Key.
  void onInput(double fromSeconds, double toSeconds);

  // Reports width to UMA the first time the media starts playing.
  void recordPlaying(WebScreenOrientationType,
                     bool isFullscreen,
                     int timelineWidth);

 private:
  enum class State {
    // No active gesture. Progresses to kKeyDown on |startKey|, or
    // kGestureFromThumb/kGestureFromElsewhere on |startGesture|.
    kInactive,

    // Pointer down on thumb. Progresses to kDragFromThumb in |onInput|.
    kGestureFromThumb,
    // Thumb is being dragged (drag started from thumb).
    kDragFromThumb,

    // Pointer down on track. Progresses to kClick in |onInput|.
    kGestureFromElsewhere,
    // Pointer down followed by input. Assumed to be a click, unless additional
    // |onInput| are received - if so progresses to kDragFromElsewhere.
    kClick,
    // Thumb is being dragged (drag started from track).
    kDragFromElsewhere,

    // A key is currently pressed down.
    kKeyDown
  };

  bool m_hasNeverBeenPlaying = true;

  State m_state = State::kInactive;

  // The following are only valid during a pointer gesture.
  TimeTicks m_dragStartTimeTicks;
  float m_dragDeltaMediaSeconds = 0;
  float m_dragSumAbsDeltaMediaSeconds = 0;
};

}  // namespace blink

#endif  // MediaControlTimelineMetrics_h
