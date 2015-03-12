// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/ios/cru_context_menu_holder.h"

#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@implementation CRUContextMenuHolder {
  // Backs the property of the same name.
  base::scoped_nsobject<NSString> _menuTitle;
  // Stores the itemTitles of the menu items.
  base::scoped_nsobject<NSMutableArray> _itemTitles;
  // Keep a copy of all the actions blocks.
  base::scoped_nsobject<NSMutableArray> _actions;
  // A set of indices in |_actions| that should cause the context menu to be
  // dismissed without animation when tapped.
  base::scoped_nsobject<NSMutableIndexSet> _dismissImmediatelyActions;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _itemTitles.reset([[NSMutableArray alloc] init]);
    _actions.reset([[NSMutableArray alloc] init]);
    _dismissImmediatelyActions.reset([[NSMutableIndexSet alloc] init]);
  }
  return self;
}

- (NSString*)menuTitle {
  return _menuTitle;
}

- (void)setMenuTitle:(NSString*)newTitle {
  _menuTitle.reset([newTitle copy]);
}

- (NSArray*)itemTitles {
  return [[_itemTitles copy] autorelease];
}

- (NSUInteger)itemCount {
  return [_itemTitles count];
}

- (void)appendItemWithTitle:(NSString*)title action:(ProceduralBlock)action {
  [self appendItemWithTitle:title action:action dismissImmediately:NO];
}

- (void)appendItemWithTitle:(NSString*)title
                     action:(ProceduralBlock)action
         dismissImmediately:(BOOL)dismissImmediately {
  DCHECK(title && action);
  [_itemTitles addObject:title];
  // The action block needs to receive the copy message in order to freeze the
  // stack data it uses. Only then can it be safely retained by the array.
  base::scoped_nsprotocol<id> heapAction([action copy]);
  [_actions addObject:heapAction];
  if (dismissImmediately) {
    [_dismissImmediatelyActions addIndex:[_actions count] - 1];
  }
}

- (void)performActionAtIndex:(NSUInteger)index {
  static_cast<ProceduralBlock>([_actions objectAtIndex:index])();
}

- (BOOL)shouldDismissImmediatelyOnClickedAtIndex:(NSUInteger)index {
  return [_dismissImmediatelyActions containsIndex:index];
}

@end
