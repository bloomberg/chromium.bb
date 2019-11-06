// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_UTILS_H_
#define UI_VIEWS_EVENT_UTILS_H_

#include "ui/views/views_export.h"

namespace base {
class TimeTicks;
}

namespace ui {
class Event;
}

namespace views {

// Returns true if the event is a mouse, touch, or pointer event that took place
// within the double-click time interval after the |initial_timestamp|.
VIEWS_EXPORT bool IsPossiblyUnintendedInteraction(
    const base::TimeTicks& initial_timestamp,
    const ui::Event& event);

}  // namespace views

#endif  // UI_VIEWS_EVENT_UTILS_H_
