// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <vector>

#include "ui/events/event.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ui {

namespace {
int g_custom_event_types = ET_LAST;
}  // namespace

bool EventCanceledDefaultHandling(const Event& event) {
  return event.phase() == EP_POSTTARGET && event.result() != ER_UNHANDLED;
}

int RegisterCustomEventType() {
  return ++g_custom_event_types;
}

base::TimeDelta EventTimeForNow() {
  return base::TimeDelta::FromInternalValue(
      base::TimeTicks::Now().ToInternalValue());
}

bool ShouldDefaultToNaturalScroll() {
  gfx::Screen* screen = gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE);
  if (!screen)
    return false;
  const std::vector<gfx::Display>& displays = screen->GetAllDisplays();
  for (std::vector<gfx::Display>::const_iterator it = displays.begin();
       it != displays.end(); ++it) {
    const gfx::Display& display = *it;
    if (display.IsInternal() &&
        display.touch_support() == gfx::Display::TOUCH_SUPPORT_AVAILABLE)
      return true;
  }
  return false;
}

}  // namespace ui
