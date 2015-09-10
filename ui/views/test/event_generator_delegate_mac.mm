// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/memory/singleton.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_targeter.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace {

// Set (and always cleared) in EmulateSendEvent() to provide an answer for
// [NSApp currentEvent].
NSEvent* g_current_event = nil;

}  // namespace

@interface NSEventDonor : NSObject
@end

@interface NSApplicationDonor : NSObject
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
      return NSApplicationDefined;
  }
}

// Emulate the dispatching that would be performed by -[NSWindow sendEvent:].
// sendEvent is a black box which (among other things) will try to peek at the
// event queue and can block indefinitely.
void EmulateSendEvent(NSWindow* window, NSEvent* event) {
  base::AutoReset<NSEvent*> reset(&g_current_event, event);
  NSResponder* responder = [window firstResponder];
  switch ([event type]) {
    case NSKeyDown:
      [responder keyDown:event];
      return;
    case NSKeyUp:
      [responder keyUp:event];
      return;
    default:
      break;
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

NSEvent* CreateMouseEventInWindow(NSWindow* window,
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
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifiers
                           timestamp:0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:click_count
                            pressure:1.0];
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
  static EventGeneratorDelegateMac* GetInstance() {
    return base::Singleton<EventGeneratorDelegateMac>::get();
  }

  IMP CurrentEventMethod() {
    return swizzle_current_event_->GetOriginalImplementation();
  }

  NSWindow* window() { return window_.get(); }
  ui::test::EventGenerator* owner() { return owner_; }

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override { return true; }
  ui::EventTarget* GetParentTarget() override { return nullptr; }
  scoped_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override { return this; }

  // Overridden from ui::EventHandler (via ui::EventTarget):
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Overridden from ui::EventSource:
  ui::EventProcessor* GetEventProcessor() override { return this; }

  // Overridden from ui::EventProcessor:
  ui::EventTarget* GetRootTarget() override { return this; }

  // Overridden from ui::EventDispatcherDelegate (via ui::EventProcessor):
  bool CanDispatchToTarget(EventTarget* target) override { return true; }

  // Overridden from ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    return root;
  }
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override {
    return nullptr;
  }

  // Overridden from ui::test::EventGeneratorDelegate:
  void SetContext(ui::test::EventGenerator* owner,
                  gfx::NativeWindow root_window,
                  gfx::NativeWindow window) override;
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override {
    return this;
  }
  ui::EventSource* GetEventSource(ui::EventTarget* target) override {
    return this;
  }
  gfx::Point CenterOfTarget(const ui::EventTarget* target) const override;
  gfx::Point CenterOfWindow(gfx::NativeWindow window) const override;

  void ConvertPointFromTarget(const ui::EventTarget* target,
                              gfx::Point* point) const override {}
  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override {}
  void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                            gfx::Point* point) const override {}

 private:
  friend struct base::DefaultSingletonTraits<EventGeneratorDelegateMac>;

  EventGeneratorDelegateMac();
  ~EventGeneratorDelegateMac() override;

  ui::test::EventGenerator* owner_;
  base::scoped_nsobject<NSWindow> window_;
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> swizzle_pressed_;
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> swizzle_location_;
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> swizzle_current_event_;
  base::scoped_nsobject<NSMenu> fake_menu_;

  DISALLOW_COPY_AND_ASSIGN(EventGeneratorDelegateMac);
};

EventGeneratorDelegateMac::EventGeneratorDelegateMac() : owner_(nullptr) {
  DCHECK(!ui::test::EventGenerator::default_delegate);
  ui::test::EventGenerator::default_delegate = this;
  // Install a fake "edit" menu. This is normally provided by Chrome's
  // MainMenu.xib, but src/ui shouldn't depend on that.
  fake_menu_.reset([[NSMenu alloc] initWithTitle:@"Edit"]);
  struct {
    NSString* title;
    SEL action;
    NSString* key_equivalent;
  } fake_menu_item[] = {
      {@"Undo", @selector(undo:), @"z"},
      {@"Redo", @selector(redo:), @"Z"},
      {@"Copy", @selector(copy:), @"c"},
      {@"Cut", @selector(cut:), @"x"},
      {@"Paste", @selector(paste:), @"v"},
      {@"Select All", @selector(selectAll:), @"a"},
  };
  for (size_t i = 0; i < arraysize(fake_menu_item); ++i) {
    [fake_menu_ insertItemWithTitle:fake_menu_item[i].title
                             action:fake_menu_item[i].action
                      keyEquivalent:fake_menu_item[i].key_equivalent
                            atIndex:i];
  }
}

EventGeneratorDelegateMac::~EventGeneratorDelegateMac() {
  DCHECK_EQ(this, ui::test::EventGenerator::default_delegate);
  ui::test::EventGenerator::default_delegate = nullptr;
}

