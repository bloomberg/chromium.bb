// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/stl_util.h"

namespace cc {

std::unique_ptr<EventMetrics> EventMetrics::Create(
    ui::EventType type,
    base::TimeTicks time_stamp,
    base::Optional<ui::ScrollInputType> scroll_input_type) {
  switch (type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
    case ui::ET_GESTURE_SCROLL_END:
      return base::WrapUnique(
          new EventMetrics(type, time_stamp, scroll_input_type));
    default:
      return nullptr;
  }
}

EventMetrics::EventMetrics(
    ui::EventType type,
    base::TimeTicks time_stamp,
    base::Optional<ui::ScrollInputType> scroll_input_type)
    : type_(type),
      time_stamp_(time_stamp),
      scroll_input_type_(scroll_input_type) {}

EventMetrics::EventMetrics(const EventMetrics&) = default;
EventMetrics& EventMetrics::operator=(const EventMetrics&) = default;

const char* EventMetrics::GetTypeName() const {
  switch (type_) {
    case ui::ET_MOUSE_PRESSED:
      return "MousePressed";
    case ui::ET_MOUSE_RELEASED:
      return "MouseReleased";
    case ui::ET_MOUSEWHEEL:
      return "MouseWheel";
    case ui::ET_KEY_PRESSED:
      // TODO(crbug/1071645): Currently, all ET_KEY_PRESSED events are reported
      // under EventLatency.KeyPressed histogram. This includes both key-down
      // and key-char events. Consider reporting them separately.
      return "KeyPressed";
    case ui::ET_KEY_RELEASED:
      return "KeyReleased";
    case ui::ET_TOUCH_PRESSED:
      return "TouchPressed";
    case ui::ET_TOUCH_RELEASED:
      return "TouchReleased";
    case ui::ET_TOUCH_MOVED:
      return "TouchMoved";
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return "GestureScrollBegin";
    case ui::ET_GESTURE_SCROLL_UPDATE:
      return "GestureScrollUpdate";
    case ui::ET_GESTURE_SCROLL_END:
      return "GestureScrollEnd";
    default:
      NOTREACHED();
      return nullptr;
  }
}

const char* EventMetrics::GetScrollTypeName() const {
  DCHECK(scroll_input_type_) << "Event is not a scroll event";

  switch (*scroll_input_type_) {
    case ui::ScrollInputType::kTouchscreen:
      return "Touchscreen";
    case ui::ScrollInputType::kWheel:
      return "Wheel";
    case ui::ScrollInputType::kAutoscroll:
      return "Autoscroll";
    case ui::ScrollInputType::kScrollbar:
      return "Scrollbar";
  }
}

bool EventMetrics::operator==(const EventMetrics& other) const {
  return std::tie(type_, time_stamp_, scroll_input_type_) ==
         std::tie(other.type_, other.time_stamp_, other.scroll_input_type_);
}

// EventMetricsSet
EventMetricsSet::EventMetricsSet() = default;
EventMetricsSet::~EventMetricsSet() = default;
EventMetricsSet::EventMetricsSet(
    std::vector<EventMetrics> main_thread_event_metrics,
    std::vector<EventMetrics> impl_thread_event_metrics)
    : main_event_metrics(std::move(main_thread_event_metrics)),
      impl_event_metrics(std::move(impl_thread_event_metrics)) {}
EventMetricsSet::EventMetricsSet(EventMetricsSet&& other) = default;
EventMetricsSet& EventMetricsSet::operator=(EventMetricsSet&& other) = default;

}  // namespace cc
