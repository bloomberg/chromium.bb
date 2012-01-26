// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

namespace aura {
namespace test {

TEST(EventTest, NoNativeEvent) {
  KeyEvent keyev(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
  EXPECT_FALSE(keyev.HasNativeEvent());
}

TEST(EventTest, NativeEvent) {
#if defined(OS_WIN)
  MSG native_event = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  KeyEvent keyev(native_event, false);
  EXPECT_TRUE(keyev.HasNativeEvent());
#elif defined(USE_X11)
  scoped_ptr<XEvent> native_event(new XEvent);
  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              native_event.get());
  KeyEvent keyev(native_event.get(), false);
  EXPECT_TRUE(keyev.HasNativeEvent());
#endif
}

TEST(EventTest, GetCharacter) {
  // Check if Control+Enter returns 10.
  KeyEvent keyev1(ui::ET_KEY_PRESSED,
                  ui::VKEY_RETURN,
                  ui::EF_CONTROL_DOWN);
  EXPECT_EQ(10, keyev1.GetCharacter());
  EXPECT_EQ(13, keyev1.GetUnmodifiedCharacter());
  // Check if Enter returns 13.
  KeyEvent keyev2(ui::ET_KEY_PRESSED,
                  ui::VKEY_RETURN,
                  0);
  EXPECT_EQ(13, keyev2.GetCharacter());
  EXPECT_EQ(13, keyev2.GetUnmodifiedCharacter());

#if defined(USE_X11)
  // For X11, test the functions with native_event() as well. crbug.com/107837
  scoped_ptr<XEvent> native_event(new XEvent);

  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui::VKEY_RETURN,
                              ui::EF_CONTROL_DOWN,
                              native_event.get());
  KeyEvent keyev3(native_event.get(), false);
  EXPECT_EQ(10, keyev3.GetCharacter());
  EXPECT_EQ(13, keyev3.GetUnmodifiedCharacter());

  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui::VKEY_RETURN,
                              0,
                              native_event.get());
  KeyEvent keyev4(native_event.get(), false);
  EXPECT_EQ(13, keyev4.GetCharacter());
  EXPECT_EQ(13, keyev4.GetUnmodifiedCharacter());
#endif
}

TEST(EventTest, ClickCount) {
  const gfx::Point origin(0, 0);
  MouseEvent mouseev(ui::ET_MOUSE_PRESSED, origin, origin, 0);
  for (int i = 1; i <=3 ; ++i) {
    mouseev.SetClickCount(i);
    EXPECT_EQ(i, mouseev.GetClickCount());
  }
}

TEST(EventTest, Repeated) {
  const gfx::Point origin(0, 0);
  MouseEvent mouse_ev1(ui::ET_MOUSE_PRESSED, origin, origin, 0);
  MouseEvent mouse_ev2(ui::ET_MOUSE_PRESSED, origin, origin, 0);
  MouseEvent::TestApi test_ev1(&mouse_ev1);
  MouseEvent::TestApi test_ev2(&mouse_ev2);

  base::TimeDelta start = base::TimeDelta::FromMilliseconds(0);
  base::TimeDelta soon = start + base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta later = start + base::TimeDelta::FromMilliseconds(1000);

  // Close point.
  test_ev1.set_location(gfx::Point(0, 0));
  test_ev2.set_location(gfx::Point(1, 0));
  test_ev1.set_time_stamp(start);
  test_ev2.set_time_stamp(soon);
  EXPECT_TRUE(MouseEvent::IsRepeatedClickEvent(mouse_ev1, mouse_ev2));

  // Too far.
  test_ev1.set_location(gfx::Point(0, 0));
  test_ev2.set_location(gfx::Point(10, 0));
  test_ev1.set_time_stamp(start);
  test_ev2.set_time_stamp(soon);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(mouse_ev1, mouse_ev2));

  // Too long a time between clicks.
  test_ev1.set_location(gfx::Point(0, 0));
  test_ev2.set_location(gfx::Point(0, 0));
  test_ev1.set_time_stamp(start);
  test_ev2.set_time_stamp(later);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(mouse_ev1, mouse_ev2));
}

}  // namespace test
}  // namespace aura
