// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"

namespace bookmarks {
class BookmarkModel;
}
@protocol ApplicationCommands;
@protocol BrowserCommands;
class BrowserPolicyConnectorIOS;
@class OverflowMenuModel;
class OverlayPresenter;
class PrefService;
@protocol FindInPageCommands;
@protocol TextZoomCommands;
class WebNavigationBrowserAgent;
class WebStateList;

// Mediator for the overflow menu. This object is in charge of creating and
// updating the items of the overflow menu.
@interface OverflowMenuMediator : NSObject <BrowserContainerConsumer>

// The data model for the overflow menu.
@property(nonatomic, readonly) OverflowMenuModel* overflowMenuModel;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              FindInPageCommands,
                              TextZoomCommands>
    dispatcher;

// Navigation agent for reloading pages.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationAgent;

// If the current session is off the record or not.
@property(nonatomic, assign) bool isIncognito;

// BaseViewController for presenting some UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// The bookmarks model to know if the page is bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

// The overlay presenter for OverlayModality::kWebContentArea.  This mediator
// listens for overlay presentation events to determine whether the "Add to
// Reading List" button should be enabled.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;

// The current browser policy connector.
@property(nonatomic, assign) BrowserPolicyConnectorIOS* browserPolicyConnector;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
