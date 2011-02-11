// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webmenurunner_mac.h"

#include "base/sys_string_conversions.h"

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
enum {
  NSUserInterfaceLayoutDirectionLeftToRight = 0,
  NSUserInterfaceLayoutDirectionRightToLeft = 1
};
typedef NSInteger NSUserInterfaceLayoutDirection;

@interface NSCell (SnowLeopardSDKDeclarations)
- (void)setUserInterfaceLayoutDirection:
    (NSUserInterfaceLayoutDirection)layoutDirection;
@end

enum {
  NSTextWritingDirectionEmbedding = (0 << 1),
  NSTextWritingDirectionOverride = (1 << 1)
};

static NSString* NSWritingDirectionAttributeName = @"NSWritingDirection";
#endif

@interface WebMenuRunner (PrivateAPI)

// Worker function used during initialization.
- (void)addItem:(const WebMenuItem&)item;

// A callback for the menu controller object to call when an item is selected
// from the menu. This is not called if the menu is dismissed without a
// selection.
- (void)menuItemSelected:(id)sender;

@end  // WebMenuRunner (PrivateAPI)

@implementation WebMenuRunner

- (id)initWithItems:(const std::vector<WebMenuItem>&)items
           fontSize:(CGFloat)fontSize
       rightAligned:(BOOL)rightAligned {
  if ((self = [super init])) {
    menu_.reset([[NSMenu alloc] initWithTitle:@""]);
    [menu_ setAutoenablesItems:NO];
    index_ = -1;
    fontSize_ = fontSize;
    rightAligned_ = rightAligned;
    for (size_t i = 0; i < items.size(); ++i)
      [self addItem:items[i]];
  }
  return self;
}

- (void)addItem:(const WebMenuItem&)item {
  if (item.type == WebMenuItem::SEPARATOR) {
    [menu_ addItem:[NSMenuItem separatorItem]];
    return;
  }

  NSString* title = base::SysUTF16ToNSString(item.label);
  NSMenuItem* menuItem = [menu_ addItemWithTitle:title
                                          action:@selector(menuItemSelected:)
                                   keyEquivalent:@""];
  [menuItem setEnabled:(item.enabled && item.type != WebMenuItem::GROUP)];
  [menuItem setTarget:self];

  // Set various alignment/language attributes. Note that many (if not most) of
  // these attributes are functional only on 10.6 and above.
  scoped_nsobject<NSMutableDictionary> attrs(
      [[NSMutableDictionary alloc] initWithCapacity:3]);
  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:rightAligned_ ? NSRightTextAlignment
                                             : NSLeftTextAlignment];
  NSWritingDirection writingDirection =
      item.rtl ? NSWritingDirectionRightToLeft
               : NSWritingDirectionLeftToRight;
  [paragraphStyle setBaseWritingDirection:writingDirection];
  [attrs setObject:paragraphStyle forKey:NSParagraphStyleAttributeName];

  if (item.has_directional_override) {
    scoped_nsobject<NSNumber> directionValue(
        [[NSNumber alloc] initWithInteger:
            writingDirection + NSTextWritingDirectionOverride]);
    scoped_nsobject<NSArray> directionArray(
        [[NSArray alloc] initWithObjects:directionValue.get(), nil]);
    [attrs setObject:directionArray forKey:NSWritingDirectionAttributeName];
  }

  [attrs setObject:[NSFont menuFontOfSize:fontSize_]
            forKey:NSFontAttributeName];

  scoped_nsobject<NSAttributedString> attrTitle(
      [[NSAttributedString alloc] initWithString:title
                                      attributes:attrs]);
  [menuItem setAttributedTitle:attrTitle];

  [menuItem setTag:[menu_ numberOfItems] - 1];
}

// Reflects the result of the user's interaction with the popup menu. If NO, the
// menu was dismissed without the user choosing an item, which can happen if the
// user clicked outside the menu region or hit the escape key. If YES, the user
// selected an item from the menu.
- (BOOL)menuItemWasChosen {
  return menuItemWasChosen_;
}

- (void)menuItemSelected:(id)sender {
  menuItemWasChosen_ = YES;
}

- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index {
  // Set up the button cell, converting to NSView coordinates. The menu is
  // positioned such that the currently selected menu item appears over the
  // popup button, which is the expected Mac popup menu behavior.
  NSPopUpButtonCell* button = [[NSPopUpButtonCell alloc] initTextCell:@""
                                                            pullsDown:NO];
  [button autorelease];
  [button setMenu:menu_];
  // We use selectItemWithTag below so if the index is out-of-bounds nothing
  // bad happens.
  [button selectItemWithTag:index];

  if (rightAligned_ &&
      [button respondsToSelector:@selector(setUserInterfaceLayoutDirection:)]) {
    [button setUserInterfaceLayoutDirection:
        NSUserInterfaceLayoutDirectionRightToLeft];
  }

  // Create a dummy view to associate the popup with, since the OS will use
  // that view for positioning the menu.
  NSView* dummyView = [[[NSView alloc] initWithFrame:bounds] autorelease];
  [view addSubview:dummyView];
  NSRect dummyBounds = [dummyView convertRect:bounds fromView:view];

  // Display the menu, and set a flag if a menu item was chosen.
  [button performClickWithFrame:dummyBounds inView:dummyView];

  if ([self menuItemWasChosen])
    index_ = [button indexOfSelectedItem];

  [dummyView removeFromSuperview];
}

- (int)indexOfSelectedItem {
  return index_;
}

@end  // WebMenuRunner

namespace webkit_glue {

// Helper function for manufacturing input events to send to WebKit.
NSEvent* EventWithMenuAction(BOOL item_chosen, int window_num,
                             int item_height, int selected_index,
                             NSRect menu_bounds, NSRect view_bounds) {
  NSEvent* event = nil;
  double event_time = (double)(AbsoluteToDuration(UpTime())) / 1000.0;

  if (item_chosen) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    NSPoint click_pos;
    click_pos.x = menu_bounds.size.width / 2;

    // This is going to be hard to calculate since the button is painted by
    // WebKit, the menu by Cocoa, and we have to translate the selected_item
    // index to a coordinate that WebKit's PopupMenu expects which uses a
    // different font *and* expects to draw the menu below the button like we do
    // on Windows.
    // The WebKit popup menu thinks it will draw just below the button, so
    // create the click at the offset based on the selected item's index and
    // account for the different coordinate system used by NSView.
    int item_offset = selected_index * item_height + item_height / 2;
    click_pos.y = view_bounds.size.height - item_offset;
    event = [NSEvent mouseEventWithType:NSLeftMouseUp
                               location:click_pos
                          modifierFlags:0
                              timestamp:event_time
                           windowNumber:window_num
                                context:nil
                            eventNumber:0
                             clickCount:1
                               pressure:1.0];
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    NSPoint key_pos;
    key_pos.x = 0;
    key_pos.y = 0;
    NSString* escape_str = [NSString stringWithFormat:@"%c", 0x1B];
    event = [NSEvent keyEventWithType:NSKeyDown
                             location:key_pos
                        modifierFlags:0
                            timestamp:event_time
                         windowNumber:window_num
                              context:nil
                           characters:@""
          charactersIgnoringModifiers:escape_str
                            isARepeat:NO
                              keyCode:0x1B];
  }

  return event;
}

}  // namespace webkit_glue
