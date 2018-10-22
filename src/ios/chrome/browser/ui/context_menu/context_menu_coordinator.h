// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace web {
struct ContextMenuParams;
}

// Abstracts displaying context menus for all device form factors. Will show a
// sheet on the phone and use a popover on a tablet.
// Once this coordinator is stopped, the underlying alert and any menu items
// which have been added are deleted.
@interface ContextMenuCoordinator : ChromeCoordinator

// Whether the context menu is visible. This will be true after |-start| is
// called until a subsequent |-stop|.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;

// Initializes with details provided in |params|. Context menu will be presented
// from |viewController|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                    params:(const web::ContextMenuParams&)params
    NS_DESIGNATED_INITIALIZER;

// Params are needed for the initialization.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// Adds an item at the end of the menu if |visible| is false.
- (void)addItemWithTitle:(NSString*)title action:(ProceduralBlock)action;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_
