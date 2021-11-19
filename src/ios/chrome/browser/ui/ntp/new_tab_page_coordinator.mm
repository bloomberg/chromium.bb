// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/feed/core/v2/public/ios/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/default_search_manager.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/discover_feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"
#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_coordinator.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_preview_delegate.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_observer_bridge.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_view_controller_configuration.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageCoordinator () <BooleanObserver,
                                     DiscoverFeedDelegate,
                                     DiscoverFeedObserverBridgeDelegate,
                                     DiscoverFeedPreviewDelegate,
                                     NewTabPageCommands,
                                     NewTabPageContentDelegate,
                                     OverscrollActionsControllerDelegate,
                                     PrefObserverDelegate,
                                     SceneStateObserver> {
  // Helper object managing the availability of the voice search feature.
  VoiceSearchAvailability _voiceSearchAvailability;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Observes changes in the DiscoverFeed.
  std::unique_ptr<DiscoverFeedObserverBridge>
      _discoverFeedProviderObserverBridge;
}

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* ntpViewController;

// Mediator owned by this Coordinator.
@property(nonatomic, strong) NTPHomeMediator* ntpMediator;

// View controller wrapping the Discover feed.
@property(nonatomic, strong)
    DiscoverFeedWrapperViewController* discoverFeedWrapperViewController;

// View controller for the incognito NTP.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

// The timetick of the last time the NTP was displayed.
@property(nonatomic, assign) base::TimeTicks didAppearTime;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
@property(nonatomic, assign) BOOL visible;

// Whether the view is new tab view is currently presented (possibly in
// background). Used to report NTP usage metrics.
@property(nonatomic, assign) BOOL viewPresented;

// Wheter the scene is currently in foreground.
@property(nonatomic, assign) BOOL sceneInForeground;

// Handles interactions with the content suggestions header and the fake
// omnibox.
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerSynchronizer;

// The ViewController displayed by this Coordinator. This is the returned
// ViewController and will contain the |containedViewController| (Which can
// change depending on Feed visibility).
@property(nonatomic, strong) UIViewController* containerViewController;

// The coordinator contained ViewController. It can be either a
// NewTabPageViewController (When the Discover Feed is being shown) or a
// ContentSuggestionsViewController (When the Discover Feed is hidden or when
// the non refactored NTP is being used.)
// TODO(crbug.com/1114792): Update this comment when the NTP refactors launches.
@property(nonatomic, strong) UIViewController* containedViewController;

// PrefService used by this Coordinator.
@property(nonatomic, assign) PrefService* prefService;

// Whether the feed is expanded or collapsed. Collapsed
// means the feed header is shown, but not any of the feed content.
@property(nonatomic, strong) PrefBackedBoolean* discoverFeedExpanded;

// The view controller representing the Discover feed.
@property(nonatomic, weak) UIViewController* discoverFeedViewController;

// The Coordinator to display previews for Discover feed websites. It also
// handles the actions related to them.
@property(nonatomic, strong) LinkPreviewCoordinator* linkPreviewCoordinator;

@end

@implementation NewTabPageCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _containerViewController = [[UIViewController alloc] init];

    _prefService =
        ChromeBrowserState::FromBrowserState(browser->GetBrowserState())
            ->GetPrefs();
      _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
      _prefChangeRegistrar->Init(_prefService);
      _prefObserverBridge.reset(new PrefObserverBridge(self));
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kArticlesForYouEnabled, _prefChangeRegistrar.get());
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kNTPContentSuggestionsEnabled, _prefChangeRegistrar.get());
      _prefObserverBridge->ObserveChangesForPreference(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName,
          _prefChangeRegistrar.get());
      _discoverFeedExpanded = [[PrefBackedBoolean alloc]
          initWithPrefService:_prefService
                     prefName:feed::prefs::kArticlesListVisible];
      [_discoverFeedExpanded setObserver:self];
      _discoverFeedProviderObserverBridge =
          std::make_unique<DiscoverFeedObserverBridge>(self);
  }
  return self;
}

