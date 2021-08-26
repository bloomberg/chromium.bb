// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_coordinator.h"

#include "base/metrics/field_trial_params.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DiscoverFeedPreviewCoordinator () {
  // The WebState used for loading the preview.
  std::unique_ptr<web::WebState> _feedPreviewWebState;
}

// View controller of the discover feed preview.
@property(nonatomic, strong) DiscoverFeedPreviewViewController* viewController;

// URL of the discover feed preview.
@property(nonatomic, assign) GURL URL;

@end

@implementation DiscoverFeedPreviewCoordinator

- (instancetype)initWithBrowser:(Browser*)browser URL:(const GURL)URL {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _URL = GURL(URL);
  }
  return self;
}

- (void)start {
  [self configureWebState];
  self.viewController = [[DiscoverFeedPreviewViewController alloc]
      initWithView:_feedPreviewWebState->GetView()];
}

- (void)stop {
  self.viewController = nil;
  _feedPreviewWebState.reset();
}

- (UIViewController*)discoverFeedPreviewViewController {
  return self.viewController;
}

- (void)handlePreviewAction {
  WebStateList* web_state_list = self.browser->GetWebStateList();
  DCHECK_NE(WebStateList::kInvalidIndex, web_state_list->active_index());
  DCHECK(_feedPreviewWebState);
  web_state_list->ReplaceWebStateAt(web_state_list->active_index(),
                                    std::move(_feedPreviewWebState));
}

#pragma mark - Private

// Configures the web state that used to load the preview.
- (void)configureWebState {
  // Make a copy of the active web state.
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  ChromeBrowserState* browserState =
      ChromeBrowserState::FromBrowserState(currentWebState->GetBrowserState());
  web::WebState::CreateParams createParams(browserState);
  _feedPreviewWebState = web::WebState::CreateWithStorageSession(
      createParams, currentWebState->BuildSessionStorage());
  // Attach tab helpers to use _feedPreviewWebState as a browser tab. It ensures
  // _feedPreviewWebState has all the expected tab helpers, including the
  // history tab helper which adding the history entry of the preview.
  AttachTabHelpers(_feedPreviewWebState.get(), /*for_prerender=*/false);
  _feedPreviewWebState->SetWebUsageEnabled(true);

  std::string referrerURL = base::GetFieldTrialParamValueByFeature(
      kEnableDiscoverFeedPreview, kDiscoverReferrerParameter);
  if (referrerURL.empty()) {
    referrerURL = kDefaultDiscoverReferrer;
  }
  web::Referrer referrer =
      web::Referrer(GURL(referrerURL), web::ReferrerPolicyDefault);

  // Load the preview page using the copied web state.
  web::NavigationManager::WebLoadParams loadParams(self.URL);
  loadParams.referrer = referrer;
  // Attempt to prevent the WebProcess from suspending. Set this before
  // triggering the preview page loads.
  _feedPreviewWebState->SetKeepRenderProcessAlive(true);
  _feedPreviewWebState->GetNavigationManager()->LoadURLWithParams(loadParams);
  _feedPreviewWebState->GetNavigationManager()->LoadIfNecessary();
}

@end
