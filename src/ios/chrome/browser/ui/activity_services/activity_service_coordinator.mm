// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_coordinator.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_mediator.h"
#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_image_source.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_text_source.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_url_source.h"
#import "ios/chrome/browser/ui/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data_builder.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActivityServiceCoordinator ()

@property(nonatomic, weak) id<BrowserCommands, FindInPageCommands> handler;

@property(nonatomic, strong) ActivityServiceMediator* mediator;

@property(nonatomic, strong) UIActivityViewController* viewController;

// Parameters determining the activity flow and values.
@property(nonatomic, strong) ActivityParams* params;

@end

@implementation ActivityServiceCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params {
  DCHECK(params);
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _params = params;
  }
  return self;
}

#pragma mark - Public methods

- (void)start {
  self.handler = static_cast<id<BrowserCommands, FindInPageCommands>>(
      self.browser->GetCommandDispatcher());

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);
  self.mediator =
      [[ActivityServiceMediator alloc] initWithHandler:self.handler
                                      bookmarksHandler:self.scopedHandler
                                   qrGenerationHandler:self.scopedHandler
                                           prefService:browserState->GetPrefs()
                                         bookmarkModel:bookmarkModel];

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  self.mediator.promoScheduler =
      [DefaultBrowserSceneAgent agentFromScene:sceneState].nonModalScheduler;

  [self.mediator shareStartedWithScenario:self.params.scenario];

  // Image item
  if (self.params.image) {
    [self shareImage];
    return;
  }

  if (self.params.URLs.count > 0) {
    // If at least one valid URL is found, share the URLs in |_params|.
    for (URLWithTitle* urlWithTitle in self.params.URLs) {
      if (!urlWithTitle.URL.is_empty()) {
        [self shareURLs];
        return;
      }
    }
  }

  // Default to sharing the current page
  [self shareCurrentPage];
}

- (void)stop {
  [self.viewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;

  self.mediator = nil;
}

#pragma mark - Private Methods

// Sets up the activity ViewController with the given |items| and |activities|.
- (void)shareItems:(NSArray<id<ChromeActivityItemSource>>*)items
        activities:(NSArray*)activities {
  self.viewController =
      [[UIActivityViewController alloc] initWithActivityItems:items
                                        applicationActivities:activities];

  [self.viewController setModalPresentationStyle:UIModalPresentationPopover];

  NSSet* excludedActivityTypes =
      [self.mediator excludedActivityTypesForItems:items];
  [self.viewController
      setExcludedActivityTypes:[excludedActivityTypes allObjects]];

  // Set-up popover positioning (for iPad).
  DCHECK(self.positionProvider);
  if ([self.positionProvider respondsToSelector:@selector(barButtonItem)] &&
      self.positionProvider.barButtonItem) {
    self.viewController.popoverPresentationController.barButtonItem =
        self.positionProvider.barButtonItem;
  } else {
    self.viewController.popoverPresentationController.sourceView =
        self.positionProvider.sourceView;
    self.viewController.popoverPresentationController.sourceRect =
        self.positionProvider.sourceRect;
  }

  // Set completion callback.
  __weak __typeof(self) weakSelf = self;
  [self.viewController setCompletionWithItemsHandler:^(
                           NSString* activityType, BOOL completed,
                           NSArray* returnedItems, NSError* activityError) {
    ActivityServiceCoordinator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    // Delegate post-activity processing to the mediator.
    [strongSelf.mediator shareFinishedWithScenario:strongSelf.params.scenario
                                      activityType:activityType
                                         completed:completed];

    // Signal the presentation provider that our scenario is over.
    [strongSelf.presentationProvider activityServiceDidEndPresenting];
  }];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private Methods: Current Page

// Fetches the current tab's URL, configures activities and items, and shows
// an activity view.
- (void)shareCurrentPage {
  // Retrieve the current page's URL.
  __weak __typeof(self) weakSelf = self;
  activity_services::RetrieveCanonicalUrl(
      self.browser->GetWebStateList()->GetActiveWebState(), ^(const GURL& url) {
        [weakSelf sharePageWithCanonicalURL:url];
      });
}

// Shares the current page using its |canonicalURL|.
- (void)sharePageWithCanonicalURL:(const GURL&)canonicalURL {
  ShareToData* data = activity_services::ShareToDataForWebState(
      self.browser->GetWebStateList()->GetActiveWebState(), canonicalURL);
  if (!data)
    return;

  NSArray<ChromeActivityURLSource*>* items =
      [self.mediator activityItemsForDataItems:@[ data ]];
  NSArray* activities =
      [self.mediator applicationActivitiesForDataItems:@[ data ]];

  [self shareItems:items activities:activities];
}

#pragma mark - Private Methods: Share Image

// Configures activities and items for an image and its title, and shows
// an activity view.
- (void)shareImage {
  ShareImageData* data =
      [[ShareImageData alloc] initWithImage:self.params.image
                                      title:self.params.imageTitle];

  NSArray<ChromeActivityImageSource*>* items =
      [self.mediator activityItemsForImageData:data];
  NSArray* activities = [self.mediator applicationActivitiesForImageData:data];

  [self shareItems:items activities:activities];
}

#pragma mark - Private Methods: Share URL

// Configures activities and items for a URL and its title, and shows
// an activity view. Also adds another activity item for additional text, if
// there is any.
- (void)shareURLs {
  NSMutableArray* dataItems = [[NSMutableArray alloc] init];

  for (URLWithTitle* urlWithTitle in self.params.URLs) {
    ShareToData* data =
        activity_services::ShareToDataForURLWithTitle(urlWithTitle);
    [dataItems addObject:data];
  }

  NSArray<id<ChromeActivityItemSource>>* items =
      [self.mediator activityItemsForDataItems:dataItems];
  NSArray* activities =
      [self.mediator applicationActivitiesForDataItems:dataItems];

  [self shareItems:items activities:activities];
}

@end
