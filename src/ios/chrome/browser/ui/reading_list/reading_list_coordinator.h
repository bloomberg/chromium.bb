// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol UrlLoader;

// Coordinator for Reading List, displaying the Reading List when starting.
@interface ReadingListCoordinator : ChromeCoordinator

// Designated initialier for a ReadingListCoordinator that loads reading list
// items using |loader|.
- (nullable instancetype)
initWithBaseViewController:(nullable UIViewController*)viewController
              browserState:(nonnull ios::ChromeBrowserState*)browserState
                    loader:(nullable id<UrlLoader>)loader
    NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)initWithBaseViewController:
    (nullable UIViewController*)viewController NS_UNAVAILABLE;
- (nullable instancetype)
initWithBaseViewController:(nullable UIViewController*)viewController
              browserState:(nullable ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_H_
