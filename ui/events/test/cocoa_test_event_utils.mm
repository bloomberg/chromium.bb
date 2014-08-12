// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "ui/events/test/cocoa_test_event_utils.h"

namespace cocoa_test_event_utils {

NSEvent* MouseEventAtPoint(NSPoint point, NSEventType type,
                           NSUInteger modifiers) {
  if (type == NSOtherMouseUp) {
    // To synthesize middle clicks we need to create a CGEvent with the
    // "center" button flags so that our resulting NSEvent will have the
    // appropriate buttonNumber field. NSEvent provides no way to create a
    // mouse event with a buttonNumber directly.
    CGPoint location = { point.x, point.y };
    CGEventRef cg_event = CGEventCreateMouseEvent(NULL, kCGEventOtherMouseUp,
                                                  location,
                                                  kCGMouseButtonCenter);
    // Also specify the modifiers for the middle click case. This makes this
    // test resilient to external modifiers being pressed.
    CGEventSetFlags(cg_event, modifiers);
    NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
    CFRelease(cg_event);
    return event;
  }
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifiers
                           timestamp:0
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:1.0];
}

NSEvent* MouseEventWithType(NSEventType type, NSUInteger modifiers) {
  return MouseEventAtPoint(NSZeroPoint, type, modifiers);
}

static NSEvent* MouseEventAtPointInWindow(NSPoint point,
                                          NSEventType type,
                                          NSWindow* window,
                                          NSUInteger clickCount) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:clickCount
                            pressure:1.0];
}

NSEvent* RightMouseDownAtPointInWindow(NSPoint point, NSWindow* window) {
  return MouseEventAtPointInWindow(point, NSRightMouseDown, window, 1);
}

NSEvent* RightMouseDownAtPoint(NSPoint point) {
  return RightMouseDownAtPointInWindow(point, nil);
}

NSEvent* LeftMouseDownAtPointInWindow(NSPoint point, NSWindow* window) {
  return MouseEventAtPointInWindow(point, NSLeftMouseDown, window, 1);
}

NSEvent* LeftMouseDownAtPoint(NSPoint point) {
  return LeftMouseDownAtPointInWindow(point, nil);
}

std::pair<NSEvent*,NSEvent*> MouseClickInView(NSView* view,
                                              NSUInteger clickCount) {
  const NSRect bounds = [view convertRect:[view bounds] toView:nil];
  const NSPoint mid_point = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  NSEvent* down = MouseEventAtPointInWindow(mid_point, NSLeftMouseDown,
                                            [view window], clickCount);
  NSEvent* up = MouseEventAtPointInWindow(mid_point, NSLeftMouseUp,
                                          [view window], clickCount);
  return std::make_pair(down, up);
}

NSEvent* KeyEventWithCharacter(unichar c) {
  return KeyEventWithKeyCode(0, c, NSKeyDown, 0);
}

NSEvent* KeyEventWithType(NSEventType event_type, NSUInteger modifiers) {
  return KeyEventWithKeyCode(0x78, 'x', event_type, modifiers);
}

NSEvent* KeyEventWithKeyCode(unsigned short key_code,
                             unichar c,
                             NSEventType event_type,
                             NSUInteger modifiers) {
  NSString* chars = [NSString stringWithCharacters:&c length:1];
  return [NSEvent keyEventWithType:event_type
                          location:NSZeroPoint
                     modifierFlags:modifiers
                         timestamp:0
                      windowNumber:0
                           context:nil
                        characters:chars
       charactersIgnoringModifiers:chars
                         isARepeat:NO
                           keyCode:key_code];
}

NSEvent* EnterExitEventWithType(NSEventType event_type) {
  return [NSEvent enterExitEventWithType:event_type
                                location:NSZeroPoint
                           modifierFlags:0
                               timestamp:0
                            windowNumber:0
                                 context:nil
                             eventNumber:0
                          trackingNumber:0
                                userData:NULL];
}

NSEvent* OtherEventWithType(NSEventType event_type) {
  return [NSEvent otherEventWithType:event_type
                            location:NSZeroPoint
                       modifierFlags:0
                           timestamp:0
                        windowNumber:0
                             context:nil
                             subtype:0
                               data1:0
                               data2:0];
}

}  // namespace cocoa_test_event_utils
