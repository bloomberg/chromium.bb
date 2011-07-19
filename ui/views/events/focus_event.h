// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENTS_FOCUS_EVENT_H_
#define UI_VIEWS_EVENTS_FOCUS_EVENT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/views/events/event.h"

namespace ui {

class FocusEvent : public Event {
 public:
  enum Type {
    TYPE_FOCUS_IN,      // Target View did gain focus
    TYPE_FOCUS_OUT      // Target View will lose focus
  };

  enum Reason {
    REASON_TRAVERSAL,   // Focus change occurred because of a tab-traversal
    REASON_RESTORE,     // Focus was restored (e.g. window activation).
    REASON_DIRECT       // Focus was directly set (e.g. with mouse click).
  };

  enum TraversalDirection {
    DIRECTION_NONE,
    DIRECTION_FORWARD,
    DIRECTION_REVERSE
  };

  FocusEvent(Type type,
             Reason reason,
             TraversalDirection direction);

  Type type() const { return type_; }
  Reason reason() const { return reason_; }
  TraversalDirection direction() const { return direction_; }

 private:
  Type type_;
  Reason reason_;
  TraversalDirection direction_;

  DISALLOW_COPY_AND_ASSIGN(FocusEvent);
};

}  // namespace ui

#endif  // UI_VIEWS_EVENTS_FOCUS_EVENT_H_
