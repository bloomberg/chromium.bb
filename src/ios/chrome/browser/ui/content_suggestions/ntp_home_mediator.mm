// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/discover_feed/discover_feed_service.h"
#include "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#include "ios/chrome/browser/ui/ntp/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URL for 'Manage Activity' item in the Discover feed menu.
const char kFeedManageActivityURL[] =
    "https://myactivity.google.com/myactivity?product=50";
// URL for 'Manage Interests' item in the Discover feed menu.
const char kFeedManageInterestsURL[] =
    "https://google.com/preferences/interests";
// URL for 'Manage Hidden' item in the Discover feed menu.
const char kFeedManageHiddenURL[] =
    "https://google.com/preferences/interests/hidden";
// URL for 'Learn More' item in the Discover feed menu;
const char kFeedLearnMoreURL[] = "https://support.google.com/chrome/"
                                 "?p=new_tab&co=GENIE.Platform%3DiOS&oco=1";
}  // namespace

@interface NTPHomeMediator () <ChromeAccountManagerServiceObserver,
                               CRWWebStateObserver,
                               IdentityManagerObserverBridgeDelegate,
                               SearchEngineObserving,
                               VoiceSearchAvailabilityObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Observes changes in identity and updates the Identity Disc.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Used to load URLs.
  UrlLoadingBrowserAgent* _URLLoader;
}

@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;
// Authentication Service to get the current user's avatar.
@property(nonatomic, assign) AuthenticationService* authService;
// Logo vendor to display the doodle on the NTP.
@property(nonatomic, strong) id<LogoVendor> logoVendor;
// The voice search availability.
@property(nonatomic, assign) VoiceSearchAvailability* voiceSearchAvailability;
// This is the object that knows how to update the Identity Disc UI.
@property(nonatomic, weak) id<UserAccountImageUpdateDelegate> imageUpdater;

@end

@implementation NTPHomeMediator

- (instancetype)
           initWithWebState:(web::WebState*)webState
         templateURLService:(TemplateURLService*)templateURLService
                  URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                authService:(AuthenticationService*)authService
            identityManager:(signin::IdentityManager*)identityManager
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
                 logoVendor:(id<LogoVendor>)logoVendor
    voiceSearchAvailability:(VoiceSearchAvailability*)voiceSearchAvailability {
  self = [super init];
  if (self) {
    _webState = webState;
    _templateURLService = templateURLService;
    _URLLoader = URLLoader;
    _authService = authService;
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    // Listen for default search engine changes.
    _searchEngineObserver = std::make_unique<SearchEngineObserverBridge>(
        self, self.templateURLService);
    _logoVendor = logoVendor;
    _voiceSearchAvailability = voiceSearchAvailability;
  }
  return self;
}

- (void)dealloc {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

- (void)setUp {
  DCHECK(!_webStateObserver);

  _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  if (self.webState) {
    self.webState->AddObserver(_webStateObserver.get());
  }

  self.voiceSearchAvailability->AddObserver(self);

  [self.consumer setLogoVendor:self.logoVendor];
  [self.consumer setVoiceSearchIsEnabled:self.voiceSearchAvailability
                                             ->IsVoiceSearchAvailable()];

  self.templateURLService->Load();
  [self searchEngineChanged];
}

- (void)shutdown {
  _searchEngineObserver.reset();
  if (_webState && _webStateObserver) {
    if (!IsSingleNtpEnabled()) {
      [self saveContentOffsetForWebState:_webState];
    }
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
  }
  if (_voiceSearchAvailability) {
    _voiceSearchAvailability->RemoveObserver(self);
    _voiceSearchAvailability = nullptr;
  }
  _identityObserverBridge.reset();
  _accountManagerServiceObserver.reset();
  self.accountManagerService = nil;
}

- (void)locationBarDidBecomeFirstResponder {
  [self.consumer locationBarBecomesFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.consumer locationBarResignsFirstResponder];
}

- (void)saveContentOffsetForWebState:(web::WebState*)webState {
  if (!IsSingleNtpEnabled() &&
      webState->GetLastCommittedURL().DeprecatedGetOriginAsURL() !=
          kChromeUINewTabURL) {
    return;
  }
  if (IsSingleNtpEnabled() &&
      (webState->GetLastCommittedURL().DeprecatedGetOriginAsURL() !=
           kChromeUINewTabURL &&
       webState->GetVisibleURL().DeprecatedGetOriginAsURL() !=
           kChromeUINewTabURL)) {
    // Do nothing if the current page is not the NTP.
    return;
  }

  web::NavigationManager* manager = webState->GetNavigationManager();
  web::NavigationItem* item =
      webState->GetLastCommittedURL() == kChromeUINewTabURL
          ? manager->GetLastCommittedItem()
          : manager->GetVisibleItem();
  web::PageDisplayState displayState;

  // TODO(crbug.com/1114792): Create a protocol to stop having references to
  // both of these ViewControllers directly.
  UICollectionView* collectionView =
      self.ntpViewController.discoverFeedWrapperViewController
          .contentCollectionView;
  UIEdgeInsets contentInset = collectionView.contentInset;
  CGPoint contentOffset = collectionView.contentOffset;
  if ([self.suggestionsMediator mostRecentTabStartSurfaceTileIsShowing]) {
    // Return to Recent tab tile is only shown one time, so subtract it's
    // vertical space to preserve relative scroll position from top.
    CGFloat tileSectionHeight =
        [ContentSuggestionsReturnToRecentTabCell defaultSize].height +
        content_suggestions::kReturnToRecentTabSectionBottomMargin;
    if (contentOffset.y >
        tileSectionHeight +
            [self.headerCollectionInteractionHandler pinnedOffsetY]) {
      contentOffset.y -= tileSectionHeight;
    }
  }

  contentOffset.y -=
      self.headerCollectionInteractionHandler.collectionShiftingOffset;
  displayState.scroll_state() =
      web::PageScrollState(contentOffset, contentInset);
  item->SetPageDisplayState(displayState);
}

// Opens web page for a menu item in the NTP.
- (void)openMenuItemWebPage:(GURL)URL {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests())
    return;
  _URLLoader->Load(UrlLoadParams::InCurrentTab(URL));
  // TODO(crbug.com/1085419): Add metrics.
}

