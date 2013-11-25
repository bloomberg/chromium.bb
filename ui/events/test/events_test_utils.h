// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_
#define UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_

#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ui {

class EventTestApi {
 public:
  explicit EventTestApi(Event* event);
  virtual ~EventTestApi();

  void set_time_stamp(base::TimeDelta time_stamp) {
    event_->time_stamp_ = time_stamp;
  }

 private:
  EventTestApi();

  Event* event_;

  DISALLOW_COPY_AND_ASSIGN(EventTestApi);
};

class LocatedEventTestApi : public EventTestApi {
 public:
  explicit LocatedEventTestApi(LocatedEvent* located_event);
  virtual ~LocatedEventTestApi();

  void set_location(const gfx::Point& location) {
    located_event_->location_ = location;
  }

 private:
  LocatedEventTestApi();

  LocatedEvent* located_event_;

  DISALLOW_COPY_AND_ASSIGN(LocatedEventTestApi);
};

class EventTargetTestApi {
 public:
  explicit EventTargetTestApi(EventTarget* target);

  const EventHandlerList& pre_target_handlers() {
    return target_->pre_target_list_;
  }

 private:
  EventTargetTestApi();

  EventTarget* target_;

  DISALLOW_COPY_AND_ASSIGN(EventTargetTestApi);
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_
