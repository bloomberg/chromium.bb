// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

@protocol ActivityServicePositioner;
class Browser;
@class BrowserContainerViewController;
@class BrowserViewControllerDependencyFactory;
@class ToolbarAccessoryPresenter;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <LogoAnimationControllerOwnerOwner,
                        FindBarPresentationDelegate,
                        PageInfoPresentation,
                        SyncPresenter,
                        ToolbarCoordinatorDelegate>

// Initializes a new BVC from its nib. |model| must not be nil. The
// webUsageSuspended property for this BVC will be based on |model|, and future
// changes to |model|'s suspension state should be made through this BVC
// instead of directly on the model.
// TODO(crbug.com/992582): Remove references to model objects from this class.
- (instancetype)initWithBrowser:(Browser*)browser
                 dependencyFactory:
                     (BrowserViewControllerDependencyFactory*)factory
    browserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns whether or not text to speech is playing.
@property(nonatomic, assign, readonly, getter=isPlayingTTS) BOOL playingTTS;

// The container used for infobar banner overlays.
@property(nonatomic, strong)
    UIViewController* infobarBannerOverlayContainerViewController;

// The container used for infobar modal overlays.
@property(nonatomic, strong)
    UIViewController* infobarModalOverlayContainerViewController;

// Presenter used to display accessories over the toolbar (e.g. Find In Page).
@property(nonatomic, strong)
    ToolbarAccessoryPresenter* toolbarAccessoryPresenter;

// Positioner for activity services attached to the toolbar.
@property(nonatomic, readonly) id<ActivityServicePositioner>
    activityServicePositioner;

// Whether the receiver is currently the primary BVC.
- (void)setPrimary:(BOOL)primary;

// Called when the user explicitly opens the tab switcher.
- (void)userEnteredTabSwitcher;

// Opens a new tab as if originating from |originPoint| and |focusOmnibox|.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox;

// Adds |tabAddedCompletion| to the completion block (if any) that will be run
// the next time a tab is added to the TabModel this object was initialized
// with.
- (void)appendTabAddedCompletion:(ProceduralBlock)tabAddedCompletion;

// Informs the BVC that a new foreground tab is about to be opened. This is
// intended to be called before setWebUsageSuspended:NO in cases where a new tab
// is about to appear in order to allow the BVC to avoid doing unnecessary work
// related to showing the previously selected tab.
- (void)expectNewForegroundTab;

// Shows the voice search UI.
- (void)startVoiceSearch;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