- (void)start {
  if (self.started)
    return;

  DCHECK(self.browser);
  DCHECK(self.webState);
  DCHECK(self.toolbarDelegate);
  DCHECK(!self.contentSuggestionsCoordinator);

  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    self.started = YES;
    return;
  }

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.ntpMediator = [[NTPHomeMediator alloc]
             initWithWebState:self.webState
           templateURLService:templateURLService
                    URLLoader:UrlLoadingBrowserAgent::FromBrowser(self.browser)
                  authService:AuthenticationServiceFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
              identityManager:IdentityManagerFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
        accountManagerService:ChromeAccountManagerServiceFactory::
                                  GetForBrowserState(
                                      self.browser->GetBrowserState())
                   logoVendor:ios::GetChromeBrowserProvider().CreateLogoVendor(
                                  self.browser, self.webState)
      voiceSearchAvailability:&_voiceSearchAvailability];
  self.ntpMediator.browser = self.browser;

  self.contentSuggestionsCoordinator = [[ContentSuggestionsCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  self.contentSuggestionsCoordinator.webState = self.webState;
  self.contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
  self.contentSuggestionsCoordinator.ntpMediator = self.ntpMediator;
  self.contentSuggestionsCoordinator.ntpCommandHandler = self;
  self.contentSuggestionsCoordinator.bubblePresenter = self.bubblePresenter;
  self.contentSuggestionsCoordinator.discoverFeedDelegate = self;

  DiscoverFeedMetricsRecorder* discoverFeedMetricsRecorder;

  if (IsDiscoverFeedEnabled()) {
    // Creating the DiscoverFeedService will start the DiscoverFeed.
    DiscoverFeedService* discoverFeedService =
        DiscoverFeedServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    discoverFeedMetricsRecorder =
        discoverFeedService->GetDiscoverFeedMetricsRecorder();
    self.contentSuggestionsCoordinator.discoverFeedMetricsRecorder =
        discoverFeedMetricsRecorder;
  }

  self.ntpViewController = [[NewTabPageViewController alloc] init];

  // Requests a Discover feed here if the correct flags and prefs are enabled.
  if ([self shouldFeedBeVisible]) {
    self.discoverFeedViewController = [self discoverFeed];
  }

  [self.contentSuggestionsCoordinator start];
  [self configureNTPAsMainViewController];
  self.ntpViewController.discoverFeedMetricsRecorder =
      discoverFeedMetricsRecorder;

  base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState addObserver:self];
  self.sceneInForeground =
      sceneState.activationLevel >= SceneActivationLevelForegroundInactive;

  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  // Unfocus omnibox, to prevent it from lingering when it should be dismissed
  // (for example, when navigating away or when changing feed visibility).
  id<OmniboxCommands> omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandHandler cancelOmniboxEdit];
  self.viewPresented = NO;
  [self updateVisible];
  [self.contentSuggestionsCoordinator stop];
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState removeObserver:self];
  self.contentSuggestionsCoordinator = nil;
  self.incognitoViewController = nil;
  self.ntpViewController = nil;
  ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->RemoveFeedViewController(self.discoverFeedViewController);
  self.discoverFeedWrapperViewController = nil;
  self.discoverFeedViewController = nil;

  [self.ntpMediator shutdown];
  self.ntpMediator = nil;

  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];

  self.started = NO;
}

- (void)disconnect {
  // TODO(crbug.com/1200303): Move this to stop once we stop starting/stopping
  // the Coordinator when turning the feed on/off.
  [self.discoverFeedExpanded setObserver:nil];
  self.discoverFeedExpanded = nil;

  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _discoverFeedProviderObserverBridge.reset();
}

// Updates the visible property based on viewPresented and sceneInForeground
// properties.
// Sends metrics when NTP becomes invisible.
- (void)updateVisible {
  BOOL visible = self.viewPresented && self.sceneInForeground;
  if (visible == self.visible) {
    return;
  }
  self.visible = visible;
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    // Do not report metrics on incognito NTP.
    return;
  }
  if (visible) {
    self.didAppearTime = base::TimeTicks::Now();
  } else {
    if (!self.didAppearTime.is_null()) {
      UmaHistogramMediumTimes("NewTabPage.TimeSpent",
                              base::TimeTicks::Now() - self.didAppearTime);
      self.didAppearTime = base::TimeTicks();
    }
  }
}

#pragma mark - ChromeCoordinatorHelpers

