// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_LANGUAGE_SELECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_LANGUAGE_SELECTION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ContainedPresenter;
class WebStateList;

// A coordinator for a language selection UI. This is intended for display as
// a contained, not presented, view controller.
// The methods defined in the LanguageSelectionHandler protocol will be called
// to start the coordinator.
@interface LanguageSelectionCoordinator : ChromeCoordinator

// Creates a coordinator that uses a |viewController| a |browserState| and
// a |webStateList|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList;

// Unavailable, use -initWithBaseViewController:browserState:webStateList:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_LANGUAGE_SELECTION_COORDINATOR_H_
