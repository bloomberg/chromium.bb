// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_BASE_IOS_CRU_CONTEXT_MENU_CONTROLLER_H_
#define UI_BASE_IOS_CRU_CONTEXT_MENU_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class CRUContextMenuHolder;

// Abstracts displaying context menus for all device form factors, given a
// CRUContextMenuHolder with the title and action to associate to each menu
// item. Will show a sheet on the phone and use a popover on a tablet.
@interface CRUContextMenuController : NSObject

// Whether the context menu is visible.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;

// Displays a context menu. If on a tablet, |localPoint| is the point in
// |view|'s coordinates to show the popup. If a phone, |localPoint| is unused
// since the display is a sheet, but |view| is still used to attach the sheet to
// the given view.
// The |menuHolder| that will be put in the menu.
- (void)showWithHolder:(CRUContextMenuHolder*)menuHolder
               atPoint:(CGPoint)localPoint
                inView:(UIView*)view;

@end

#endif  // UI_BASE_IOS_CRU_CONTEXT_MENU_CONTROLLER_H_