// Configures |self.ntpViewController| and sets it up as the main ViewController
// managed by this Coordinator.
- (void)configureNTPAsMainViewController {
  self.contentSuggestionsCoordinator.headerController.baseViewController =
      self.baseViewController;

  self.ntpViewController.contentSuggestionsViewController =
      self.contentSuggestionsCoordinator.viewController;
  self.ntpViewController.panGestureHandler = self.panGestureHandler;
  self.ntpViewController.feedVisible =
      [self shouldFeedBeVisible] && self.discoverFeedViewController;
  self.ntpMediator.ntpViewController = self.ntpViewController;

  self.discoverFeedWrapperViewController =
      [[DiscoverFeedWrapperViewController alloc]
          initWithDiscoverFeedViewController:self.discoverFeedViewController];

  self.headerSynchronizer = [[ContentSuggestionsHeaderSynchronizer alloc]
      initWithCollectionController:self.ntpViewController
                  headerController:self.contentSuggestionsCoordinator
                                       .headerController];

  self.ntpViewController.discoverFeedWrapperViewController =
      self.discoverFeedWrapperViewController;
  self.ntpViewController.overscrollDelegate = self;
  self.ntpViewController.ntpContentDelegate = self;
  self.ntpViewController.identityDiscButton =
      [self.contentSuggestionsCoordinator.headerController identityDiscButton];

  self.ntpViewController.headerController =
      self.contentSuggestionsCoordinator.headerController;
  self.ntpMediator.primaryViewController = self.ntpViewController;
  self.ntpMediator.headerCollectionInteractionHandler = self.headerSynchronizer;

  [self configureMainViewControllerUsing:self.ntpViewController];
}

// Configures the main ViewController managed by this Coordinator.
- (void)configureMainViewControllerUsing:
    (UIViewController*)containedViewController {
  [containedViewController
      willMoveToParentViewController:self.containerViewController];
  [self.containerViewController addChildViewController:containedViewController];
  [self.containerViewController.view addSubview:containedViewController.view];
  [containedViewController
      didMoveToParentViewController:self.containerViewController];

  containedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(containedViewController.view,
                     self.containerViewController.view);

  self.containedViewController = containedViewController;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  [self start];
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.containerViewController;
  }
}

- (id<ThumbStripSupporting>)thumbStripSupporting {
  return self.ntpViewController;
}

#pragma mark - Public Methods

- (void)setWebState:(web::WebState*)webState {
  self.ntpMediator.webState = webState;
  _webState = webState;
}

- (void)dismissModals {
  [self.contentSuggestionsCoordinator dismissModals];
}

- (void)stopScrolling {
  if (!self.contentSuggestionsCoordinator) {
    return;
  }
  [self.ntpViewController stopScrolling];
}

- (UIEdgeInsets)contentInset {
  return [self.contentSuggestionsCoordinator contentInset];
}

- (CGPoint)contentOffset {
  return [self.contentSuggestionsCoordinator contentOffset];
}

- (void)willUpdateSnapshot {
  if (self.contentSuggestionsCoordinator.started) {
    [self.ntpViewController willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  if (self.discoverFeedViewController) {
    [self.ntpViewController focusFakebox];
  } else {
    [self.contentSuggestionsCoordinator.headerController focusFakebox];
  }
}

- (void)reload {
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->RefreshFeed();
  [self reloadContentSuggestions];
}

- (void)locationBarDidBecomeFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidBecomeFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidResignFirstResponder];
}

- (void)constrainDiscoverHeaderMenuButtonNamedGuide {
  [self.contentSuggestionsCoordinator
          constrainDiscoverHeaderMenuButtonNamedGuide];
}

- (void)ntpDidChangeVisibility:(BOOL)visible {
  if (visible) {
    [self.contentSuggestionsCoordinator configureStartSurfaceIfNeeded];
  }
  self.viewPresented = visible;
  [self updateVisible];
}

#pragma mark - NewTabPageCommands

- (void)updateNTPForDiscoverFeed {
  [self stop];
  [self start];
  [self updateDiscoverFeedLayout];

  [self.containerViewController.view setNeedsLayout];
  [self.containerViewController.view layoutIfNeeded];
}

- (void)updateDiscoverFeedLayout {
  // If this coordinator has not finished [self start], the below will start
  // viewDidLoad before the UI is ready, failing DCHECKS.
  if (self.started) {
    [self.containedViewController.view setNeedsLayout];
    [self.containedViewController.view layoutIfNeeded];
    [self.ntpViewController updateContentSuggestionForCurrentLayout];
  }
}

- (void)setContentOffsetToTop {
  [self.ntpViewController setContentOffsetToTop];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.contentSuggestionsCoordinator
              .headerController logoAnimationControllerOwner];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  self.sceneInForeground = level >= SceneActivationLevelForegroundInactive;
  [self updateVisible];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNTPForDiscoverFeed];
}

#pragma mark - DiscoverFeedDelegate

- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed {
  return [SceneStateBrowserAgent::FromBrowser(self.browser)
              ->GetSceneState()
              .window.rootViewController.view safeAreaInsets];
}

- (void)contentSuggestionsWasUpdated {
  [self updateDiscoverFeedLayout];
  [self setContentOffsetToTop];
}

