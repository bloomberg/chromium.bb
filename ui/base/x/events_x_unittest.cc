// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include <X11/Xlib.h>

// Generically-named #defines from Xlib that conflict with symbols in GTest.
#undef Bool
#undef None

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/events/event_constants.h"
#include "ui/gfx/point.h"

namespace ui {

namespace {

// Initializes the passed-in Xlib event.
void InitButtonEvent(XEvent* event,
                     bool is_press,
                     const gfx::Point& location,
                     int button,
                     int state) {
  memset(event, 0, sizeof(*event));

  // We don't bother setting fields that the event code doesn't use, such as
  // x_root/y_root and window/root/subwindow.
  XButtonEvent* button_event = &(event->xbutton);
  button_event->type = is_press ? ButtonPress : ButtonRelease;
  button_event->x = location.x();
  button_event->y = location.y();
  button_event->button = button;
  button_event->state = state;
}

}  // namespace

TEST(EventsXTest, ButtonEvents) {
  XEvent event;
  gfx::Point location(5, 10);

  InitButtonEvent(&event, true, location, 1, 0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));

  InitButtonEvent(&event, true, location, 2, Button1Mask | ShiftMask);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
                ui::EF_SHIFT_DOWN,
            ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));

  InitButtonEvent(&event, false, location, 3, 0);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));

  // Scroll up.
  InitButtonEvent(&event, true, location, 4, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  EXPECT_GT(ui::GetMouseWheelOffset(&event), 0);

  // Scroll down.
  InitButtonEvent(&event, true, location, 5, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  EXPECT_LT(ui::GetMouseWheelOffset(&event), 0);

  // Scroll left, typically.
  InitButtonEvent(&event, true, location, 6, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  EXPECT_EQ(0, ui::GetMouseWheelOffset(&event));

  // Scroll right, typically.
  InitButtonEvent(&event, true, location, 7, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  EXPECT_EQ(0, ui::GetMouseWheelOffset(&event));

  // TODO(derat): Test XInput code.
}

TEST(EventsXTest, AvoidExtraEventsOnWheelRelease) {
  XEvent event;
  gfx::Point location(5, 10);

  InitButtonEvent(&event, true, location, 4, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));

  // We should return ET_UNKNOWN for the release event instead of returning
  // ET_MOUSEWHEEL; otherwise we'll scroll twice for each scrollwheel step.
  InitButtonEvent(&event, false, location, 4, 0);
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromNative(&event));

  // TODO(derat): Test XInput code.
}

}  // namespace ui
