// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/ui/ntp/logo_animation_controller.h"

namespace web {
class WebState;
}

@class BubblePresenter;
@protocol NewTabPageControllerDelegate;
@protocol ThumbStripSupporting;
@class ViewRevealingVerticalPanHandler;

// Coordinator handling the NTP.
@interface NewTabPageCoordinator
    : ChromeCoordinator <LogoAnimationControllerOwnerOwner>

// ViewController associated with this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// Webstate associated with this coordinator.
@property(nonatomic, assign) web::WebState* webState;

// The toolbar delegate to pass to ContentSuggestionsCoordinator.
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

// Returns |YES| if the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// The pan gesture handler for the view controller.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// Allows for the in-flight enabling/disabling of the thumb strip.
@property(nonatomic, weak, readonly) id<ThumbStripSupporting>
    thumbStripSupporting;

// Exposes content inset of contentSuggestions collectionView to ensure all of
// content is visible under the bottom toolbar.
@property(nonatomic, readonly) UIEdgeInsets contentInset;

// Bubble presenter for displaying IPH bubbles relating to the NTP.
@property(nonatomic, strong) BubblePresenter* bubblePresenter;

// Dismisses all modals owned by the NTP.
- (void)dismissModals;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Stop any scrolling in the scroll view.
- (void)stopScrolling;

// The content offset of the scroll view.
- (CGPoint)contentOffset;

// Reloads the content of the NewTabPage. Does not do anything on Incognito.
- (void)reload;

// Calls when the visibility of the NTP changes.
- (void)ntpDidChangeVisibility:(BOOL)visible;

// The location bar has lost focus.
- (void)locationBarDidResignFirstResponder;

// Tell location bar has taken focus.
- (void)locationBarDidBecomeFirstResponder;

// Constrains the named layout guide for the Discover header menu button.
- (void)constrainDiscoverHeaderMenuButtonNamedGuide;

// TODO(crbug.com/1200303):Remove this method once we stop starting/stopping the
// Coordinator when turning the feed on/off.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_
