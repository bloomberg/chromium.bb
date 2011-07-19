// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "webkit/plugins/npapi/plugin_instance.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_5
@interface NSMenu (SnowLeopardMenuPopUpDeclaration)
- (BOOL)popUpMenuPositioningItem:(NSMenuItem*)item
                      atLocation:(NSPoint)location
                          inView:(NSView*)view;
@end
#endif

namespace webkit {
namespace npapi {

namespace {

// Returns an autoreleased NSEvent constructed from the given np_event,
// targeting the given window.
NSEvent* NSEventForNPCocoaEvent(NPCocoaEvent* np_event, NSWindow* window) {
  bool mouse_down = 1;
  switch (np_event->type) {
    case NPCocoaEventMouseDown:
      mouse_down = 1;
      break;
    case NPCocoaEventMouseUp:
      mouse_down = 0;
      break;
    default:
      // If plugins start bringing up context menus for things other than
      // clicks, this will need more plumbing; for now just log it and proceed
      // as if it were a mouse down.
      NOTREACHED();
  }
  NSEventType event_type = NSLeftMouseDown;
  switch (np_event->data.mouse.buttonNumber) {
    case 0:
      event_type = mouse_down ? NSLeftMouseDown : NSLeftMouseUp;
      break;
    case 1:
      event_type = mouse_down ? NSRightMouseDown : NSRightMouseUp;
      break;
    default:
      event_type = mouse_down ? NSOtherMouseDown : NSOtherMouseUp;
      break;
  }

  NSInteger click_count = np_event->data.mouse.clickCount;
  NSInteger modifiers = np_event->data.mouse.modifierFlags;
  // NPCocoaEvent doesn't have a timestamp, so just use the current time.
  NSEvent* event =
      [NSEvent mouseEventWithType:event_type
                         location:NSMakePoint(0, 0)
                    modifierFlags:modifiers
                        timestamp:[[NSApp currentEvent] timestamp]
                     windowNumber:[window windowNumber]
                          context:[NSGraphicsContext currentContext]
                      eventNumber:0
                       clickCount:click_count
                         pressure:1.0];
  return event;
}

}  // namespace

NPError PluginInstance::PopUpContextMenu(NPMenu* menu) {
  if (!currently_handled_event_)
    return NPERR_GENERIC_ERROR;

  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());
  NSPoint screen_point = NSMakePoint(
      plugin_origin_.x() + currently_handled_event_->data.mouse.pluginX,
      plugin_origin_.y() + currently_handled_event_->data.mouse.pluginY);
  // Plugin offsets are upper-left based, so flip vertically for Cocoa.
  screen_point.y = main_display_bounds.size.height - screen_point.y;

  NSMenu* nsmenu = reinterpret_cast<NSMenu*>(menu);
  NPError return_val = NPERR_NO_ERROR;
  NSWindow* window = nil;
  @try {
    if ([nsmenu respondsToSelector:
           @selector(popUpMenuPositioningItem:atLocation:inView:)]) {
      [nsmenu popUpMenuPositioningItem:nil atLocation:screen_point inView:nil];
    } else {
      NSRect dummy_window_rect = NSMakeRect(screen_point.x, screen_point.y,
                                            1, 1);
      window = [[NSWindow alloc] initWithContentRect:dummy_window_rect
                                           styleMask:NSBorderlessWindowMask
                                             backing:NSBackingStoreNonretained
                                               defer:YES];
      [window setTitle:@"PopupMenuDummy"];  // Lets interposing identify it.
      [window setAlphaValue:0];
      [window makeKeyAndOrderFront:nil];
      [NSMenu popUpContextMenu:nsmenu
                     withEvent:NSEventForNPCocoaEvent(currently_handled_event_,
                                                      window)
                       forView:[window contentView]];
    }
  }
  @catch (NSException* e) {
    NSLog(@"Caught exception while handling PopUpContextMenu: %@", e);
    return_val = NPERR_GENERIC_ERROR;
  }

  if (window) {
    @try {
      [window orderOut:nil];
      [window release];
    }
    @catch (NSException* e) {
      NSLog(@"Caught exception while cleaning up in PopUpContextMenu: %@", e);
    }
  }

  return return_val;
}

ScopedCurrentPluginEvent::ScopedCurrentPluginEvent(PluginInstance* instance,
                                                   NPCocoaEvent* event)
    : instance_(instance) {
  instance_->set_currently_handled_event(event);
}

ScopedCurrentPluginEvent::~ScopedCurrentPluginEvent() {
  instance_->set_currently_handled_event(NULL);
}

}  // namespace npapi
}  // namespace webkit
