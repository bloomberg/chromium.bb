// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/events/test/cocoa_test_event_utils.h"

#include <stdint.h>

#include "base/time/time.h"
#include "ui/events/base_event_utils.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

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
    CGEventSetFlags(cg_event, static_cast<CGEventFlags>(modifiers));
    NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
    CFRelease(cg_event);
    return event;
  }
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifiers
                           timestamp:TimeIntervalSinceSystemStartup()
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:1.0];
}

NSEvent* MouseEventWithType(NSEventType type, NSUInteger modifiers) {
  return MouseEventAtPoint(NSZeroPoint, type, modifiers);
}

NSEvent* MouseEventAtPointInWindow(NSPoint point,
                                   NSEventType type,
                                   NSWindow* window,
                                   NSUInteger clickCount) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:TimeIntervalSinceSystemStartup()
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

std::pair<NSEvent*, NSEvent*> RightMouseClickInView(NSView* view,
                                                    NSUInteger clickCount) {
  const NSRect bounds = [view convertRect:[view bounds] toView:nil];
  const NSPoint mid_point = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  NSEvent* down = MouseEventAtPointInWindow(mid_point, NSRightMouseDown,
                                            [view window], clickCount);
  NSEvent* up = MouseEventAtPointInWindow(mid_point, NSRightMouseUp,
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
                         timestamp:TimeIntervalSinceSystemStartup()
                      windowNumber:0
                           context:nil
                        characters:chars
       charactersIgnoringModifiers:chars
                         isARepeat:NO
                           keyCode:key_code];
}

static NSEvent* EnterExitEventWithType(NSEventType event_type) {
  return [NSEvent enterExitEventWithType:event_type
                                location:NSZeroPoint
                           modifierFlags:0
                               timestamp:TimeIntervalSinceSystemStartup()
                            windowNumber:0
                                 context:nil
                             eventNumber:0
                          trackingNumber:0
                                userData:NULL];
}

NSEvent* EnterEvent() {
  return EnterExitEventWithType(NSMouseEntered);
}

NSEvent* ExitEvent() {
  return EnterExitEventWithType(NSMouseExited);
}

NSEvent* OtherEventWithType(NSEventType event_type) {
  return [NSEvent otherEventWithType:event_type
                            location:NSZeroPoint
                       modifierFlags:0
                           timestamp:TimeIntervalSinceSystemStartup()
                        windowNumber:0
                             context:nil
                             subtype:0
                               data1:0
                               data2:0];
}

NSTimeInterval TimeIntervalSinceSystemStartup() {
  base::TimeDelta time_elapsed = ui::EventTimeForNow() - base::TimeTicks();
  return time_elapsed.InSecondsF();
}

NSEvent* SynthesizeKeyEvent(NSWindow* window,
                            bool keyDown,
                            ui::KeyboardCode keycode,
                            NSUInteger flags,
                            ui::DomKey dom_key) {
  // If caps lock is set for an alpha keycode, treat it as if shift was pressed.
  // Note on Mac (unlike other platforms) shift while caps is down does not go
  // back to lowercase.
  if (keycode >= ui::VKEY_A && keycode <= ui::VKEY_Z &&
      (flags & NSAlphaShiftKeyMask))
    flags |= NSShiftKeyMask;

  // Clear caps regardless -- MacKeyCodeForWindowsKeyCode doesn't implement
  // logic to support it.
  flags &= ~NSAlphaShiftKeyMask;

  // Call sites may generate unicode character events with an undefined
  // keycode. Since it's not feasible to determine the correct keycode for
  // each unicode character, we use a dummy keycode corresponding to key 'A'.
  if (dom_key.IsCharacter() && keycode == ui::VKEY_UNKNOWN)
    keycode = ui::VKEY_A;

  unichar character;
  unichar shifted_character;
  int macKeycode = ui::MacKeyCodeForWindowsKeyCode(
      keycode, flags, &shifted_character, &character);

  if (macKeycode < 0)
    return nil;

  // If an explicit unicode character is provided, use that instead of the one
  // derived from the keycode.
  if (dom_key.IsCharacter())
    shifted_character = dom_key.ToCharacter();

  // Note that, in line with AppKit's documentation (and tracing "real" events),
  // -[NSEvent charactersIngoringModifiers]" are "the characters generated by
  // the receiving key event as if no modifier key (except for Shift)".
  // So |charactersIgnoringModifiers| uses |shifted_character|.
  NSString* charactersIgnoringModifiers =
      [[[NSString alloc] initWithCharacters:&shifted_character
                                     length:1] autorelease];
  NSString* characters;
  // The following were determined empirically on OSX 10.9.
  if (flags & NSControlKeyMask) {
    // If Ctrl is pressed, Cocoa always puts an empty string into |characters|.
    characters = [NSString string];
  } else if (flags & NSCommandKeyMask) {
    // If Cmd is pressed, Cocoa puts a lowercase character into |characters|,
    // regardless of Shift. If, however, Alt is also pressed then shift *is*
    // preserved, but re-mappings for Alt are not implemented. Although we still
    // need to support Alt for things like Alt+Left/Right which don't care.
    characters =
        [[[NSString alloc] initWithCharacters:&character length:1] autorelease];
  } else {
    // If just Shift or nothing is pressed, |characters| will match
    // |charactersIgnoringModifiers|. Alt puts a special character into
    // |characters| (not |charactersIgnoringModifiers|), but they're not mapped
    // here.
    characters = charactersIgnoringModifiers;
  }

  NSEventType type = (keyDown ? NSKeyDown : NSKeyUp);

  // Modifier keys generate NSFlagsChanged event rather than
  // NSKeyDown/NSKeyUp events.
  if (keycode == ui::VKEY_CONTROL || keycode == ui::VKEY_SHIFT ||
      keycode == ui::VKEY_MENU || keycode == ui::VKEY_COMMAND)
    type = NSFlagsChanged;

  // For events other than mouse moved, [event locationInWindow] is
  // UNDEFINED if the event is not NSMouseMoved.  Thus, the (0,0)
  // location should be fine.
  NSEvent* event = [NSEvent keyEventWithType:type
                                    location:NSZeroPoint
                               modifierFlags:flags
                                   timestamp:TimeIntervalSinceSystemStartup()
                                windowNumber:[window windowNumber]
                                     context:nil
                                  characters:characters
                 charactersIgnoringModifiers:charactersIgnoringModifiers
                                   isARepeat:NO
                                     keyCode:(unsigned short)macKeycode];

  return event;
}

}  // namespace cocoa_test_event_utils
