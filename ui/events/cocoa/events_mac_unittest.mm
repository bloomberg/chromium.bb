// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/point.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

NSWindow* g_test_window = nil;

}  // namespace

// Mac APIs for creating test events are frustrating. Quartz APIs have, e.g.,
// CGEventCreateMouseEvent() which can't set a window or modifier flags.
// Cocoa APIs have +[NSEvent mouseEventWithType:..] which can't set
// buttonNumber or scroll deltas. To work around this, these tests use some
// Objective C magic to donate member functions to NSEvent temporarily.
@interface MiddleMouseButtonNumberDonor : NSObject
@end

@interface TestWindowDonor : NSObject
@end

@implementation MiddleMouseButtonNumberDonor
- (NSUInteger)buttonNumber { return 2; }
@end

@implementation TestWindowDonor
- (NSWindow*)window { return g_test_window; }
@end

namespace ui {

namespace {

class EventsMacTest : public CocoaTest {
 public:
  EventsMacTest() {}

  gfx::Point Flip(gfx::Point window_location) {
    window_location.set_y(
        NSHeight([test_window() frame]) - window_location.y());
    return window_location;
  }

  void SwizzleMiddleMouseButton() {
    DCHECK(!swizzler_);
    swizzler_.reset(new ScopedClassSwizzler(
        [NSEvent class],
        [MiddleMouseButtonNumberDonor class],
        @selector(buttonNumber)));
  }

  void SwizzleTestWindow() {
    DCHECK(!g_test_window);
    DCHECK(!swizzler_);
    g_test_window = test_window();
    swizzler_.reset(new ScopedClassSwizzler(
        [NSEvent class],
        [TestWindowDonor class],
        @selector(window)));
  }

  void ClearSwizzle() {
    swizzler_.reset();
    g_test_window = nil;
  }

  NSEvent* TestMouseEvent(NSEventType type,
                          const gfx::Point &window_location,
                          NSInteger modifier_flags) {
    NSPoint point = NSPointFromCGPoint(Flip(window_location).ToCGPoint());
    return [NSEvent mouseEventWithType:type
                              location:point
                         modifierFlags:modifier_flags
                             timestamp:0
                          windowNumber:[test_window() windowNumber]
                               context:nil
                           eventNumber:0
                            clickCount:0
                              pressure:1.0];
  }

  NSEvent* TestScrollEvent(const gfx::Point& window_location,
                           int32_t delta_x,
                           int32_t delta_y) {
    SwizzleTestWindow();
    base::ScopedCFTypeRef<CGEventRef> scroll(
        CGEventCreateScrollWheelEvent(NULL,
                                      kCGScrollEventUnitLine,
                                      2,
                                      delta_y,
                                      delta_x));
    // CGEvents are always in global display coordinates. These are like screen
    // coordinates, but flipped. But first the point needs to be converted out
    // of window coordinates (which also requires flipping).
    NSPoint window_point =
        NSPointFromCGPoint(Flip(window_location).ToCGPoint());
    NSPoint screen_point = [test_window() convertBaseToScreen:window_point];
    CGFloat primary_screen_height =
        NSHeight([[[NSScreen screens] objectAtIndex:0] frame]);
    screen_point.y = primary_screen_height - screen_point.y;
    CGEventSetLocation(scroll, NSPointToCGPoint(screen_point));
    return [NSEvent eventWithCGEvent:scroll];
  }

 private:
  scoped_ptr<ScopedClassSwizzler> swizzler_;

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
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_CAPS_LOCK_DOWN,
            EventFlagsFromNative(caps));

  // Shift + Left
  NSEvent* shift = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                              NSShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN, EventFlagsFromNative(shift));

  // Ctrl + Left
  NSEvent* ctrl = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                             NSControlKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_CONTROL_DOWN, EventFlagsFromNative(ctrl));

  // Alt + Left
  NSEvent* alt = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_ALT_DOWN, EventFlagsFromNative(alt));

  // Cmd + Left
  NSEvent* cmd = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSCommandKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN, EventFlagsFromNative(cmd));

  // Shift + Ctrl + Left
  NSEvent* shiftctrl = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSShiftKeyMask | NSControlKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN | EF_CONTROL_DOWN,
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

  NSEvent* event = TestMouseEvent(NSLeftMouseDown, location, 0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));

  SwizzleMiddleMouseButton();
  event = TestMouseEvent(NSOtherMouseDown, location, NSShiftKeyMask);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
            ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  ClearSwizzle();

  event = TestMouseEvent(NSRightMouseUp, location, 0);
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
  ClearSwizzle();

  // Scroll down.
  event = TestScrollEvent(location, 0, -1);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());
  ClearSwizzle();

  // Scroll left.
  event = TestScrollEvent(location, 1, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_GT(offset.x(), 0);
  ClearSwizzle();

  // Scroll right.
  event = TestScrollEvent(location, -1, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_LT(offset.x(), 0);
  ClearSwizzle();
}

}  // namespace ui
