// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"

#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AdaptiveToolbarCoordinator ()

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Mediator for updating the toolbar when the WebState changes.
@property(nonatomic, strong) ToolbarMediator* mediator;
// Actions handler for the toolbar buttons.
@property(nonatomic, strong) ToolbarButtonActionsHandler* actionHandler;

@end

@implementation AdaptiveToolbarCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.started)
    return;

  self.started = YES;

  self.viewController.longPressDelegate = self.longPressDelegate;
  if (@available(iOS 13, *)) {
    self.viewController.overrideUserInterfaceStyle =
        self.browser->GetBrowserState()->IsOffTheRecord()
            ? UIUserInterfaceStyleDark
            : UIUserInterfaceStyleUnspecified;
  }

  self.mediator = [[ToolbarMediator alloc] init];
  self.mediator.incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.bookmarkModel = ios::BookmarkModelFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.prefService = self.browser->GetBrowserState()->GetPrefs();
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Properties

- (void)setLongPressDelegate:(id<PopupMenuLongPressDelegate>)longPressDelegate {
  _longPressDelegate = longPressDelegate;
  self.viewController.longPressDelegate = longPressDelegate;
}

#pragma mark - SideSwipeToolbarSnapshotProviding

- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState {
  [self updateToolbarForSideSwipeSnapshot:webState];

  UIImage* toolbarSnapshot = CaptureViewWithOption(
      [self.viewController view], [[UIScreen mainScreen] scale],
      kClientSideRendering);

  [self resetToolbarAfterSideSwipeSnapshot];

  return toolbarSnapshot;
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  [self.viewController setScrollProgressForTabletOmnibox:progress];
}

#pragma mark - ToolbarCommands

- (void)triggerToolsMenuButtonAnimation {
  [self.viewController.toolsMenuButton triggerAnimation];
}

#pragma mark - ToolbarCoordinatee

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater {
  return self.viewController;
}

#pragma mark - Protected

- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type {
  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;

  self.actionHandler = [[ToolbarButtonActionsHandler alloc] init];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.actionHandler.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands, FindInPageCommands,
                     OmniboxCommands>>(self.browser->GetCommandDispatcher());
  self.actionHandler.incognito =
      self.browser->GetBrowserState()->IsOffTheRecord();

  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.actionHandler = self.actionHandler;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

  return buttonFactory;
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  BOOL isNTP = IsVisibleURLNewTabPage(webState);

  [self.mediator updateConsumerForWebState:webState];
  [self.viewController updateForSideSwipeSnapshotOnNTP:isNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator updateConsumerForWebState:self.browser->GetWebStateList()
                                               ->GetActiveWebState()];
  [self.viewController resetAfterSideSwipeSnapshot];
}

@end
