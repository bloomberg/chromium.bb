// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "ui/events/event_processor.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_targeter.h"
#include "ui/events/test/event_generator.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace {

// Singleton to provide state for swizzled Objective C methods.
ui::test::EventGenerator* g_active_generator = NULL;

}  // namespace

@interface NSEventDonor : NSObject
@end

@implementation NSEventDonor

// Donate +[NSEvent pressedMouseButtons] by retrieving the flags from the
// active generator.
+ (NSUInteger)pressedMouseButtons {
  int flags = g_active_generator->flags();
  NSUInteger bitmask = 0;
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    bitmask |= 1;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    bitmask |= 1 << 1;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    bitmask |= 1 << 2;
  return bitmask;
}

@end

namespace {

NSPoint ConvertRootPointToTarget(NSWindow* target,
                                 const gfx::Point& point_in_root) {
  // Normally this would do [NSWindow convertScreenToBase:]. However, Cocoa can
  // reposition the window on screen and make things flaky. Initially, just
  // assume that the contentRect of |target| is at the top-left corner of the
  // screen.
  NSRect content_rect = [target contentRectForFrameRect:[target frame]];
  return NSMakePoint(point_in_root.x(),
                     NSHeight(content_rect) - point_in_root.y());
}

// Inverse of ui::EventFlagsFromModifiers().
NSUInteger EventFlagsToModifiers(int flags) {
  NSUInteger modifiers = 0;
  modifiers |= (flags & ui::EF_CAPS_LOCK_DOWN) ? NSAlphaShiftKeyMask : 0;
  modifiers |= (flags & ui::EF_SHIFT_DOWN) ? NSShiftKeyMask : 0;
  modifiers |= (flags & ui::EF_CONTROL_DOWN) ? NSControlKeyMask : 0;
  modifiers |= (flags & ui::EF_ALT_DOWN) ? NSAlternateKeyMask : 0;
  modifiers |= (flags & ui::EF_COMMAND_DOWN) ? NSCommandKeyMask : 0;
  // ui::EF_*_MOUSE_BUTTON not handled here.
  // NSFunctionKeyMask, NSNumericPadKeyMask and NSHelpKeyMask not mapped.
  return modifiers;
}

// Picks the corresponding mouse event type for the buttons set in |flags|.
NSEventType PickMouseEventType(int flags,
                               NSEventType left,
                               NSEventType right,
                               NSEventType other) {
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    return left;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    return right;
  return other;
}

// Inverse of ui::EventTypeFromNative(). If non-null |modifiers| will be set
// using the inverse of ui::EventFlagsFromNSEventWithModifiers().
NSEventType EventTypeToNative(ui::EventType ui_event_type,
                              int flags,
                              NSUInteger* modifiers) {
  if (modifiers)
    *modifiers = EventFlagsToModifiers(flags);
  switch (ui_event_type) {
    case ui::ET_UNKNOWN:
      return 0;
    case ui::ET_KEY_PRESSED:
      return NSKeyDown;
    case ui::ET_KEY_RELEASED:
      return NSKeyUp;
    case ui::ET_MOUSE_PRESSED:
      return PickMouseEventType(flags,
                                NSLeftMouseDown,
                                NSRightMouseDown,
                                NSOtherMouseDown);
    case ui::ET_MOUSE_RELEASED:
      return PickMouseEventType(flags,
                                NSLeftMouseUp,
                                NSRightMouseUp,
                                NSOtherMouseUp);
    case ui::ET_MOUSE_DRAGGED:
      return PickMouseEventType(flags,
                                NSLeftMouseDragged,
                                NSRightMouseDragged,
                                NSOtherMouseDragged);
    case ui::ET_MOUSE_MOVED:
      return NSMouseMoved;
    case ui::ET_MOUSEWHEEL:
      return NSScrollWheel;
    case ui::ET_MOUSE_ENTERED:
      return NSMouseEntered;
    case ui::ET_MOUSE_EXITED:
      return NSMouseExited;
    case ui::ET_SCROLL_FLING_START:
      return NSEventTypeSwipe;
    default:
      NOTREACHED();
      return 0;
  }
}

// Emulate the dispatching that would be performed by -[NSWindow sendEvent:].
// sendEvent is a black box which (among other things) will try to peek at the
// event queue and can block indefinitely.
void EmulateSendEvent(NSWindow* window, NSEvent* event) {
  NSResponder* responder = [window firstResponder];
  switch ([event type]) {
    case NSKeyDown:
      [responder keyDown:event];
      return;
    case NSKeyUp:
      [responder keyUp:event];
      return;
  }

  // For mouse events, NSWindow will use -[NSView hitTest:] for the initial
  // mouseDown, and then keep track of the NSView returned. The toolkit-views
  // RootView does this too. So, for tests, assume tracking will be done there,
  // and the NSWindow's contentView is wrapping a views::internal::RootView.
  responder = [window contentView];
  switch ([event type]) {
    case NSLeftMouseDown:
      [responder mouseDown:event];
      break;
    case NSRightMouseDown:
      [responder rightMouseDown:event];
      break;
    case NSOtherMouseDown:
      [responder otherMouseDown:event];
      break;
    case NSLeftMouseUp:
      [responder mouseUp:event];
      break;
    case NSRightMouseUp:
      [responder rightMouseUp:event];
      break;
    case NSOtherMouseUp:
      [responder otherMouseUp:event];
      break;
    case NSLeftMouseDragged:
      [responder mouseDragged:event];
      break;
    case NSRightMouseDragged:
      [responder rightMouseDragged:event];
      break;
    case NSOtherMouseDragged:
      [responder otherMouseDragged:event];
      break;
    case NSMouseMoved:
      // Assumes [NSWindow acceptsMouseMovedEvents] would return YES, and that
      // NSTrackingAreas have been appropriately installed on |responder|.
      [responder mouseMoved:event];
      break;
    case NSScrollWheel:
      [responder scrollWheel:event];
      break;
    case NSMouseEntered:
    case NSMouseExited:
      // With the assumptions in NSMouseMoved, it doesn't make sense for the
      // generator to handle entered/exited separately. It's the responsibility
      // of views::internal::RootView to convert the moved events into entered
      // and exited events for the individual views.
      NOTREACHED();
      break;
    case NSEventTypeSwipe:
      // NSEventTypeSwipe events can't be generated using public interfaces on
      // NSEvent, so this will need to be handled at a higher level.
      NOTREACHED();
      break;
    default:
      NOTREACHED();
  }
}

void DispatchMouseEventInWindow(NSWindow* window,
                                ui::EventType event_type,
                                const gfx::Point& point_in_root,
                                int flags) {
  NSUInteger click_count = 0;
  if (event_type == ui::ET_MOUSE_PRESSED ||
      event_type == ui::ET_MOUSE_RELEASED) {
    if (flags & ui::EF_IS_TRIPLE_CLICK)
      click_count = 3;
    else if (flags & ui::EF_IS_DOUBLE_CLICK)
      click_count = 2;
    else
      click_count = 1;
  }
  NSPoint point = ConvertRootPointToTarget(window, point_in_root);
  NSUInteger modifiers = 0;
  NSEventType type = EventTypeToNative(event_type, flags, &modifiers);
  NSEvent* event = [NSEvent mouseEventWithType:type
                                      location:point
                                 modifierFlags:modifiers
                                     timestamp:0
                                  windowNumber:[window windowNumber]
                                       context:nil
                                   eventNumber:0
                                    clickCount:click_count
                                      pressure:1.0];

  // Typically events go through NSApplication. For tests, dispatch the event
  // directly to make things more predicatble.
  EmulateSendEvent(window, event);
}

// Implementation of ui::test::EventGeneratorDelegate for Mac. Everything
// defined inline is just a stub. Interesting overrides are defined below the
// class.
class EventGeneratorDelegateMac : public ui::EventTarget,
                                  public ui::EventSource,
                                  public ui::EventProcessor,
                                  public ui::EventTargeter,
                                  public ui::test::EventGeneratorDelegate {
 public:
  EventGeneratorDelegateMac(ui::test::EventGenerator* owner, NSWindow* window);
  virtual ~EventGeneratorDelegateMac() {}

  // Overridden from ui::EventTarget:
  virtual bool CanAcceptEvent(const ui::Event& event) OVERRIDE { return true; }
  virtual ui::EventTarget* GetParentTarget()  OVERRIDE { return NULL; }
  virtual scoped_ptr<ui::EventTargetIterator> GetChildIterator() const OVERRIDE;
  virtual ui::EventTargeter* GetEventTargeter() OVERRIDE {
    return this;
  }

  // Overridden from ui::EventHandler (via ui::EventTarget):
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // Overridden from ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE { return this; }

  // Overridden from ui::EventProcessor:
  virtual ui::EventTarget* GetRootTarget() OVERRIDE { return this; }

  // Overridden from ui::EventDispatcherDelegate (via ui::EventProcessor):
  virtual bool CanDispatchToTarget(EventTarget* target) OVERRIDE {
    return true;
  }

  // Overridden from ui::test::EventGeneratorDelegate:
  virtual ui::EventTarget* GetTargetAt(const gfx::Point& location) OVERRIDE {
    return this;
  }
  virtual ui::EventSource* GetEventSource(ui::EventTarget* target) OVERRIDE {
    return this;
  }
  virtual gfx::Point CenterOfTarget(
      const ui::EventTarget* target) const OVERRIDE;
  virtual gfx::Point CenterOfWindow(gfx::NativeWindow window) const OVERRIDE;

  virtual void ConvertPointFromTarget(const ui::EventTarget* target,
                                      gfx::Point* point) const OVERRIDE {}
  virtual void ConvertPointToTarget(const ui::EventTarget* target,
                                    gfx::Point* point) const OVERRIDE {}
  virtual void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                                    gfx::Point* point) const OVERRIDE {}

