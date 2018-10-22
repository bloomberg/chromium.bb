// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

@class CommandDispatcher;
@protocol OmniboxPopupPositioner;
class OmniboxPopupViewIOS;

namespace ios {
class ChromeBrowserState;
}

// Coordinator for the Omnibox Popup.
@interface OmniboxPopupCoordinator : NSObject

- (instancetype)initWithPopupView:
    (std::unique_ptr<OmniboxPopupViewIOS>)popupView NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Positioner for the popup.
@property(nonatomic, weak) id<OmniboxPopupPositioner> positioner;
// Whether this coordinator has results to show.
@property(nonatomic, assign, readonly) BOOL hasResults;
// Whether the popup is open.
@property(nonatomic, assign, readonly) BOOL isOpen;
// The dispatcher for this view controller.
@property(nonatomic, readwrite, weak) CommandDispatcher* dispatcher;

- (void)start;
- (void)stop;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
