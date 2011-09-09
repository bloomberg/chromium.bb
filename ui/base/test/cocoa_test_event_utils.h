// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_COCOA_TEST_EVENT_UTILS_H_
#define UI_BASE_TEST_COCOA_TEST_EVENT_UTILS_H_
#pragma once

#include <utility>

#import <objc/objc-class.h>

#include "base/basictypes.h"

// Within a given scope, replace the selector |selector| on |target| with that
// from |source|.
class ScopedClassSwizzler {
 public:
  ScopedClassSwizzler(Class target, Class source, SEL selector);
  ~ScopedClassSwizzler();

 private:
  Method old_selector_impl_;
  Method new_selector_impl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedClassSwizzler);
};

namespace cocoa_test_event_utils {

// Create synthetic mouse events for testing. Currently these are very
// basic, flesh out as needed.  Points are all in window coordinates;
// where the window is not specified, coordinate system is undefined
// (but will be repeated when the event is queried).
NSEvent* MakeMouseEvent(NSEventType type, NSUInteger modifiers);
NSEvent* MouseEventAtPoint(NSPoint point, NSEventType type,
                           NSUInteger modifiers);
NSEvent* LeftMouseDownAtPoint(NSPoint point);
NSEvent* LeftMouseDownAtPointInWindow(NSPoint point, NSWindow* window);

// Return a mouse down and an up event with the given |clickCount| at
// |view|'s midpoint.
std::pair<NSEvent*, NSEvent*> MouseClickInView(NSView* view,
                                               NSUInteger clickCount);

}  // namespace cocoa_test_event_utils

#endif  // UI_BASE_TEST_COCOA_TEST_EVENT_UTILS_H_
