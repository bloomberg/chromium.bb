// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/cocoa/cocoa_event_utils.h"

#import <objc/objc-class.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace ui {

namespace {

class CocoaEventUtilsTest : public CocoaTest {
};

TEST_F(CocoaEventUtilsTest, EventFlagsFromNative) {
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

}  // namespace

}  // namespace ui