- (void)returnToRecentTabWasAdded {
  [self updateDiscoverFeedLayout];
  [self setContentOffsetToTop];
}

#pragma mark - DiscoverFeedObserverBridge

- (void)onDiscoverFeedModelRecreated {
  [self updateNTPForDiscoverFeed];
}

#pragma mark - DiscoverFeedPreviewDelegate

- (UIViewController*)discoverFeedPreviewWithURL:(const GURL)URL {
  std::string referrerURL = base::GetFieldTrialParamValueByFeature(
      kEnableDiscoverFeedPreview, kDiscoverReferrerParameter);
  if (referrerURL.empty()) {
    referrerURL = kDefaultDiscoverReferrer;
  }

  self.linkPreviewCoordinator =
      [[LinkPreviewCoordinator alloc] initWithBrowser:self.browser URL:URL];
  self.linkPreviewCoordinator.referrer =
      web::Referrer(GURL(referrerURL), web::ReferrerPolicyDefault);
  [self.linkPreviewCoordinator start];
  return [self.linkPreviewCoordinator linkPreviewViewController];
}

- (void)didTapDiscoverFeedPreview {
  DCHECK(self.linkPreviewCoordinator);
  [self.linkPreviewCoordinator handlePreviewAction];
  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  id<ApplicationCommands, BrowserCommands, OmniboxCommands, SnackbarCommands>
      handler = static_cast<id<ApplicationCommands, BrowserCommands,
                               OmniboxCommands, SnackbarCommands>>(
          self.browser->GetCommandDispatcher());
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      [handler openURLInNewTab:[OpenNewTabCommand command]];
    } break;
    case OverscrollAction::CLOSE_TAB: {
      [handler closeCurrentTab];
      base::RecordAction(base::UserMetricsAction("OverscrollActionCloseTab"));
    } break;
    case OverscrollAction::REFRESH:
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return YES;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [[self.contentSuggestionsCoordinator.headerController toolBarView]
      snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.discoverFeedWrapperViewController.view;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat topInset =
      self.discoverFeedWrapperViewController.view.safeAreaInsets.top;
  return self.contentSuggestionsCoordinator.viewController.collectionView
             .contentSize.height +
         topInset;
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat height =
      [self.contentSuggestionsCoordinator.headerController toolBarView]
          .bounds.size.height;
  CGFloat topInset =
      self.discoverFeedWrapperViewController.view.safeAreaInsets.top;
  return height + topInset;
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // Fullscreen isn't supported here.
  return nullptr;
}

#pragma mark - NewTabPageContentDelegate

- (void)reloadContentSuggestions {
  [self.contentSuggestionsCoordinator reload];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kArticlesForYouEnabled ||
      preferenceName == prefs::kNTPContentSuggestionsEnabled) {
    [self updateNTPForDiscoverFeed];
  }
  if (preferenceName ==
      DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
    [self defaultSearchEngineDidChange];
  }
}

#pragma mark - Private

// Determines whether the feed should be fetched based on the user prefs.
- (BOOL)shouldFeedBeVisible {
  BOOL isFeedEnabled =
      self.prefService->GetBoolean(prefs::kArticlesForYouEnabled) &&
      self.prefService->GetBoolean(prefs::kNTPContentSuggestionsEnabled);

  return isFeedEnabled && [self.discoverFeedExpanded value];
}

// Creates, configures and returns a DiscoverFeed ViewController.
- (UIViewController*)discoverFeed {
  if (tests_hook::DisableDiscoverFeed())
    return nil;

  DiscoverFeedViewControllerConfiguration* viewControllerConfig =
      [[DiscoverFeedViewControllerConfiguration alloc] init];
  viewControllerConfig.browser = self.browser;
  viewControllerConfig.scrollDelegate = self.ntpViewController;
  viewControllerConfig.previewDelegate = self;

  UIViewController* discoverFeed =
      ios::GetChromeBrowserProvider()
          .GetDiscoverFeedProvider()
          ->NewFeedViewControllerWithConfiguration(viewControllerConfig);
  return discoverFeed;
}

// Handles how the NTP should react when the default search engine setting is
// changed.
- (void)defaultSearchEngineDidChange {
  BOOL isScrolledToTop = [self.ntpViewController isNTPScrolledToTop];
  [self updateDiscoverFeedLayout];
  // Ensures doodle is visible if content suggestions height changes when
  // scrolled to top. Otherwise, maintain scroll position.
  if (isScrolledToTop) {
    [self.ntpViewController setContentOffsetToTop];
  }
}

@end