scoped_ptr<ui::EventTargetIterator>
EventGeneratorDelegateMac::GetChildIterator() const {
  // Return nullptr to dispatch all events to the result of GetRootTarget().
  return nullptr;
}

void EventGeneratorDelegateMac::OnMouseEvent(ui::MouseEvent* event) {
  NSEvent* ns_event = CreateMouseEventInWindow(window_,
                                               event->type(),
                                               event->location(),
                                               event->changed_button_flags());
  if (owner_->targeting_application())
    [NSApp sendEvent:ns_event];
  else
    EmulateSendEvent(window_, ns_event);
}

void EventGeneratorDelegateMac::OnKeyEvent(ui::KeyEvent* event) {
  NSUInteger modifiers = EventFlagsToModifiers(event->flags());
  NSEvent* ns_event = cocoa_test_event_utils::SynthesizeKeyEvent(
      window_, event->type() == ui::ET_KEY_PRESSED, event->key_code(),
      modifiers);
  if (owner_->targeting_application()) {
    [NSApp sendEvent:ns_event];
    return;
  }

  if ([fake_menu_ performKeyEquivalent:ns_event])
    return;

  EmulateSendEvent(window_, ns_event);
}

void EventGeneratorDelegateMac::OnTouchEvent(ui::TouchEvent* event) {
  NOTREACHED() << "Touchscreen events not supported on Chrome Mac.";
}

void EventGeneratorDelegateMac::SetContext(ui::test::EventGenerator* owner,
                                           gfx::NativeWindow root_window,
                                           gfx::NativeWindow window) {
  swizzle_pressed_.reset();
  swizzle_location_.reset();
  swizzle_current_event_.reset();
  owner_ = owner;

  // Retain the NSWindow (note it can be nil). This matches Cocoa's tendency to
  // have autoreleased objects, or objects still in the event queue, that
  // reference the NSWindow.
  window_.reset([window retain]);

  // Normally, edit menu items have a `nil` target. This results in -[NSMenu
  // performKeyEquivalent:] relying on -[NSApplication targetForAction:to:from:]
  // to find a target starting at the first responder of the key window. Since
  // non-interactive tests have no key window, that won't work. So set (or
  // clear) the target explicitly on all menu items.
  [[fake_menu_ itemArray] makeObjectsPerformSelector:@selector(setTarget:)
                                          withObject:[window firstResponder]];

  if (owner_) {
    swizzle_pressed_.reset(new base::mac::ScopedObjCClassSwizzler(
        [NSEvent class],
        [NSEventDonor class],
        @selector(pressedMouseButtons)));
    swizzle_location_.reset(new base::mac::ScopedObjCClassSwizzler(
        [NSEvent class], [NSEventDonor class], @selector(mouseLocation)));
    swizzle_current_event_.reset(new base::mac::ScopedObjCClassSwizzler(
        [NSApplication class],
        [NSApplicationDonor class],
        @selector(currentEvent)));
  }
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

// Return the current owner of the EventGeneratorDelegate. May be null.
ui::test::EventGenerator* GetActiveGenerator() {
  return EventGeneratorDelegateMac::GetInstance()->owner();
}

}  // namespace

namespace views {
namespace test {

void InitializeMacEventGeneratorDelegate() {
  EventGeneratorDelegateMac::GetInstance();
}

}  // namespace test
}  // namespace views

@implementation NSEventDonor

// Donate +[NSEvent pressedMouseButtons] by retrieving the flags from the
// active generator.
+ (NSUInteger)pressedMouseButtons {
  ui::test::EventGenerator* generator = GetActiveGenerator();
  if (!generator)
    return [NSEventDonor pressedMouseButtons];  // Call original implementation.

  int flags = generator->flags();
  NSUInteger bitmask = 0;
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    bitmask |= 1;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    bitmask |= 1 << 1;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    bitmask |= 1 << 2;
  return bitmask;
}

// Donate +[NSEvent mouseLocation] by retrieving the current position on screen.
+ (NSPoint)mouseLocation {
  ui::test::EventGenerator* generator = GetActiveGenerator();
  if (!generator)
    return [NSEventDonor mouseLocation];  // Call original implementation.

  // The location is the point in the root window which, for desktop widgets, is
  // the widget itself.
  gfx::Point point_in_root = generator->current_location();
  NSWindow* window = EventGeneratorDelegateMac::GetInstance()->window();
  NSPoint point_in_window = ConvertRootPointToTarget(window, point_in_root);
  return [window convertBaseToScreen:point_in_window];
}

@end

@implementation NSApplicationDonor

- (NSEvent*)currentEvent {
  if (g_current_event)
    return g_current_event;

  // Find the original implementation and invoke it.
  IMP original = EventGeneratorDelegateMac::GetInstance()->CurrentEventMethod();
  return original(self, _cmd);
}

@end
