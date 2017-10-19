// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/AnimationPlaybackEvent.h"

namespace blink {

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomicString& type,
                                               double current_time,
                                               double timeline_time)
    : Event(type, false, false),
      current_time_(current_time),
      timeline_time_(timeline_time) {}

AnimationPlaybackEvent::AnimationPlaybackEvent(
    const AtomicString& type,
    const AnimationPlaybackEventInit& initializer)
    : Event(type, initializer), current_time_(0.0), timeline_time_(0.0) {
  if (initializer.hasCurrentTime())
    current_time_ = initializer.currentTime();
  if (initializer.hasTimelineTime())
    timeline_time_ = initializer.timelineTime();
}

AnimationPlaybackEvent::~AnimationPlaybackEvent() {}

double AnimationPlaybackEvent::currentTime(bool& is_null) const {
  double result = currentTime();
  is_null = std::isnan(result);
  return result;
}

double AnimationPlaybackEvent::currentTime() const {
  return current_time_;
}

double AnimationPlaybackEvent::timelineTime(bool& is_null) const {
  double result = timelineTime();
  is_null = std::isnan(result);
  return result;
}

double AnimationPlaybackEvent::timelineTime() const {
  return timeline_time_;
}

const AtomicString& AnimationPlaybackEvent::InterfaceName() const {
  return EventNames::AnimationPlaybackEvent;
}

void AnimationPlaybackEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
