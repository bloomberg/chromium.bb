// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_CONTEXT_MENU_READING_LIST_CONTEXT_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_CONTEXT_MENU_READING_LIST_CONTEXT_MENU_COORDINATOR_H_

#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"

@protocol ReadingListContextMenuCommands;
@class ReadingListContextMenuParams;

// Coordinator used for the Reading List context menu.
@interface ReadingListContextMenuCoordinator : ActionSheetCoordinator

// The parameters passed on initialization.
@property(nonatomic, strong, readonly) ReadingListContextMenuParams* params;
// The handler for commands originating from the context menu.
@property(nonatomic, weak) id<ReadingListContextMenuCommands> commandHandler;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                    params:(ReadingListContextMenuParams*)params
    NS_DESIGNATED_INITIALIZER;

// ReadingListContextMenuCoordinator must be created using
// ReadingListContextMenuParams.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message
                                      rect:(CGRect)rect
                                      view:(UIView*)view NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message
                             barButtonItem:(UIBarButtonItem*)barButtonItem
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_CONTEXT_MENU_READING_LIST_CONTEXT_MENU_COORDINATOR_H_
