// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

// Generically-named #defines from Xlib that conflict with symbols in GTest.
#undef Bool
#undef None

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
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
  gfx::Vector2d offset;

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
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_GT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll down.
  InitButtonEvent(&event, true, location, 5, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll left, typically.
  InitButtonEvent(&event, true, location, 6, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_EQ(0, offset.y());
  EXPECT_EQ(0, offset.x());

  // Scroll right, typically.
  InitButtonEvent(&event, true, location, 7, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(location, ui::EventLocationFromNative(&event));
  EXPECT_TRUE(ui::IsMouseEvent(&event));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_EQ(0, offset.y());
  EXPECT_EQ(0, offset.x());

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

TEST(EventsXTest, EnterLeaveEvent) {
  XEvent event;
  event.xcrossing.type = EnterNotify;
  event.xcrossing.x = 10;
  event.xcrossing.y = 20;
  event.xcrossing.x_root = 110;
  event.xcrossing.y_root = 120;

  // Mouse enter events are converted to mouse move events to be consistent with
  // the way views handle mouse enter. See comments for EnterNotify case in
  // ui::EventTypeFromNative for more details.
  EXPECT_EQ(ui::ET_MOUSE_MOVED, ui::EventTypeFromNative(&event));
  EXPECT_EQ("10,20", ui::EventLocationFromNative(&event).ToString());
  EXPECT_EQ("110,120", ui::EventSystemLocationFromNative(&event).ToString());

  event.xcrossing.type = LeaveNotify;
  event.xcrossing.x = 30;
  event.xcrossing.y = 40;
  event.xcrossing.x_root = 230;
  event.xcrossing.y_root = 240;
  EXPECT_EQ(ui::ET_MOUSE_EXITED, ui::EventTypeFromNative(&event));
  EXPECT_EQ("30,40", ui::EventLocationFromNative(&event).ToString());
  EXPECT_EQ("230,240", ui::EventSystemLocationFromNative(&event).ToString());
}

TEST(EventsXTest, ClickCount) {
  XEvent event;
  gfx::Point location(5, 10);

  for (int i = 1; i <= 3; ++i) {
    InitButtonEvent(&event, true, location, 1, 0);
    {
      MouseEvent mouseev(&event);
      EXPECT_EQ(ui::ET_MOUSE_PRESSED, mouseev.type());
      EXPECT_EQ(i, mouseev.GetClickCount());
    }

    InitButtonEvent(&event, false, location, 1, 0);
    {
      MouseEvent mouseev(&event);
      EXPECT_EQ(ui::ET_MOUSE_RELEASED, mouseev.type());
      EXPECT_EQ(i, mouseev.GetClickCount());
    }
  }
}

#if defined(USE_XI2_MT)
TEST(EventsXTest, TouchEventBasic) {
  std::vector<unsigned int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<Valuator> valuators;

  // Init touch begin with tracking id 5, touch id 0.
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_MAJOR, 20));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_ORIENTATION, 0.3f));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_PRESSURE, 100));
  ui::ScopedXI2Event scoped_xevent;
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 5, gfx::Point(10, 10), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("10,10", ui::EventLocationFromNative(scoped_xevent).ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 0);
  EXPECT_EQ(GetTouchRadiusX(scoped_xevent), 10);
  EXPECT_FLOAT_EQ(GetTouchAngle(scoped_xevent), 0.15f);
  EXPECT_FLOAT_EQ(GetTouchForce(scoped_xevent), 0.1f);

  // Touch update, with new orientation info.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_ORIENTATION, 0.5f));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchUpdate, 5, gfx::Point(20, 20), valuators);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("20,20", ui::EventLocationFromNative(scoped_xevent).ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 0);
  EXPECT_EQ(GetTouchRadiusX(scoped_xevent), 10);
  EXPECT_FLOAT_EQ(GetTouchAngle(scoped_xevent), 0.25f);
  EXPECT_FLOAT_EQ(GetTouchForce(scoped_xevent), 0.1f);

  // Another touch with tracking id 6, touch id 1.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_MAJOR, 100));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_ORIENTATION, 0.9f));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_PRESSURE, 500));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 6, gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("200,200", ui::EventLocationFromNative(scoped_xevent).ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 1);
  EXPECT_EQ(GetTouchRadiusX(scoped_xevent), 50);
  EXPECT_FLOAT_EQ(GetTouchAngle(scoped_xevent), 0.45f);
  EXPECT_FLOAT_EQ(GetTouchForce(scoped_xevent), 0.5f);

  // Touch with tracking id 5 should have old radius/angle value and new pressue
  // value.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_PRESSURE, 50));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 5, gfx::Point(30, 30), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("30,30", ui::EventLocationFromNative(scoped_xevent).ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 0);
  EXPECT_EQ(GetTouchRadiusX(scoped_xevent), 10);
  EXPECT_FLOAT_EQ(GetTouchAngle(scoped_xevent), 0.25f);
  EXPECT_FLOAT_EQ(GetTouchForce(scoped_xevent), 0.05f);

  // Touch with tracking id 6 should have old angle/pressure value and new
  // radius value.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_MAJOR, 50));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 6, gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("200,200", ui::EventLocationFromNative(scoped_xevent).ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 1);
  EXPECT_EQ(GetTouchRadiusX(scoped_xevent), 25);
  EXPECT_FLOAT_EQ(GetTouchAngle(scoped_xevent), 0.45f);
  EXPECT_FLOAT_EQ(GetTouchForce(scoped_xevent), 0.5f);
}
#endif
}  // namespace ui
