// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webmenurunner_mac.h"

#include "base/strings/sys_string_conversions.h"

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
  if (!item.toolTip.empty()) {
    NSString* toolTip = base::SysUTF16ToNSString(item.toolTip);
    [menuItem setToolTip:toolTip];
  }
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
  scoped_nsobject<NSPopUpButtonCell>
      cell([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
  [cell setMenu:menu_];
  // We use selectItemWithTag below so if the index is out-of-bounds nothing
  // bad happens.
  [cell selectItemWithTag:index];

  if (rightAligned_ &&
      [cell respondsToSelector:@selector(setUserInterfaceLayoutDirection:)]) {
    [cell setUserInterfaceLayoutDirection:
        NSUserInterfaceLayoutDirectionRightToLeft];
  }

  // When popping up a menu near the Dock, Cocoa restricts the menu
  // size to not overlap the Dock, with a scroll arrow.  Below a
  // certain point this doesn't work.  At that point the menu is
  // popped up above the element, so that the current item can be
  // selected without mouse-tracking selecting a different item
  // immediately.
  //
  // Unfortunately, instead of popping up above the passed |bounds|,
  // it pops up above the bounds of the view passed to inView:.  Use a
  // dummy view to fake this out.
  scoped_nsobject<NSView> dummyView([[NSView alloc] initWithFrame:bounds]);
  [view addSubview:dummyView];

  // Display the menu, and set a flag if a menu item was chosen.
  [cell attachPopUpWithFrame:[dummyView bounds] inView:dummyView];
  [cell performClickWithFrame:[dummyView bounds] inView:dummyView];

  [dummyView removeFromSuperview];

  if ([self menuItemWasChosen])
    index_ = [cell indexOfSelectedItem];
}

- (int)indexOfSelectedItem {
  return index_;
}

@end  // WebMenuRunner
