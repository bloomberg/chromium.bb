// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/snap_scroll_controller.h"

#include <cmath>

#include "ui/events/gesture_detection/motion_event.h"

namespace ui {
namespace {
const int kSnapBound = 16;
}  // namespace

SnapScrollController::Config::Config()
    : screen_width_pixels(1), screen_height_pixels(1), device_scale_factor(1) {}

SnapScrollController::Config::~Config() {}

SnapScrollController::SnapScrollController(const Config& config)
    : channel_distance_(CalculateChannelDistance(config)),
      snap_scroll_mode_(SNAP_NONE),
      first_touch_x_(-1),
      first_touch_y_(-1),
      distance_x_(0),
      distance_y_(0) {}

SnapScrollController::~SnapScrollController() {}

void SnapScrollController::UpdateSnapScrollMode(float distance_x,
                                                float distance_y) {
  if (snap_scroll_mode_ == SNAP_HORIZ || snap_scroll_mode_ == SNAP_VERT) {
    distance_x_ += std::abs(distance_x);
    distance_y_ += std::abs(distance_y);
    if (snap_scroll_mode_ == SNAP_HORIZ) {
      if (distance_y_ > channel_distance_) {
        snap_scroll_mode_ = SNAP_NONE;
      } else if (distance_x_ > channel_distance_) {
        distance_x_ = 0;
        distance_y_ = 0;
      }
    } else {
      if (distance_x_ > channel_distance_) {
        snap_scroll_mode_ = SNAP_NONE;
      } else if (distance_y_ > channel_distance_) {
        distance_x_ = 0;
        distance_y_ = 0;
      }
    }
  }
}

void SnapScrollController::SetSnapScrollingMode(
    const MotionEvent& event,
    bool is_scale_gesture_detection_in_progress) {
  switch (event.GetAction()) {
    case MotionEvent::ACTION_DOWN:
      snap_scroll_mode_ = SNAP_NONE;
      first_touch_x_ = event.GetX();
      first_touch_y_ = event.GetY();
      break;
    // Set scrolling mode to SNAP_X if scroll towards x-axis exceeds kSnapBound
    // and movement towards y-axis is trivial.
    // Set scrolling mode to SNAP_Y if scroll towards y-axis exceeds kSnapBound
    // and movement towards x-axis is trivial.
    // Scrolling mode will remain in SNAP_NONE for other conditions.
    case MotionEvent::ACTION_MOVE:
      if (!is_scale_gesture_detection_in_progress &&
          snap_scroll_mode_ == SNAP_NONE) {
        int x_diff = static_cast<int>(std::abs(event.GetX() - first_touch_x_));
        int y_diff = static_cast<int>(std::abs(event.GetY() - first_touch_y_));
        if (x_diff > kSnapBound && y_diff < kSnapBound) {
          snap_scroll_mode_ = SNAP_HORIZ;
        } else if (x_diff < kSnapBound && y_diff > kSnapBound) {
          snap_scroll_mode_ = SNAP_VERT;
        }
      }
      break;
    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_CANCEL:
      first_touch_x_ = -1;
      first_touch_y_ = -1;
      distance_x_ = 0;
      distance_y_ = 0;
      break;
    default:
      break;
  }
}

// static
float SnapScrollController::CalculateChannelDistance(const Config& config) {
  float channel_distance = 16.f;

  const float screen_size = std::abs(
      hypot((float)config.screen_width_pixels / config.device_scale_factor,
            (float)config.screen_height_pixels / config.device_scale_factor));
  if (screen_size < 3.f) {
    channel_distance = 16.f;
  } else if (screen_size < 5.f) {
    channel_distance = 22.f;
  } else if (screen_size < 7.f) {
    channel_distance = 28.f;
  } else {
    channel_distance = 34.f;
  }
  channel_distance = channel_distance * config.device_scale_factor;
  if (channel_distance < 16.f)
    channel_distance = 16.f;

  return channel_distance;
}

}  // namespace ui
