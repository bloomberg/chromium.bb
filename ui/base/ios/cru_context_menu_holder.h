// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IOS_CRU_CONTEXT_MENU_HOLDER_H_
#define UI_BASE_IOS_CRU_CONTEXT_MENU_HOLDER_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

// This class simply stores the information necessary to build a menu:
// a menu title, a list of item titles and associated actions (blocks).
@interface CRUContextMenuHolder : NSObject

// Designated initializer.
- (instancetype)init;

// Menu title; can be nil.
@property(nonatomic, copy) NSString* menuTitle;

// A list of menu item titles in the order they will appear in the menu.
@property(nonatomic, readonly, copy) NSArray* itemTitles;

// A number of menu items.
@property(nonatomic, readonly) NSUInteger itemCount;

// Adds an item at the end of the menu.
- (void)appendItemWithTitle:(NSString*)title action:(ProceduralBlock)action;

// Adds an item at the end of the menu. If |dismissImmediately| is YES,
// then tapping this item will cause the context menu to be dismissed without
// any animation.
- (void)appendItemWithTitle:(NSString*)title
                     action:(ProceduralBlock)action
         dismissImmediately:(BOOL)dismissImmediately;

// Performs the action for the item at the given index.
- (void)performActionAtIndex:(NSUInteger)index;

// Returns YES if the action at |index| should cause the context menu to
// dismiss without any animation.
- (BOOL)shouldDismissImmediatelyOnClickedAtIndex:(NSUInteger)index;

@end

#endif  // UI_BASE_IOS_CRU_CONTEXT_MENU_HOLDER_H_
