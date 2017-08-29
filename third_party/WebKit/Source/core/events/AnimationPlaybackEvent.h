// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationPlaybackEvent_h
#define AnimationPlaybackEvent_h

#include "core/dom/events/Event.h"
#include "core/events/AnimationPlaybackEventInit.h"

namespace blink {

class AnimationPlaybackEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationPlaybackEvent* Create(const AtomicString& type,
                                        double current_time,
                                        double timeline_time) {
    return new AnimationPlaybackEvent(type, current_time, timeline_time);
  }
  static AnimationPlaybackEvent* Create(
      const AtomicString& type,
      const AnimationPlaybackEventInit& initializer) {
    return new AnimationPlaybackEvent(type, initializer);
  }

  ~AnimationPlaybackEvent() override;

  double currentTime(bool& is_null) const;
  double currentTime() const;
  double timelineTime(bool& is_null) const;
  double timelineTime() const;

  const AtomicString& InterfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  AnimationPlaybackEvent(const AtomicString& type,
                         double current_time,
                         double timeline_time);
  AnimationPlaybackEvent(const AtomicString&,
                         const AnimationPlaybackEventInit&);

  double current_time_;
  double timeline_time_;
};

}  // namespace blink

#endif  // AnimationPlaybackEvent_h