 private:
  ui::test::EventGenerator* owner_;
  NSWindow* window_;
  ScopedClassSwizzler swizzle_pressed_;

  DISALLOW_COPY_AND_ASSIGN(EventGeneratorDelegateMac);
};

EventGeneratorDelegateMac::EventGeneratorDelegateMac(
    ui::test::EventGenerator* owner, NSWindow* window)
    : owner_(owner),
      window_(window),
      swizzle_pressed_([NSEvent class],
                       [NSEventDonor class],
                       @selector(pressedMouseButtons)) {
}

scoped_ptr<ui::EventTargetIterator>
EventGeneratorDelegateMac::GetChildIterator() const {
  // Return NULL to dispatch all events to the result of GetRootTarget().
  return scoped_ptr<ui::EventTargetIterator>();
}

void EventGeneratorDelegateMac::OnMouseEvent(ui::MouseEvent* event) {
  // For mouse drag events, ensure the swizzled methods return the right flags.
  base::AutoReset<ui::test::EventGenerator*> reset(&g_active_generator, owner_);
  DispatchMouseEventInWindow(window_,
                             event->type(),
                             event->location(),
                             event->changed_button_flags());
}

gfx::Point EventGeneratorDelegateMac::CenterOfTarget(
    const ui::EventTarget* target) const {
  DCHECK_EQ(target, this);
  return CenterOfWindow(window_);
}

gfx::Point EventGeneratorDelegateMac::CenterOfWindow(
    gfx::NativeWindow window) const {
  DCHECK_EQ(window, window_);
  return gfx::ScreenRectFromNSRect([window frame]).CenterPoint();
}

}  // namespace

namespace ui {
namespace test {

// static
EventGeneratorDelegate* EventGenerator::CreateDefaultPlatformDelegate(
    EventGenerator* owner,
    gfx::NativeWindow root_window,
    gfx::NativeWindow window) {
  return new EventGeneratorDelegateMac(owner, window);
}

}  // namespace test
}  // namespace ui
