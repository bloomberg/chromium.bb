// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/geometry/point.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace ui {

namespace {

// Although CGEventFlags is just a typedef to int in 10.10 and earlier headers,
// the 10.11 header makes this a CF_ENUM, but doesn't give an option for "none".
const CGEventFlags kNoEventFlags = static_cast<CGEventFlags>(0);

class EventsMacTest : public CocoaTest {
 public:
  EventsMacTest() {}

  gfx::Point Flip(gfx::Point window_location) {
    NSRect window_frame = [test_window() frame];
    CGFloat content_height =
        NSHeight([test_window() contentRectForFrameRect:window_frame]);
    window_location.set_y(content_height - window_location.y());
    return window_location;
  }

  // TODO(tapted): Move this to cocoa_test_event_utils. It's not a drop-in
  // replacement because -[NSApp sendEvent:] may route events generated this way
  // differently.
  NSEvent* TestMouseEvent(CGEventType type,
                          const gfx::Point& window_location,
                          CGEventFlags event_flags) {
    // CGEventCreateMouseEvent() ignores the CGMouseButton parameter unless
    // |type| is one of kCGEventOtherMouse{Up,Down,Dragged}. It can be an
    // integer up to 31. However, constants are only supplied up to 2. For now,
    // just assume "other" means the third/center mouse button, and rely on
    // Quartz ignoring it when the type is not "other".
    CGMouseButton other_button = kCGMouseButtonCenter;
    CGPoint screen_point = cocoa_test_event_utils::ScreenPointFromWindow(
        Flip(window_location).ToCGPoint(), test_window());
    base::ScopedCFTypeRef<CGEventRef> mouse(
        CGEventCreateMouseEvent(nullptr, type, screen_point, other_button));
    CGEventSetFlags(mouse, event_flags);
    return cocoa_test_event_utils::AttachWindowToCGEvent(mouse, test_window());
  }

  // Creates a scroll event from a "real" mouse wheel (i.e. not a trackpad).
  NSEvent* TestScrollEvent(const gfx::Point& window_location,
                           int32_t delta_x,
                           int32_t delta_y) {
    return cocoa_test_event_utils::TestScrollEvent(
        Flip(window_location).ToCGPoint(), test_window(), delta_x, delta_y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EventsMacTest);
};

}  // namespace

TEST_F(EventsMacTest, EventFlagsFromNative) {
  // Left click.
  NSEvent* left = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, EventFlagsFromNative(left));

  // Right click.
  NSEvent* right = cocoa_test_event_utils::MouseEventWithType(NSRightMouseUp,
                                                              0);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, EventFlagsFromNative(right));

  // Middle click.
  NSEvent* middle = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp,
                                                               0);
  EXPECT_EQ(EF_MIDDLE_MOUSE_BUTTON, EventFlagsFromNative(middle));

  // Caps + Left
  NSEvent* caps = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSAlphaShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_CAPS_LOCK_ON, EventFlagsFromNative(caps));

  // Shift + Left
  NSEvent* shift = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                              NSShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN, EventFlagsFromNative(shift));

  // Ctrl + Left. Note we map this to a right click on Mac and remove Control.
  NSEvent* ctrl = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                             NSControlKeyMask);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, EventFlagsFromNative(ctrl));

  // Ctrl + Right. Remains a right click.
  NSEvent* ctrl_right = cocoa_test_event_utils::MouseEventWithType(
      NSRightMouseUp, NSControlKeyMask);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON | EF_CONTROL_DOWN,
            EventFlagsFromNative(ctrl_right));

  // Alt + Left
  NSEvent* alt = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_ALT_DOWN, EventFlagsFromNative(alt));

  // Cmd + Left
  NSEvent* cmd = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSCommandKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN, EventFlagsFromNative(cmd));

  // Shift + Ctrl + Left. Also mapped to a right-click. Control removed.
  NSEvent* shiftctrl = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSShiftKeyMask | NSControlKeyMask);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON | EF_SHIFT_DOWN,
            EventFlagsFromNative(shiftctrl));

  // Cmd + Alt + Right
  NSEvent* cmdalt = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSCommandKeyMask | NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN | EF_ALT_DOWN,
            EventFlagsFromNative(cmdalt));
}