- (void)handleFeedManageActivityTapped {
  [self openMenuItemWebPage:GURL(kFeedManageActivityURL)];
  [self.feedMetricsRecorder recordHeaderMenuManageActivityTapped];
}

- (void)handleFeedManageInterestsTapped {
  [self openMenuItemWebPage:GURL(kFeedManageInterestsURL)];
  [self.feedMetricsRecorder recordHeaderMenuManageInterestsTapped];
}

- (void)handleFeedManageHiddenTapped {
  [self openMenuItemWebPage:GURL(kFeedManageHiddenURL)];
  [self.feedMetricsRecorder recordHeaderMenuManageHiddenTapped];
}

- (void)handleFeedLearnMoreTapped {
  [self openMenuItemWebPage:GURL(kFeedLearnMoreURL)];
  [self.feedMetricsRecorder recordHeaderMenuLearnMoreTapped];
}

#pragma mark - Properties.

- (void)setWebState:(web::WebState*)webState {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
  }
  _webState = webState;
  if (IsSingleNtpEnabled()) {
    [self.logoVendor setWebState:webState];
  }
  if (_webState && _webStateObserver) {
    if (IsSingleNtpEnabled()) {
      [self setContentOffsetForWebState:webState];
    }
    _webState->AddObserver(_webStateObserver.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self setContentOffsetForWebState:webState];
}

- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self locationBarDidResignFirstResponder];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(self.webState, webState);
  self.webState = nullptr;
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityChanged:(ChromeIdentity*)identity {
  [self updateAccountImage];
}

#pragma mark - ContentSuggestionsHeaderViewControllerDelegate

- (BOOL)isScrolledToMinimumHeight {
  return [self.ntpViewController isScrolledToMinimumHeight];
}

- (void)registerImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater {
  self.imageUpdater = imageUpdater;
  [self updateAccountImage];
}

- (BOOL)ignoreLoadRequests {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests()) {
    return YES;
  }
  return NO;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL showLogo = NO;
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  if (defaultURL) {
    showLogo = defaultURL->GetEngineType(
                   self.templateURLService->search_terms_data()) ==
               SEARCH_ENGINE_GOOGLE;
  }
  [self.consumer setLogoIsShowing:showLogo];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateAccountImage];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - VoiceSearchAvailabilityObserver

- (void)voiceSearchAvailability:(VoiceSearchAvailability*)availability
            updatedAvailability:(BOOL)available {
  [self.consumer setVoiceSearchIsEnabled:available];
}

#pragma mark - Private

// Set the NTP scroll offset for the current navigation item.
- (void)setContentOffsetForWebState:(web::WebState*)webState {
  if (webState->GetVisibleURL().DeprecatedGetOriginAsURL() !=
      kChromeUINewTabURL) {
    return;
  }
  web::NavigationManager* navigationManager = webState->GetNavigationManager();
  web::NavigationItem* item = navigationManager->GetVisibleItem();
  CGFloat offset =
      item ? item->GetPageDisplayState().scroll_state().content_offset().y : 0;
  CGFloat minimumOffset = -[self.ntpViewController heightAboveFeed];
  if (offset > minimumOffset) {
    [self.ntpViewController setSavedContentOffset:offset];
  } else if (IsSingleNtpEnabled()) {
    // Remove this if NTPs are ever scoped back to the WebState.
    [self.ntpViewController setContentOffsetToTop];
    // Refresh NTP content if there is is no saved scrolled state or when a new
    // NTP is opened. Since the same NTP is being shared across tabs, this
    // ensures that new content is being fetched.
    [self.suggestionsMediator refreshMostVisitedTiles];

    // Refresh DiscoverFeed unless in off-the-record NTP.
    if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
      DiscoverFeedServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->RefreshFeedIfNeeded();
    }
  }
}

// Fetches and update user's avatar on NTP, or use default avatar if user is
// not signed in.
- (void)updateAccountImage {
  UIImage* image = nil;
  // Fetches user's identity from Authentication Service.
  ChromeIdentity* identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (identity) {
    // Only show an avatar if the user is signed in.
    image = self.accountManagerService->GetIdentityAvatarWithIdentity(
        identity, IdentityAvatarSize::SmallSize);
  }
  [self.imageUpdater updateAccountImage:image];
}

@end
