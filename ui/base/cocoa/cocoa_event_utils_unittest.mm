// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/objc-class.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/cocoa/cocoa_event_utils.h"
#include "ui/events/event_constants.h"
#import "ui/base/test/cocoa_test_event_utils.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

// We provide a donor class with a specially modified |modifierFlags|
// implementation that we swap with NSEvent's. This is because we can't create a
// NSEvent that represents a middle click with modifiers.
@interface TestEvent : NSObject
@end
@implementation TestEvent
- (NSUInteger)modifierFlags { return NSShiftKeyMask; }
@end

namespace ui {

namespace {

class EventUtilsTest : public CocoaTest {
};

TEST_F(EventUtilsTest, TestWindowOpenDispositionFromNSEvent) {
  // Left Click = same tab.
  NSEvent* me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
  EXPECT_EQ(CURRENT_TAB, WindowOpenDispositionFromNSEvent(me));

  // Middle Click = new background tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp, 0);
  EXPECT_EQ(NEW_BACKGROUND_TAB, WindowOpenDispositionFromNSEvent(me));

  // Shift+Middle Click = new foreground tab.
  {
    ScopedClassSwizzler swizzler([NSEvent class], [TestEvent class],
                                 @selector(modifierFlags));
    me = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp,
                                                    NSShiftKeyMask);
    EXPECT_EQ(NEW_FOREGROUND_TAB, WindowOpenDispositionFromNSEvent(me));
  }

  // Cmd+Left Click = new background tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                  NSCommandKeyMask);
  EXPECT_EQ(NEW_BACKGROUND_TAB, WindowOpenDispositionFromNSEvent(me));

  // Cmd+Shift+Left Click = new foreground tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                              NSCommandKeyMask |
                                              NSShiftKeyMask);
  EXPECT_EQ(NEW_FOREGROUND_TAB, WindowOpenDispositionFromNSEvent(me));

  // Shift+Left Click = new window
  me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                  NSShiftKeyMask);
  EXPECT_EQ(NEW_WINDOW, WindowOpenDispositionFromNSEvent(me));
}


TEST_F(EventUtilsTest, TestEventFlagsFromNSEvent) {
  // Left click.
  NSEvent* left = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, EventFlagsFromNSEvent(left));

  // Right click.
  NSEvent* right = cocoa_test_event_utils::MouseEventWithType(NSRightMouseUp,
                                                              0);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, EventFlagsFromNSEvent(right));

  // Middle click.
  NSEvent* middle = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp,
                                                               0);
  EXPECT_EQ(EF_MIDDLE_MOUSE_BUTTON, EventFlagsFromNSEvent(middle));

  // Caps + Left
  NSEvent* caps =
      cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                 NSAlphaShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_CAPS_LOCK_DOWN,
            EventFlagsFromNSEvent(caps));

  // Shift + Left
  NSEvent* shift = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                              NSShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN, EventFlagsFromNSEvent(shift));

  // Ctrl + Left
  NSEvent* ctrl = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                             NSControlKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_CONTROL_DOWN,
            EventFlagsFromNSEvent(ctrl));

  // Alt + Left
  NSEvent* alt = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_ALT_DOWN, EventFlagsFromNSEvent(alt));

  // Cmd + Left
  NSEvent* cmd = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSCommandKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN, EventFlagsFromNSEvent(cmd));

  // Shift + Ctrl + Left
  NSEvent* shiftctrl =
      cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                 NSShiftKeyMask |
                                                 NSControlKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN | EF_CONTROL_DOWN,
            EventFlagsFromNSEvent(shiftctrl));

  // Cmd + Alt + Right
  NSEvent* cmdalt =
      cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                 NSCommandKeyMask |
                                                 NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN | EF_ALT_DOWN,
            EventFlagsFromNSEvent(cmdalt));
}

}  // namespace

}  // namespace ui