// Tests mouse button presses and mouse wheel events.
TEST_F(EventsMacTest, ButtonEvents) {
  gfx::Point location(5, 10);
  gfx::Vector2d offset;

  NSEvent* event =
      TestMouseEvent(kCGEventLeftMouseDown, location, kNoEventFlags);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));

  event =
      TestMouseEvent(kCGEventOtherMouseDown, location, kCGEventFlagMaskShift);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
            ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));

  event = TestMouseEvent(kCGEventRightMouseUp, location, kNoEventFlags);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));

  // Scroll up.
  event = TestScrollEvent(location, 0, 1);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location.ToString(), ui::EventLocationFromNative(event).ToString());
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_GT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll down.
  event = TestScrollEvent(location, 0, -1);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll left.
  event = TestScrollEvent(location, 1, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_GT(offset.x(), 0);

  // Scroll right.
  event = TestScrollEvent(location, -1, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_LT(offset.x(), 0);
}

// Test correct location when the window has a native titlebar.
TEST_F(EventsMacTest, NativeTitlebarEventLocation) {
  gfx::Point location(5, 10);
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask;

  // First check that the window provided by ui::CocoaTest is how we think.
  DCHECK_EQ(NSBorderlessWindowMask, [test_window() styleMask]);
  [test_window() setStyleMask:style_mask];
  DCHECK_EQ(style_mask, [test_window() styleMask]);

  // EventLocationFromNative should behave the same as the ButtonEvents test.
  NSEvent* event =
      TestMouseEvent(kCGEventLeftMouseDown, location, kNoEventFlags);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));

  // And be explicit, to ensure the test doesn't depend on some property of the
  // test harness. The change to the frame rect could be OS-specfic, so set it
  // to a known value.
  const CGFloat kTestHeight = 400;
  NSRect content_rect = NSMakeRect(0, 0, 600, kTestHeight);
  NSRect frame_rect = [test_window() frameRectForContentRect:content_rect];
  [test_window() setFrame:frame_rect display:YES];
  event = [NSEvent mouseEventWithType:NSLeftMouseDown
                             location:NSMakePoint(0, 0)  // Bottom-left corner.
                        modifierFlags:0
                            timestamp:0
                         windowNumber:[test_window() windowNumber]
                              context:nil
                          eventNumber:0
                           clickCount:0
                             pressure:1.0];
  // Bottom-left corner should be flipped.
  EXPECT_EQ(gfx::Point(0, kTestHeight), ui::EventLocationFromNative(event));

  // Removing the border, and sending the same event should move it down in the
  // toolkit-views coordinate system.
  int height_change = NSHeight(frame_rect) - kTestHeight;
  EXPECT_GT(height_change, 0);
  [test_window() setStyleMask:NSBorderlessWindowMask];
  [test_window() setFrame:frame_rect display:YES];
  EXPECT_EQ(gfx::Point(0, kTestHeight + height_change),
            ui::EventLocationFromNative(event));
}

// Testing for ui::EventTypeFromNative() not covered by ButtonEvents.
TEST_F(EventsMacTest, EventTypeFromNative) {
  NSEvent* event = cocoa_test_event_utils::KeyEventWithType(NSKeyDown, 0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::KeyEventWithType(NSKeyUp, 0);
  EXPECT_EQ(ui::ET_KEY_RELEASED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseDragged, 0);
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, ui::EventTypeFromNative(event));
  event = cocoa_test_event_utils::MouseEventWithType(NSRightMouseDragged, 0);
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, ui::EventTypeFromNative(event));
  event = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseDragged, 0);
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::MouseEventWithType(NSMouseMoved, 0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::EnterEvent();
  EXPECT_EQ(ui::ET_MOUSE_ENTERED, ui::EventTypeFromNative(event));
  event = cocoa_test_event_utils::ExitEvent();
  EXPECT_EQ(ui::ET_MOUSE_EXITED, ui::EventTypeFromNative(event));
}

}  // namespace ui
