// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBMENURUNNER_MAC_H_
#define WEBKIT_GLUE_WEBMENURUNNER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/memory/scoped_nsobject.h"
#include "webkit/glue/webkit_glue_export.h"
#include "webkit/glue/webmenuitem.h"


// WebMenuRunner ---------------------------------------------------------------
// A class for determining whether an item was selected from an HTML select
// control, or if the menu was dismissed without making a selection. If a menu
// item is selected, MenuDelegate is informed and sets a flag which can be
// queried after the menu has finished running.

WEBKIT_GLUE_EXPORT
@interface WebMenuRunner : NSObject {
 @private
  // The native menu control.
  scoped_nsobject<NSMenu> menu_;

  // A flag set to YES if a menu item was chosen, or NO if the menu was
  // dismissed without selecting an item.
  BOOL menuItemWasChosen_;

  // The index of the selected menu item.
  int index_;

  // The font size being used for the menu.
  CGFloat fontSize_;

  // Whether the menu should be displayed right-aligned.
  BOOL rightAligned_;
}

// Initializes the MenuDelegate with a list of items sent from WebKit.
- (id)initWithItems:(const std::vector<WebMenuItem>&)items
           fontSize:(CGFloat)fontSize
       rightAligned:(BOOL)rightAligned;

// Returns YES if an item was selected from the menu, NO if the menu was
// dismissed.
- (BOOL)menuItemWasChosen;

// Displays and runs a native popup menu.
- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index;

// Returns the index of selected menu item, or its initial value (-1) if no item
// was selected.
- (int)indexOfSelectedItem;

@end  // @interface WebMenuRunner

#endif // WEBKIT_GLUE_WEBMENURUNNER_MAC_H_
