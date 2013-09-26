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
#include "ui/events/x/device_data_manager.h"
#include "ui/events/x/touch_factory_x11.h"
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

#if defined(USE_XI2_MT)
const int kValuatorNum = 3;
const int kTouchValuatorMap[kValuatorNum][4] = {
  // { valuator_index, valuator_type, min_val, max_val }
  { 0, DeviceDataManager::DT_TOUCH_MAJOR, 0, 1000},
  { 1, DeviceDataManager::DT_TOUCH_ORIENTATION, 0, 1.0},
  { 2, DeviceDataManager::DT_TOUCH_PRESSURE, 0, 1000},
};

struct Valuator {
  Valuator(DeviceDataManager::DataType type, double v)
      : data_type(type), value(v) {}

  DeviceDataManager::DataType data_type;
  double value;
};

XEvent* CreateTouchEvent(int deviceid,
                         int evtype,
                         int tracking_id,
                         const gfx::Point& location,
                         const std::vector<Valuator>& valuators) {
  XEvent* event = new XEvent;
  memset(event, 0, sizeof(*event));
  event->type = GenericEvent;
  event->xcookie.data = new XIDeviceEvent;
  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(event->xcookie.data);
  xiev->deviceid = deviceid;
  xiev->sourceid = deviceid;
  xiev->evtype = evtype;
  xiev->detail = tracking_id;
  xiev->event_x = location.x();
  xiev->event_y = location.y();

  xiev->valuators.mask_len = (valuators.size() / 8) + 1;
  xiev->valuators.mask = new unsigned char[xiev->valuators.mask_len];
  memset(xiev->valuators.mask, 0, xiev->valuators.mask_len);
  xiev->valuators.values = new double[valuators.size()];

  int val_count = 0;
  for (int i = 0; i < kValuatorNum; i++) {
    for(size_t j = 0; j < valuators.size(); j++) {
      if (valuators[j].data_type == kTouchValuatorMap[i][1]) {
        XISetMask(xiev->valuators.mask, kTouchValuatorMap[i][0]);
        xiev->valuators.values[val_count++] = valuators[j].value;
      }
    }
  }

  return event;
}

void DestroyTouchEvent(XEvent* event) {
  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(event->xcookie.data);
  if (xiev) {
    delete[] xiev->valuators.mask;
    delete[] xiev->valuators.values;
    delete xiev;
  }
  delete event;
}

void SetupTouchFactory(const std::vector<unsigned int>& devices) {
  TouchFactory* factory = TouchFactory::GetInstance();
  factory->SetTouchDeviceForTest(devices);
}

void SetupDeviceDataManager(const std::vector<unsigned int>& devices) {
  ui::DeviceDataManager* manager = ui::DeviceDataManager::GetInstance();
  manager->SetDeviceListForTest(devices);
  for (size_t i = 0; i < devices.size(); i++) {
    for (int j = 0; j < kValuatorNum; j++) {
      manager->SetDeviceValuatorForTest(
          devices[i],
          kTouchValuatorMap[j][0],
          static_cast<DeviceDataManager::DataType>(kTouchValuatorMap[j][1]),
          kTouchValuatorMap[j][2],
          kTouchValuatorMap[j][3]);
    }
  }
}
#endif
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
  SetupTouchFactory(devices);
  SetupDeviceDataManager(devices);
  XEvent* event = NULL;
  std::vector<Valuator> valuators;

  // Init touch begin with tracking id 5, touch id 0.
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_MAJOR, 20));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_ORIENTATION, 0.3f));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_PRESSURE, 100));
  event = CreateTouchEvent(0, XI_TouchBegin, 5, gfx::Point(10, 10), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ("10,10", ui::EventLocationFromNative(event).ToString());
  EXPECT_EQ(GetTouchId(event), 0);
  EXPECT_EQ(GetTouchRadiusX(event), 10);
  EXPECT_FLOAT_EQ(GetTouchAngle(event), 0.15f);
  EXPECT_FLOAT_EQ(GetTouchForce(event), 0.1f);
  DestroyTouchEvent(event);

  // Touch update, with new orientation info.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_ORIENTATION, 0.5f));
  event = CreateTouchEvent(0, XI_TouchUpdate, 5, gfx::Point(20, 20), valuators);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, ui::EventTypeFromNative(event));
  EXPECT_EQ("20,20", ui::EventLocationFromNative(event).ToString());
  EXPECT_EQ(GetTouchId(event), 0);
  EXPECT_EQ(GetTouchRadiusX(event), 10);
  EXPECT_FLOAT_EQ(GetTouchAngle(event), 0.25f);
  EXPECT_FLOAT_EQ(GetTouchForce(event), 0.1f);
  DestroyTouchEvent(event);

  // Another touch with tracking id 6, touch id 1.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_MAJOR, 100));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_ORIENTATION, 0.9f));
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_PRESSURE, 500));
  event = CreateTouchEvent(
      0, XI_TouchBegin, 6, gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ("200,200", ui::EventLocationFromNative(event).ToString());
  EXPECT_EQ(GetTouchId(event), 1);
  EXPECT_EQ(GetTouchRadiusX(event), 50);
  EXPECT_FLOAT_EQ(GetTouchAngle(event), 0.45f);
  EXPECT_FLOAT_EQ(GetTouchForce(event), 0.5f);
  DestroyTouchEvent(event);

  // Touch with tracking id 5 should have old radius/angle value and new pressue
  // value.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_PRESSURE, 50));
  event = CreateTouchEvent(0, XI_TouchEnd, 5, gfx::Point(30, 30), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromNative(event));
  EXPECT_EQ("30,30", ui::EventLocationFromNative(event).ToString());
  EXPECT_EQ(GetTouchId(event), 0);
  EXPECT_EQ(GetTouchRadiusX(event), 10);
  EXPECT_FLOAT_EQ(GetTouchAngle(event), 0.25f);
  EXPECT_FLOAT_EQ(GetTouchForce(event), 0.05f);
  DestroyTouchEvent(event);

  // Touch with tracking id 6 should have old angle/pressure value and new
  // radius value.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManager::DT_TOUCH_MAJOR, 50));
  event = CreateTouchEvent(0, XI_TouchEnd, 6, gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromNative(event));
  EXPECT_EQ("200,200", ui::EventLocationFromNative(event).ToString());
  EXPECT_EQ(GetTouchId(event), 1);
  EXPECT_EQ(GetTouchRadiusX(event), 25);
  EXPECT_FLOAT_EQ(GetTouchAngle(event), 0.45f);
  EXPECT_FLOAT_EQ(GetTouchForce(event), 0.5f);
  DestroyTouchEvent(event);
}
#endif
}  // namespace ui
