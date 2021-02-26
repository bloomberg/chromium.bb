// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_controller.h"

#include "base/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#import "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/url_formatter/url_formatter.h"
#include "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/browser_util.h"
#include "ios/chrome/browser/ntp/features.h"
#include "ios/chrome/browser/screenshot/screenshot_delegate.h"
#include "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_commands.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_coordinator.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_field_trial.h"
#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#include "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/incognito_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/main/ui_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#include "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"
#include "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/whats_new/default_browser_utils.h"
#import "ios/chrome/browser/url_loading/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A rough estimate of the expected duration of a view controller transition
// animation. It's used to temporarily disable mutally exclusive chrome
// commands that trigger a view controller presentation.
const int64_t kExpectedTransitionDurationInNanoSeconds = 0.2 * NSEC_PER_SEC;

// Possible results of snapshotting at the moment the user enters the tab
// switcher. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class EnterTabSwitcherSnapshotResult {
  // Page was loading at the time of the snapshot request, and the snapshot
  // failed.
  kPageLoadingAndSnapshotFailed = 0,
  // Page was loading at the time of the snapshot request, and the snapshot
  // succeeded.
  kPageLoadingAndSnapshotSucceeded = 1,
  // Page was not loading at the time of the snapshot request, and the snapshot
  // failed.
  kPageNotLoadingAndSnapshotFailed = 2,
  // Page was not loading at the time of the snapshot request, and the snapshot
  // succeeded.
  kPageNotLoadingAndSnapshotSucceeded = 3,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kPageNotLoadingAndSnapshotSucceeded,
};

// Histogram enum values for showing the experiment arms of the location
// permissions experiment. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
enum class LocationPermissionsUI {
  // The First Run native location prompt was not shown.
  kFirstRunPromptNotShown = 0,
  // The First Run location permissions modal was shown.
  kFirstRunModal = 1,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kFirstRunModal,
};

// Used to update the current BVC mode if a new tab is added while the tab
// switcher view is being dismissed.  This is different than ApplicationMode in
// that it can be set to |NONE| when not in use.
enum class TabSwitcherDismissalMode { NONE, NORMAL, INCOGNITO };

// Key of the UMA IOS.MultiWindow.OpenInNewWindow histogram.
const char kMultiWindowOpenInNewWindowHistogram[] =
    "IOS.MultiWindow.OpenInNewWindow";

}  // namespace

@interface SceneController () <AppStateObserver,
                               UserFeedbackDataSource,
                               SettingsNavigationControllerDelegate,
                               SceneURLLoadingServiceDelegate,
                               TabGridCoordinatorDelegate,
                               WebStateListObserving,
                               LocationPermissionsCommands> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
}

// Navigation View controller for the settings.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The scene level component for url loading. Is passed down to
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) SceneUrlLoadingService* sceneURLLoadingService;

// Returns YES if the settings are presented, either from
// self.settingsNavigationController or from SigninCoordinator.
@property(nonatomic, assign, readonly, getter=isSettingsViewPresented)
    BOOL settingsViewPresented;

// Coordinator for displaying history.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;

// Coordinates the creation of PDF screenshots with the window's content.
@property(nonatomic, strong) ScreenshotDelegate* screenshotDelegate;

// The tab switcher command and the voice search commands can be sent by views
// that reside in a different UIWindow leading to the fact that the exclusive
// touch property will be ineffective and a command for processing both
// commands may be sent in the same run of the runloop leading to
// inconsistencies. Those two boolean indicate if one of those commands have
// been processed in the last 200ms in order to only allow processing one at
// a time.
// TODO(crbug.com/560296):  Provide a general solution for handling mutually
// exclusive chrome commands sent at nearly the same time.
@property(nonatomic, assign) BOOL isProcessingTabSwitcherCommand;
@property(nonatomic, assign) BOOL isProcessingVoiceSearchCommand;

// If not NONE, the current BVC should be switched to this BVC on completion
// of tab switcher dismissal.
@property(nonatomic, assign)
    TabSwitcherDismissalMode modeToDisplayOnTabSwitcherDismissal;

// A property to track whether the QR Scanner should be started upon tab
// switcher dismissal. It can only be YES if the QR Scanner experiment is
// enabled.
@property(nonatomic, readwrite)
    NTPTabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// The main coordinator, lazily created the first time it is accessed. Manages
// the main view controller. This property should not be accessed before the
// browser has started up to the FOREGROUND stage.
@property(nonatomic, strong) TabGridCoordinator* mainCoordinator;
// If YES, the tab switcher is currently active.
@property(nonatomic, assign, getter=isTabSwitcherActive)
    BOOL tabSwitcherIsActive;
// YES while animating the dismissal of tab switcher.
@property(nonatomic, assign) BOOL dismissingTabSwitcher;

// Wrangler to handle BVC and tab model creation, access, and related logic.
// Implements features exposed from this object through the
// BrowserViewInformation protocol.
@property(nonatomic, strong) BrowserViewWrangler* browserViewWrangler;
// The coordinator used to control sign-in UI flows. Lazily created the first
// time it is accessed.
@property(nonatomic, strong) SigninCoordinator* signinCoordinator;

@property(nonatomic, strong)
    LocationPermissionsCoordinator* locationPermissionsCoordinator;

// Additional product specific data used by UserFeedbackDataSource.
// TODO(crbug.com/1117041): Move this into a UserFeedback config object.
@property(nonatomic, strong)
    NSDictionary<NSString*, NSString*>* specificProductData;

@end

@implementation SceneController {
  // UI blocker used while FRE is shown in the scene controlled by this object.
  std::unique_ptr<ScopedUIBlocker> _firstRunUIBlocker;
}
@synthesize startupParameters = _startupParameters;

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _sceneState = sceneState;
    [_sceneState addObserver:self];
    [_sceneState.appState addObserver:self];

    // The window is necessary very early in the app/scene lifecycle, so it
    // should be created right away.
    // When multiwindow is supported, the window is created by SceneDelegate,
    // and fetched by SceneState from UIScene's windows.
    if (!IsSceneStartupSupported() && !self.sceneState.window) {
      self.sceneState.window = [[ChromeOverlayWindow alloc]
          initWithFrame:[[UIScreen mainScreen] bounds]];
      CustomizeUIWindowAppearance(self.sceneState.window);
    }
    _sceneURLLoadingService = new SceneUrlLoadingService();
    _sceneURLLoadingService->SetDelegate(self);

    _webStateListForwardingObserver =
        std::make_unique<WebStateListObserverBridge>(self);

    // Add agents.
    [_sceneState addAgent:[[UIBlockerSceneAgent alloc] init]];
    [_sceneState addAgent:[[IncognitoBlockerSceneAgent alloc] init]];
  }
  return self;
}

#pragma mark - Setters and getters

- (id<BrowsingDataCommands>)browsingDataCommandsHandler {
  return HandlerForProtocol(self.sceneState.appState.appCommandDispatcher,
                            BrowsingDataCommands);
}

- (TabGridCoordinator*)mainCoordinator {
  if (!_mainCoordinator) {
    // Lazily create the main coordinator.
    TabGridCoordinator* tabGridCoordinator = [[TabGridCoordinator alloc]
                     initWithWindow:self.sceneState.window
         applicationCommandEndpoint:self
        browsingDataCommandEndpoint:self.browsingDataCommandsHandler
                     regularBrowser:self.mainInterface.browser
                   incognitoBrowser:self.incognitoInterface.browser];
    _mainCoordinator = tabGridCoordinator;
    tabGridCoordinator.regularThumbStripAttacher = self.mainInterface.bvc;
    tabGridCoordinator.incognitoThumbStripAttacher =
        self.incognitoInterface.bvc;
  }
  return _mainCoordinator;
}

- (id<BrowserInterface>)mainInterface {
  return self.browserViewWrangler.mainInterface;
}

- (id<BrowserInterface>)currentInterface {
  return self.browserViewWrangler.currentInterface;
}

- (id<BrowserInterface>)incognitoInterface {
  return self.browserViewWrangler.incognitoInterface;
}

- (id<BrowserInterfaceProvider>)interfaceProvider {
  return self.browserViewWrangler;
}

- (BOOL)isSettingsViewPresented {
  return self.settingsNavigationController ||
         self.signinCoordinator.isSettingsViewPresented;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  AppState* appState = self.sceneState.appState;
  if (appState.isInSafeMode) {
    // Nothing at all should happen in safe mode. Code in
    // appStateDidExitSafeMode will ensure the updates happen once safe mode
    // ends.
    return;
  }
  BOOL initializingUIInColdStart = level > SceneActivationLevelBackground &&
                                   !self.sceneState.hasInitializedUI;
  if (initializingUIInColdStart) {
    [self initializeUI];
    if (IsMultiwindowSupported()) {
      if (@available(iOS 13, *)) {
        // Add the scene to the list of connected scene, to restore in case of
        // crashes.
        [[PreviousSessionInfo sharedInstance]
            addSceneSessionID:sceneState.scene.session.persistentIdentifier];
      }
    }
  }

  // When the scene transitions to inactive (such as when it's being shown in
  // the OS app-switcher), update the title for display on iPadOS.
  if (@available(iOS 13, *)) {
    if (level == SceneActivationLevelForegroundInactive) {
      sceneState.scene.title = [self displayTitleForAppSwitcher];
    }
  }

  if (level == SceneActivationLevelForegroundActive) {
    if (![self presentSigninUpgradePromoIfPossible]) {
      [self presentSignInAccountsViewControllerIfNecessary];
    }

    [self handleExternalIntents];

    if (!initializingUIInColdStart && self.tabSwitcherIsActive &&
        [self shouldOpenNTPTabOnActivationOfBrowser:self.currentInterface
                                                        .browser]) {
      DCHECK(!self.dismissingTabSwitcher);
      [self beginDismissingTabSwitcherWithCurrentBrowser:self.mainInterface
                                                             .browser
                                            focusOmnibox:NO];

      OpenNewTabCommand* command = [OpenNewTabCommand commandWithIncognito:NO];
      command.userInitiated = NO;
      Browser* browser = self.currentInterface.browser;
      id<ApplicationCommands> applicationHandler = HandlerForProtocol(
          browser->GetCommandDispatcher(), ApplicationCommands);
      [applicationHandler openURLInNewTab:command];
      [self finishDismissingTabSwitcher];
    }
  }

  if (sceneState.currentOrigin != WindowActivityRestoredOrigin) {
    if (IsMultiwindowSupported()) {
      if (@available(iOS 13, *)) {
        base::UmaHistogramEnumeration(kMultiWindowOpenInNewWindowHistogram,
                                      sceneState.currentOrigin);
      }
    }
  }

  if (self.sceneState.hasInitializedUI &&
      level == SceneActivationLevelUnattached) {
    if (IsMultiwindowSupported()) {
      if (@available(iOS 13, *)) {
        if (IsMultipleScenesSupported()) {
          // If Multiple scenes are not supported, the session shouldn't be
          // removed as it can be used for normal restoration.
          [[PreviousSessionInfo sharedInstance]
              removeSceneSessionID:sceneState.scene.session
                                       .persistentIdentifier];
        }
      }
    }
    [self teardownUI];
  }
}

- (void)handleExternalIntents {
  if (self.sceneState.appState.startupInformation.isPresentingFirstRunUI ||
      self.sceneState.presentingModalOverlay) {
    return;
  }
  if (IsSceneStartupSupported()) {
    if (@available(iOS 13, *)) {
      // Handle URL opening from
      // |UIWindowSceneDelegate scene:willConnectToSession:options:|.
      for (UIOpenURLContext* context in self.sceneState.connectionOptions
               .URLContexts) {
        URLOpenerParams* params =
            [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
        [self openTabFromLaunchWithParams:params
                       startupInformation:self.sceneState.appState
                                              .startupInformation
                                 appState:self.sceneState.appState];
      }
      if (self.sceneState.connectionOptions.shortcutItem) {
        [UserActivityHandler
            performActionForShortcutItem:self.sceneState.connectionOptions
                                             .shortcutItem
                       completionHandler:nil
                               tabOpener:self
                   connectionInformation:self
                      startupInformation:self.sceneState.appState
                                             .startupInformation
                       interfaceProvider:self.interfaceProvider];
      }

      // See if this scene launched as part of a multiwindow URL opening.
      // If so, load that URL (this also creates a new tab to load the URL
      // in). No other UI will show in this case.
      NSUserActivity* activityWithCompletion;
      for (NSUserActivity* activity in self.sceneState.connectionOptions
               .userActivities) {
        if (ActivityIsURLLoad(activity)) {
          UrlLoadParams params = LoadParamsFromActivity(activity);
          ApplicationMode mode = params.in_incognito
                                     ? ApplicationMode::INCOGNITO
                                     : ApplicationMode::NORMAL;
          [self openOrReuseTabInMode:mode
                   withUrlLoadParams:params
                 tabOpenedCompletion:nil];
        } else if (ActivityIsTabMove(activity)) {
          NSString* tabID = GetTabIDFromActivity(activity);
          MoveTabToBrowser(tabID, self.mainInterface.browser,
                           /*destination_tab_index=*/0);
        } else if (!activityWithCompletion) {
          // Completion involves user interaction.
          // Only one can be triggered.
          activityWithCompletion = activity;
        }
      }
      if (activityWithCompletion) {
        // This function is called when the scene is activated (or unblocked).
        // Consider the scene as still not active at this point as the handling
        // of startup parameters is not yet done (and will be later in this
        // function).
        [UserActivityHandler
             continueUserActivity:activityWithCompletion
              applicationIsActive:NO
                        tabOpener:self
            connectionInformation:self
               startupInformation:self.sceneState.appState.startupInformation
                     browserState:self.currentInterface.browserState];
      }
      self.sceneState.connectionOptions = nil;
    }

    if (self.startupParameters) {
      [UserActivityHandler
          handleStartupParametersWithTabOpener:self
                         connectionInformation:self
                            startupInformation:self.sceneState.appState
                                                   .startupInformation
                                  browserState:self.currentInterface
                                                   .browserState];
    }

  } else {
    NSDictionary* launchOptions =
        self.sceneState.appState.startupInformation.launchOptions;
    URLOpenerParams* params =
        [[URLOpenerParams alloc] initWithLaunchOptions:launchOptions];
    [self
        openTabFromLaunchWithParams:params
                 startupInformation:self.sceneState.appState.startupInformation
                           appState:self.sceneState.appState];
  }
}

- (BOOL)presentSignInAccountsViewControllerIfNecessary {
  ChromeBrowserState* browserState = self.currentInterface.browserState;
  DCHECK(browserState);
  if ([SignedInAccountsViewController
          shouldBePresentedForBrowserState:browserState]) {
    [self presentSignedInAccountsViewControllerForBrowserState:browserState];
    return YES;
  }
  return NO;
}

- (void)sceneState:(SceneState*)sceneState
    hasPendingURLs:(NSSet<UIOpenURLContext*>*)URLContexts
    API_AVAILABLE(ios(13)) {
  DCHECK(URLContexts);
  // It is necessary to reset the URLContextsToOpen after opening them.
  // Handle the opening asynchronously to avoid interfering with potential
  // other observers.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self openURLContexts:sceneState.URLContextsToOpen];
    self.sceneState.URLContextsToOpen = nil;
  });
}

- (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
    API_AVAILABLE(ios(13)) {
  self.sceneState.startupHadExternalIntent = YES;
  [UserActivityHandler
      performActionForShortcutItem:shortcutItem
                 completionHandler:completionHandler
                         tabOpener:self
             connectionInformation:self
                startupInformation:self.sceneState.appState.startupInformation
                 interfaceProvider:self.interfaceProvider];
}

- (void)sceneState:(SceneState*)sceneState
    receivedUserActivity:(NSUserActivity*)userActivity {
  if (self.sceneState.appState.isInSafeMode || !userActivity) {
    return;
  }
  BOOL sceneIsActive =
      self.sceneState.activationLevel >= SceneActivationLevelForegroundActive;
  if (self.sceneState.appState.startupInformation.isPresentingFirstRunUI ||
      self.sceneState.presentingModalOverlay) {
    sceneIsActive = NO;
  }
  self.sceneState.startupHadExternalIntent = YES;
  [UserActivityHandler
       continueUserActivity:userActivity
        applicationIsActive:sceneIsActive
                  tabOpener:self
      connectionInformation:self
         startupInformation:self.sceneState.appState.startupInformation
               browserState:self.currentInterface.browserState];
  if (sceneIsActive) {
    // It is necessary to reset the pendingUserActivity after handling it.
    // Handle the reset asynchronously to avoid interfering with other
    // observers.
    dispatch_async(dispatch_get_main_queue(), ^{
      self.sceneState.pendingUserActivity = nil;
    });
  }
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  if (self.sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    [self handleExternalIntents];
  }
}

#pragma mark - AppStateObserver

- (void)appStateDidExitSafeMode:(AppState*)appState {
  // All events were postponed in safe mode. Resend them.
  [self sceneState:self.sceneState
      transitionedToActivationLevel:self.sceneState.activationLevel];
}

#pragma mark - private

- (void)initializeUI {
  if (self.sceneState.hasInitializedUI) {
    return;
  }

  [self startUpChromeUI];
  self.sceneState.hasInitializedUI = YES;
}

// Returns YES if restore prompt can be shown.
// The restore prompt shouldn't appear if its appearance may be in conflict
// with the expected behavior by the user.
// The following cases will not show restore prompt:
//   1- New tab / Navigation startup parameters are specified.
//   2- Load URL User activity is queud.
//   3- Move tab user activity is queued.
// In these cases if a restore prompt was shown, it may be dismissed immediately
// and the user will not have a chance to restore the session.
- (BOOL)shouldShowRestorePrompt {
  BOOL shouldShow = !self.startupParameters;
  if (shouldShow && IsSceneStartupSupported()) {
    if (@available(iOS 13, *)) {
      for (NSUserActivity* activity in self.sceneState.connectionOptions
               .userActivities) {
        if (ActivityIsTabMove(activity) || ActivityIsURLLoad(activity)) {
          shouldShow = NO;
          break;
        }
      }
    }
  }
  return shouldShow;
}

// Starts up a single chrome window and its UI.
- (void)startUpChromeUI {
  DCHECK(!self.browserViewWrangler);
  DCHECK(self.sceneURLLoadingService);
  DCHECK(self.sceneState.appState.mainBrowserState);

  self.browserViewWrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:self.sceneState.appState.mainBrowserState
                       sceneState:self.sceneState
       applicationCommandEndpoint:self
      browsingDataCommandEndpoint:self.browsingDataCommandsHandler];

  // Ensure the main browser is created. This also creates the BVC.
  [self.browserViewWrangler createMainBrowser];

  // Add Scene Agent that requires CommandDispatcher.
  [self.sceneState
      addAgent:[[DefaultBrowserSceneAgent alloc]
                   initWithCommandDispatcher:self.mainInterface.browser
                                                 ->GetCommandDispatcher()]];

  if (IsSceneStartupSupported() &&
      base::FeatureList::IsEnabled(kEnableFullPageScreenshot)) {
    if (@available(iOS 13, *)) {
      self.screenshotDelegate = [[ScreenshotDelegate alloc]
          initWithBrowserInterfaceProvider:self.browserViewWrangler];
      [self.sceneState.scene.screenshotService
          setDelegate:self.screenshotDelegate];
    }
  }

  // Only create the restoration helper if the session with the current session
  // id was backed up successfully.
  if (self.sceneState.appState.sessionRestorationRequired) {
    if (!IsMultiwindowSupported() ||
        [CrashRestoreHelper
            isBackedUpSessionID:self.sceneState.sceneSessionID]) {
      self.sceneState.appState.startupInformation.restoreHelper =
          [[CrashRestoreHelper alloc]
              initWithBrowser:self.mainInterface.browser];
    }
  }

  // Make sure the launch mode is correct and consistent with the mode used
  // when the application was terminated. It is possible for the incognito
  // UI to have been presented but with no tabs (e.g. the tab switcher was
  // active and user closed the last tab). In that case, switch to regular
  // UI. Also, if the app crashed, always switch back to regular UI.
  const BOOL startInIncognito =
      self.sceneState.incognitoContentVisible &&
      !self.sceneState.appState.postCrashLaunch &&
      !self.interfaceProvider.incognitoInterface.browser->GetWebStateList()
           ->empty();

  // If the application crashed, clear incognito state.
  if (self.sceneState.appState.postCrashLaunch)
    [self clearIOSSpecificIncognitoData];

  [self createInitialUI:(startInIncognito ? ApplicationMode::INCOGNITO
                                          : ApplicationMode::NORMAL)];

  // By reaching here, it's guaranteed that both Normal and incognito sessions
  // were restored. and if it's the first time that multi-window sessions are
  // created, the migration was done. Update the Multi-Window flag so it's not
  // used again in the same run when creating new windows.
  // TODO(crbug.com/1109280): Remove after the migration to Multi-Window
  // sessions is done.
  [[PreviousSessionInfo sharedInstance] updateMultiWindowSupportStatus];

  if ([self shouldShowRestorePrompt]) {
    [self.sceneState.appState.startupInformation
            .restoreHelper showRestorePrompt];
    self.sceneState.appState.startupInformation.restoreHelper = nil;
  }
}

// Determines which UI should be shown on startup, and shows it.
- (void)createInitialUI:(ApplicationMode)launchMode {
  DCHECK(self.sceneState.appState.mainBrowserState);

  // Set the Scene application URL loader on the URL loading browser interface
  // for the regular and incognito interfaces. This will lazily instantiate the
  // incognito interface if it isn't already created.
  UrlLoadingBrowserAgent::FromBrowser(self.mainInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  // Observe the web state lists for both browsers.
  self.mainInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());
  self.incognitoInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());

  // Enables UI initializations to query the keyWindow's size.
  [self.sceneState.window makeKeyAndVisible];

  // Lazy init of mainCoordinator.
  [self.mainCoordinator start];

  [self.mainCoordinator setActivePage:[self activePage]];

  // Decide if the First Run UI needs to run.
  const bool firstRun = ShouldPresentFirstRunExperience();

  Browser* browser;
  if (launchMode == ApplicationMode::INCOGNITO) {
    browser = self.incognitoInterface.browser;
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  } else {
    browser = self.mainInterface.browser;
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }

  // Figure out what UI to show initially.

  if (self.tabSwitcherIsActive) {
    DCHECK(!self.dismissingTabSwitcher);
    [self
        beginDismissingTabSwitcherWithCurrentBrowser:self.mainInterface.browser
                                        focusOmnibox:NO];
    [self finishDismissingTabSwitcher];
  }

  // If this is first run, or if this web state list should have an NTP created
  // when it activates, then create that tab.
  if (firstRun || [self shouldOpenNTPTabOnActivationOfBrowser:browser]) {
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:self.currentInterface.incognito];
    command.userInitiated = NO;
    Browser* browser = self.currentInterface.browser;
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  }

  // If this is first run, show the first run UI on top of the new tab.
  // If this isn't first run, check if the sign-in promo needs to display.
  if (firstRun && launchMode != ApplicationMode::INCOGNITO &&
      !self.sceneState.appState.startupInformation.isPresentingFirstRunUI) {
    [self showFirstRunUI];
    // Do not ever show the 'restore' infobar during first run.
    self.sceneState.appState.startupInformation.restoreHelper = nil;
  }

  // If skipping first run, not in Safe Mode, and the launch is not after a
  // crash, consider showing the default browser promo.
  if (!firstRun && !self.sceneState.appState.isInSafeMode &&
      !self.sceneState.appState.postCrashLaunch) {
    // Show the Default Browser promo UI if the user's past behavior fits
    // the categorization of potentially interested users or if the user is
    // signed in. Do not show if it is determined that Chrome is already the
    // default browser or if the user has already seen the promo UI.
    BOOL isEligibleUser = IsLikelyInterestedDefaultBrowserUser() ||
                          ios::GetChromeBrowserProvider()
                              ->GetChromeIdentityService()
                              ->HasIdentities();
    if (!IsChromeLikelyDefaultBrowser() &&
        !HasUserInteractedWithFullscreenPromoBefore() && isEligibleUser) {
      self.sceneState.appState.shouldShowDefaultBrowserPromo = YES;
    }
  }
}

// This method completely destroys all of the UI. It should be called when the
// scene is disconnected.
- (void)teardownUI {
  if (!self.sceneState.hasInitializedUI) {
    return;  // Nothing to do.
  }

  // The UI should be stopped before the models they observe are stopped.
  [self.signinCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:nil];
  self.signinCoordinator = nil;

  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  // Force close the settings if open. This gives Settings the opportunity to
  // unregister observers and destroy C++ objects before the application is
  // shut down without depending on non-deterministic call to -dealloc.
  [self settingsWasDismissed];

  [_mainCoordinator stop];
  _mainCoordinator = nil;

  // Stop observing web state list changes before tearing down the web state
  // lists.
  self.mainInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());
  self.incognitoInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());

  [self.browserViewWrangler shutdown];
  self.browserViewWrangler = nil;

  self.sceneState.hasInitializedUI = NO;

  [self.sceneState.appState removeObserver:self];
}

// Formats string for display on iPadOS application switcher with the
// domain of the foreground tab and the tab count. Assumes the scene is
// visible. Will return nil if there are no tabs.
- (NSString*)displayTitleForAppSwitcher {
  DCHECK(self.currentInterface.browser);
  web::WebState* webState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (!webState)
    return nil;

  // At this point there is at least one tab.
  int numberOfTabs = self.currentInterface.browser->GetWebStateList()->count();
  DCHECK(numberOfTabs > 0);
  GURL url = webState->GetVisibleURL();
  base::string16 urlText = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlTrimAfterHost,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  base::string16 pattern =
      l10n_util::GetStringUTF16(IDS_IOS_APP_SWITCHER_SCENE_TITLE);
  base::string16 formattedTitle =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          pattern, "domain", urlText, "count", numberOfTabs - 1);
  return base::SysUTF16ToNSString(formattedTitle);
}

- (void)logLocationPermissionsExperimentForGroupShown:
    (LocationPermissionsUI)experimentGroup {
  UMA_HISTOGRAM_ENUMERATION("IOS.LocationPermissionsUI", experimentGroup);
}

#pragma mark - First Run

// Initializes the first run UI and presents it to the user.
- (void)showFirstRunUI {
  DCHECK(!self.signinCoordinator);
  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker = std::make_unique<ScopedUIBlocker>(self.sceneState);
  // Register for the first run dismissal notification to reset
  // |sceneState.presentingFirstRunUI| flag;
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIWillFinish)
             name:kChromeFirstRunUIWillFinishNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIDidFinish)
             name:kChromeFirstRunUIDidFinishNotification
           object:nil];

  Browser* browser = self.mainInterface.browser;
  id<ApplicationCommands, BrowsingDataCommands> welcomeHandler =
      static_cast<id<ApplicationCommands, BrowsingDataCommands>>(
          browser->GetCommandDispatcher());

  WelcomeToChromeViewController* welcomeToChrome =
      [[WelcomeToChromeViewController alloc]
          initWithBrowser:browser
                presenter:self.mainInterface.bvc
               dispatcher:welcomeHandler];
  UINavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:welcomeToChrome];
  [navController setModalTransitionStyle:UIModalTransitionStyleCrossDissolve];
  navController.modalPresentationStyle = UIModalPresentationFullScreen;
  CGRect appFrame = [[UIScreen mainScreen] bounds];
  [[navController view] setFrame:appFrame];
  self.sceneState.presentingFirstRunUI = YES;
  [self.mainInterface.viewController presentViewController:navController
                                                  animated:NO
                                                completion:nil];
}

// Sets a LocalState pref marking the TOS EULA as accepted.
- (void)markEulaAsAccepted {
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  if (!prefs->GetBoolean(prefs::kEulaAccepted))
    prefs->SetBoolean(prefs::kEulaAccepted, true);
  prefs->CommitPendingWrite();
}

- (void)handleFirstRunUIWillFinish {
  [self markEulaAsAccepted];

  DCHECK(self.sceneState.presentingFirstRunUI);
  _firstRunUIBlocker.reset();
  self.sceneState.presentingFirstRunUI = NO;
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIWillFinishNotification
              object:nil];
  if (self.sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    [self handleExternalIntents];
  }
}

// Handles the notification that first run modal dialog UI completed.
- (void)handleFirstRunUIDidFinish {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIDidFinishNotification
              object:nil];

  if (!location_permissions_field_trial::IsInRemoveFirstRunPromptGroup() &&
      !location_permissions_field_trial::IsInFirstRunModalGroup()) {
    [self logLocationPermissionsExperimentForGroupShown:
              LocationPermissionsUI::kFirstRunPromptNotShown];
    // As soon as First Run has finished, give OmniboxGeolocationController an
    // opportunity to present the iOS system location alert.
    [[OmniboxGeolocationController sharedInstance]
        triggerSystemPromptForNewUser:YES];
  } else if (location_permissions_field_trial::
                 IsInRemoveFirstRunPromptGroup()) {
    // If in RemoveFirstRunPrompt group, the system prompt will be delayed until
    // the site requests location information.
    [[OmniboxGeolocationController sharedInstance]
        systemPromptSkippedForNewUser];
  }
}

// Presents the sign-in upgrade promo if is relevant and possible.
// Returns YES if the promo is shown.
- (BOOL)presentSigninUpgradePromoIfPossible {
  if (!SigninShouldPresentUserSigninUpgrade(
          self.sceneState.appState.mainBrowserState))
    return NO;
  // Don't show promos if first run is shown in any scene.  (Note:  This flag
  // is only YES while the first run UI is visible.  However, as this function
  // is called immediately after the UI is shown, it's a safe check.)
  for (SceneState* sceneState in self.sceneState.appState.connectedScenes) {
    if (sceneState.presentingFirstRunUI) {
      return NO;
    }
  }
  // Don't show the promo if there is a blocking task in process.
  if (self.sceneState.appState.currentUIBlocker)
    return NO;
  // Don't show the promo in Incognito mode.
  if (self.currentInterface == self.incognitoInterface)
    return NO;
  // Don't show promos if the app was launched from a URL.
  if (self.startupParameters)
    return NO;
  // Don't show the promo if the window is not active.
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive)
    return NO;
  // Don't show the promo if already presented.
  if (self.sceneState.appState.signinUpgradePromoPresentedOnce)
    return NO;
  self.sceneState.appState.signinUpgradePromoPresentedOnce = YES;
  DCHECK(!self.signinCoordinator);
  Browser* browser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      upgradeSigninPromoCoordinatorWithBaseViewController:self.mainInterface
                                                              .viewController
                                                  browser:browser];
  [self startSigninCoordinatorWithCompletion:nil];
  return YES;
}

#pragma mark - ApplicationCommands

- (void)dismissModalDialogs {
  [self dismissModalDialogsWithCompletion:nil dismissOmnibox:YES];
}

- (void)showHistory {
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.currentInterface.viewController
                         browser:self.mainInterface.browser];
  self.historyCoordinator.loadStrategy =
      self.currentInterface.incognito ? UrlLoadStrategy::ALWAYS_IN_INCOGNITO
                                      : UrlLoadStrategy::NORMAL;
  [self.historyCoordinator start];
}

// Opens an url from a link in the settings UI.
- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command {
  [self openUrlFromSettings:command];
}

- (void)closeSettingsUI {
  [self closeSettingsAnimated:YES completion:nullptr];
}

- (void)prepareTabSwitcher {
  web::WebState* currentWebState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    BOOL loading = currentWebState->IsLoading();
    SnapshotTabHelper::FromWebState(currentWebState)
        ->UpdateSnapshotWithCallback(^(UIImage* snapshot) {
          EnterTabSwitcherSnapshotResult snapshotResult;
          if (loading && !snapshot) {
            snapshotResult =
                EnterTabSwitcherSnapshotResult::kPageLoadingAndSnapshotFailed;
          } else if (loading && snapshot) {
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageLoadingAndSnapshotSucceeded;
          } else if (!loading && !snapshot) {
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageNotLoadingAndSnapshotFailed;
          } else {
            DCHECK(!loading && snapshot);
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageNotLoadingAndSnapshotSucceeded;
          }
          UMA_HISTOGRAM_ENUMERATION("IOS.EnterTabSwitcherSnapshotResult",
                                    snapshotResult);
        });
  }
  [self.mainCoordinator prepareToShowTabGrid];
}

- (void)displayTabSwitcherInGridLayout {
  if (!IsThumbStripEnabled()) {
    // When the thumb strip feature is enabled, |self.tabSwitcherIsActive| could
    // be YES if the tab switcher button is tapped while the thumb strip is
    // visible, or it could be NO if tapped while thumb strip is hidden.
    // Otherwise, when the thumb strip feature is disabled,
    // |self.tabSwitcherIsActive| should always be NO at this point in code.
    DCHECK(!self.tabSwitcherIsActive);
  }
  if (!self.isProcessingVoiceSearchCommand) {
    [self.currentInterface.bvc userEnteredTabSwitcher];
    [self showTabSwitcher];
    self.isProcessingTabSwitcherCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.isProcessingTabSwitcherCommand = NO;
                   });
  }
}

// TODO(crbug.com/779791) : Remove showing settings from MainController.
- (void)showAutofillSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController)
    return;

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController autofillProfileControllerForBrowser:browser
                                                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender {
  [self showReportAnIssueFromViewController:baseViewController
                                     sender:sender
                        specificProductData:nil];
}

- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData {
  DCHECK(baseViewController);
  self.specificProductData = specificProductData;
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  dispatch_async(dispatch_get_main_queue(), ^{
    DCHECK(!self.signinCoordinator);
    if (self.settingsNavigationController)
      return;
    Browser* browser = self.mainInterface.browser;
    self.settingsNavigationController =
        [SettingsNavigationController userFeedbackControllerForBrowser:browser
                                                              delegate:self
                                                    feedbackDataSource:self
                                                                sender:sender
                                                               handler:self];
    [baseViewController presentViewController:self.settingsNavigationController
                                     animated:YES
                                   completion:nil];
  });
}

- (void)openURLInNewTab:(OpenNewTabCommand*)command {
  UrlLoadParams params =
      UrlLoadParams::InNewTab(command.URL, command.virtualURL);
  params.SetInBackground(command.inBackground);
  params.web_params.referrer = command.referrer;
  params.in_incognito = command.inIncognito;
  params.append_to = command.appendTo;
  params.origin_point = command.originPoint;
  params.from_chrome = command.fromChrome;
  params.user_initiated = command.userInitiated;
  params.should_focus_omnibox = command.shouldFocusOmnibox;
  self.sceneURLLoadingService->LoadUrlInNewTab(params);
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  Browser* mainBrowser = self.mainInterface.browser;

  switch (command.operation) {
    case AUTHENTICATION_OPERATION_REAUTHENTICATE:
      self.signinCoordinator = [SigninCoordinator
          reAuthenticationCoordinatorWithBaseViewController:baseViewController
                                                    browser:mainBrowser
                                                accessPoint:command.accessPoint
                                                promoAction:command
                                                                .promoAction];
      break;
    case AUTHENTICATION_OPERATION_SIGNIN:
      self.signinCoordinator = [SigninCoordinator
          userSigninCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                             identity:command.identity
                                          accessPoint:command.accessPoint
                                          promoAction:command.promoAction];
      break;
    case AUTHENTICATION_OPERATION_ADD_ACCOUNT:
      self.signinCoordinator = [SigninCoordinator
          addAccountCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                          accessPoint:command.accessPoint];
      break;
  }
  [self startSigninCoordinatorWithCompletion:command.callback];
}

- (void)showAdvancedSigninSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  Browser* mainBrowser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:baseViewController
                                                      browser:mainBrowser];
  [self startSigninCoordinatorWithCompletion:^(BOOL success) {
    if (location_permissions_field_trial::IsInFirstRunModalGroup()) {
      [self showLocationPermissionsFromViewController:baseViewController];
    }
  }];
}

- (void)showLocationPermissionsFromViewController:
    (UIViewController*)baseViewController {
  [self logLocationPermissionsExperimentForGroupShown:LocationPermissionsUI::
                                                          kFirstRunModal];
  self.locationPermissionsCoordinator = [[LocationPermissionsCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.mainInterface.browser];
  self.locationPermissionsCoordinator.handler = self;
  [self.locationPermissionsCoordinator start];
}

- (void)dismissLocationPermissionsExplanationModal {
  [self.locationPermissionsCoordinator stop];
  self.locationPermissionsCoordinator = nil;
}

- (void)
    showTrustedVaultReauthenticationFromViewController:
        (UIViewController*)baseViewController
                                      retrievalTrigger:
                                          (syncer::KeyRetrievalTriggerForUMA)
                                              retrievalTrigger {
  DCHECK(!self.signinCoordinator);
  Browser* mainBrowser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordiantorWithBaseViewController:
          baseViewController
                                                            browser:mainBrowser
                                                   retrievalTrigger:
                                                       retrievalTrigger];
  [self startSigninCoordinatorWithCompletion:nil];
}

// TODO(crbug.com/779791) : Remove settings commands from MainController.
- (void)showAddAccountFromViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  self.signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:baseViewController
                                          browser:self.mainInterface.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_UNKNOWN];

  [self startSigninCoordinatorWithCompletion:nil];
}

- (void)showConsistencyPromoFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  self.signinCoordinator = [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:baseViewController
                                                      browser:self.mainInterface
                                                                  .browser];
  [self startSigninCoordinatorWithCompletion:nil];
}

- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController {
  web::WebState* webState =
      self.mainInterface.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(webState);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  DCHECK(infoBarManager);
  SigninNotificationInfoBarDelegate::Create(
      infoBarManager, self.mainInterface.browser->GetBrowserState(), self,
      baseViewController);
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  self.sceneState.incognitoContentVisible = incognitoContentVisible;
}

- (void)startVoiceSearch {
  if (!self.isProcessingTabSwitcherCommand) {
    [self startVoiceSearchInCurrentBVC];
    self.isProcessingVoiceSearchCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.isProcessingVoiceSearchCommand = NO;
                   });
  }
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController)
    return;
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];

  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController mainSettingsControllerForBrowser:browser
                                                            delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity {
  if (!IsMultipleScenesSupported())
    return;  // silent no-op.

  if (@available(iOS 13, *)) {
    UISceneActivationRequestOptions* options =
        [[UISceneActivationRequestOptions alloc] init];
    options.requestingScene = self.sceneState.scene;

    [UIApplication.sharedApplication
        requestSceneSessionActivation:nil /* make a new scene */
                         userActivity:userActivity
                              options:options
                         errorHandler:nil];
  }
}

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Remove show settings from MainController.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (!baseViewController) {
    DCHECK_EQ(self.currentInterface.viewController,
              self.mainCoordinator.activeViewController);
    baseViewController = self.currentInterface.viewController;
  }

  if (self.currentInterface.incognito) {
    NOTREACHED();
    return;
  }
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController accountsControllerForBrowser:browser
                                                        delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove Google services settings from MainController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (!baseViewController) {
    DCHECK_EQ(self.currentInterface.viewController,
              self.mainCoordinator.activeViewController);
    baseViewController = self.currentInterface.viewController;
  }

  if (self.settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [self.settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController googleServicesControllerForBrowser:browser
                                                              delegate:self];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController syncPassphraseControllerForBrowser:browser
                                                              delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (!baseViewController) {
    // TODO(crbug.com/779791): Don't pass base view controller through
    // dispatched command.
    baseViewController = self.currentInterface.viewController;
  }
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController savePasswordsControllerForBrowser:browser
                                                             delegate:self
                                      startPasswordCheckAutomatically:NO];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  [self dismissModalDialogs];
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
            baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController savePasswordsControllerForBrowser:browser
                                                             delegate:self
                                      startPasswordCheckAutomatically:YES];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController autofillProfileControllerForBrowser:browser
                                                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showCreditCardSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showCreditCardSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardControllerForBrowser:browser
                                    delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

#pragma mark - ApplicationCommandsHelpers

- (void)openUrlFromSettings:(OpenNewTabCommand*)command {
  DCHECK([command fromChrome]);
  UrlLoadParams params = UrlLoadParams::InNewTab([command URL]);
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  ProceduralBlock completion = ^{
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                    NORMAL
                              withUrlLoadParams:params
                                 dismissOmnibox:YES
                                     completion:nil];
  };
  [self closeSettingsAnimated:YES completion:completion];
}

#pragma mark - UserFeedbackDataSource

- (BOOL)currentPageIsIncognito {
  return self.currentInterface.incognito;
}

- (NSString*)currentPageDisplayURL {
  if (self.tabSwitcherIsActive)
    return nil;
  web::WebState* webState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (!webState)
    return nil;
  // Returns URL of browser tab that is currently showing.
  GURL url = webState->GetVisibleURL();
  base::string16 urlText = url_formatter::FormatUrl(url);
  return base::SysUTF16ToNSString(urlText);
}

- (UIImage*)currentPageScreenshot {
  UIView* lastView = self.mainCoordinator.activeViewController.view;
  DCHECK(lastView);
  CGFloat scale = 0.0;
  // For screenshots of the tab switcher we need to use a scale of 1.0 to avoid
  // spending too much time since the tab switcher can have lots of subviews.
  if (self.tabSwitcherIsActive)
    scale = 1.0;
  return CaptureView(lastView, scale);
}

- (NSString*)currentPageSyncedUserName {
  ChromeBrowserState* browserState = self.currentInterface.browserState;
  if (browserState->IsOffTheRecord())
    return nil;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  std::string username = identity_manager->GetPrimaryAccountInfo().email;
  return username.empty() ? nil : base::SysUTF8ToNSString(username);
}

- (NSDictionary<NSString*, NSString*>*)specificProductData {
  return _specificProductData;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (void)settingsWasDismissed {
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
}

- (id<ApplicationCommands, BrowserCommands>)handlerForSettings {
  // Assume that settings always wants the dispatcher from the main BVC.
  return static_cast<id<ApplicationCommands, BrowserCommands>>(
      self.mainInterface.browser->GetCommandDispatcher());
}

#pragma mark - TabGridCoordinatorDelegate

- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldFinishWithBrowser:(Browser*)browser
               focusOmnibox:(BOOL)focusOmnibox {
  [self beginDismissingTabSwitcherWithCurrentBrowser:browser
                                        focusOmnibox:focusOmnibox];
}

- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid {
  [self finishDismissingTabSwitcher];
}

// Begins the process of dismissing the tab switcher with the given current
// model, switching which BVC is suspended if necessary, but not updating the
// UI.  The omnibox will be focused after the tab switcher dismissal is
// completed if |focusOmnibox| is YES.
- (void)beginDismissingTabSwitcherWithCurrentBrowser:(Browser*)browser
                                        focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(browser == self.mainInterface.browser ||
         browser == self.incognitoInterface.browser);

  self.dismissingTabSwitcher = YES;
  ApplicationMode mode = (browser == self.mainInterface.browser)
                             ? ApplicationMode::NORMAL
                             : ApplicationMode::INCOGNITO;
  [self setCurrentInterfaceForMode:mode];

  // The call to set currentBVC above does not actually display the BVC, because
  // _dismissingTabSwitcher is YES.  So: Force the BVC transition to start.
  [self displayCurrentBVCAndFocusOmnibox:focusOmnibox];
}

// Completes the process of dismissing the tab switcher, removing it from the
// screen and showing the appropriate BVC.
- (void)finishDismissingTabSwitcher {
  // In real world devices, it is possible to have an empty tab model at the
  // finishing block of a BVC presentation animation. This can happen when the
  // following occur: a) There is JS that closes the last incognito tab, b) that
  // JS was paused while the user was in the tab switcher, c) the user enters
  // the tab, activating the JS while the tab is being presented. Effectively,
  // the BVC finishes the presentation animation, but there are no tabs to
  // display. The only appropriate action is to dismiss the BVC and return the
  // user to the tab switcher.
  if (self.currentInterface.browser &&
      self.currentInterface.browser->GetWebStateList() &&
      self.currentInterface.browser->GetWebStateList()->count() == 0U) {
    self.tabSwitcherIsActive = NO;
    self.dismissingTabSwitcher = NO;
    self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;
    self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
    [self showTabSwitcher];
    return;
  }

  // The tab switcher dismissal animation runs
  // as part of the BVC presentation process.  The BVC is presented before the
  // animations begin, so it should be the current active VC at this point.
  DCHECK_EQ(self.mainCoordinator.activeViewController,
            self.currentInterface.viewController);

  if (self.modeToDisplayOnTabSwitcherDismissal ==
      TabSwitcherDismissalMode::NORMAL) {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  } else if (self.modeToDisplayOnTabSwitcherDismissal ==
             TabSwitcherDismissalMode::INCOGNITO) {
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  }

  self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;

  ProceduralBlock action = [self completionBlockForTriggeringAction:
                                     self.NTPActionAfterTabSwitcherDismissal];
  self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
  if (action) {
    action();
  }

  self.tabSwitcherIsActive = NO;
  self.dismissingTabSwitcher = NO;
}

#pragma mark Tab opening utility methods.

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action {
  switch (action) {
    case START_VOICE_SEARCH:
      return ^{
        [self startVoiceSearchInCurrentBVC];
      };
    case START_QR_CODE_SCANNER:
      return ^{
        if (!self.currentInterface.browser) {
          return;
        }
        id<QRScannerCommands> QRHandler = HandlerForProtocol(
            self.currentInterface.browser->GetCommandDispatcher(),
            QRScannerCommands);
        [QRHandler showQRScanner];
      };
    case FOCUS_OMNIBOX:
      return ^{
        if (!self.currentInterface.browser) {
          return;
        }
        id<OmniboxCommands> focusHandler = HandlerForProtocol(
            self.currentInterface.browser->GetCommandDispatcher(),
            OmniboxCommands);
        [focusHandler focusOmnibox];
      };
    default:
      return nil;
  }
}

// Starts a voice search on the current BVC.
- (void)startVoiceSearchInCurrentBVC {
  // If the background (non-current) BVC is playing TTS audio, call
  // -startVoiceSearch on it to stop the TTS.
  BrowserViewController* backgroundBVC =
      self.mainInterface == self.currentInterface ? self.incognitoInterface.bvc
                                                  : self.mainInterface.bvc;
  if (backgroundBVC.playingTTS)
    [backgroundBVC startVoiceSearch];
  else
    [self.currentInterface.bvc startVoiceSearch];
}

#pragma mark - TabSwitching

- (BOOL)openNewTabFromTabSwitcher {
  if (!self.mainCoordinator)
    return NO;

  UrlLoadParams urlLoadParams =
      UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  urlLoadParams.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;

  [self addANewTabAndPresentBrowser:self.mainInterface.browser
                  withURLLoadParams:urlLoadParams];
  return YES;
}

#pragma mark - TabOpening implementation.

- (void)dismissModalsAndOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                            withUrlLoadParams:
                                (const UrlLoadParams&)urlLoadParams
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   completion:(ProceduralBlock)completion {
  UrlLoadParams copyOfUrlLoadParams = urlLoadParams;
  [self
      dismissModalDialogsWithCompletion:^{
        [self openSelectedTabInMode:targetMode
                  withUrlLoadParams:copyOfUrlLoadParams
                         completion:completion];
      }
                         dismissOmnibox:dismissOmnibox];
}

- (void)dismissModalsAndOpenMultipleTabsInMode:
            (ApplicationModeForTabOpening)targetMode
                                          URLs:(const std::vector<GURL>&)URLs
                                dismissOmnibox:(BOOL)dismissOmnibox
                                    completion:(ProceduralBlock)completion {
  __weak SceneController* weakSelf = self;

  std::vector<GURL> copyURLs = URLs;

  id<BrowserInterface> targetInterface =
      [self extractInterfaceBaseOnMode:targetMode];

  web::WebState* currentWebState =
      targetInterface.browser->GetWebStateList()->GetActiveWebState();

  if (currentWebState) {
    web::NavigationManager* navigation_manager =
        currentWebState->GetNavigationManager();
    // Check if the current tab is in the procress of restoration and whether it
    // is an NTP. If so, add the tabs-opening action to the
    // RestoreCompletionCallback queue so that the tabs are opened only after
    // the NTP finishes restoring. This is to avoid an edge where multiple tabs
    // are trying to open in the middle of NTP restoration, as this will cause
    // all tabs trying to load into the same NTP, causing a race condition that
    // results in wrong behavior.
    if (navigation_manager->IsRestoreSessionInProgress() &&
        IsURLNtp(currentWebState->GetVisibleURL())) {
      navigation_manager->AddRestoreCompletionCallback(base::BindOnce(^{
        [self
            dismissModalDialogsWithCompletion:^{
              [weakSelf openMultipleTabsInMode:targetMode
                                          URLs:copyURLs
                                    completion:completion];
            }
                               dismissOmnibox:dismissOmnibox];
      }));
      return;
    }
  }

  [self
      dismissModalDialogsWithCompletion:^{
        [weakSelf openMultipleTabsInMode:targetMode
                                    URLs:copyURLs
                              completion:completion];
      }
                         dismissOmnibox:dismissOmnibox];
}

- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation
                           appState:(AppState*)appState {
  if (params) {
    [URLOpener handleLaunchOptions:params
                         tabOpener:self
             connectionInformation:self
                startupInformation:startupInformation
                          appState:appState];
  }
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  WebStateList* webStateList = self.mainInterface.browser->GetWebStateList();
  return webStateList && webStateList->GetIndexOfWebStateWithURL(URL) !=
                             WebStateList::kInvalidIndex;
}

- (BOOL)shouldOpenNTPTabOnActivationOfBrowser:(Browser*)browser {
  // Check if there are pending actions that would result in opening a new tab.
  // In that case, it is not useful to open another tab.
  if (IsSceneStartupSupported()) {
    if (@available(iOS 13, *)) {
      for (NSUserActivity* activity in self.sceneState.connectionOptions
               .userActivities) {
        if (ActivityIsURLLoad(activity) || ActivityIsTabMove(activity)) {
          return NO;
        }
      }
    }
  }

  if (self.startupParameters) {
    return NO;
  }

  if (self.tabSwitcherIsActive) {
    Browser* mainBrowser = self.mainInterface.browser;
    Browser* otrBrowser = self.incognitoInterface.browser;
    // Only attempt to dismiss the tab switcher and open a new tab if:
    // - there are no tabs open in either tab model, and
    // - the tab switcher controller is not directly or indirectly presenting
    // another view controller.
    if (!(mainBrowser->GetWebStateList()->empty()) ||
        !(otrBrowser->GetWebStateList()->empty()))
      return NO;

    // If the tabSwitcher is contained, check if the parent container is
    // presenting another view controller.
    if ([self.mainCoordinator.baseViewController
                .parentViewController presentedViewController]) {
      return NO;
    }

    // Check if the tabSwitcher is directly presenting another view controller.
    if (self.mainCoordinator.baseViewController.presentedViewController) {
      return NO;
    }

    return YES;
  }

  return browser->GetWebStateList()->empty();
}

#pragma mark - SceneURLLoadingServiceDelegate

// Note that the current tab of |browserCoordinator|'s BVC will normally be
// reloaded by this method. If a new tab is about to be added, call
// expectNewForegroundTab on the BVC first to avoid extra work and possible page
// load side-effects for the tab being replaced.
- (void)setCurrentInterfaceForMode:(ApplicationMode)mode {
  DCHECK(self.interfaceProvider);
  BOOL incognitio = mode == ApplicationMode::INCOGNITO;
  id<BrowserInterface> currentInterface =
      self.interfaceProvider.currentInterface;
  id<BrowserInterface> newInterface =
      incognitio ? self.interfaceProvider.incognitoInterface
                 : self.interfaceProvider.mainInterface;
  if (currentInterface && currentInterface == newInterface)
    return;

  // Update the snapshot before switching another application mode.  This
  // ensures that the snapshot is correct when links are opened in a different
  // application mode.
  [self updateActiveWebStateSnapshot];

  self.interfaceProvider.currentInterface = newInterface;

  if (!self.dismissingTabSwitcher)
    [self displayCurrentBVCAndFocusOmnibox:NO];

  // Tell the BVC that was made current that it can use the web.
  [self activateBVCAndMakeCurrentBVCPrimary];
}

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  // Immediately hide modals from the provider (alert views, action sheets,
  // popovers). They will be ultimately dismissed by their owners, but at least,
  // they are not visible.
  ios::GetChromeBrowserProvider()->HideModalViewStack();

  // ChromeIdentityService is responsible for the dialogs displayed by the
  // services it wraps.
  ios::GetChromeBrowserProvider()->GetChromeIdentityService()->DismissDialogs();

  // MailtoHandlerProvider is responsible for the dialogs displayed by the
  // services it wraps.
  ios::GetChromeBrowserProvider()
      ->GetMailtoHandlerProvider()
      ->DismissAllMailtoHandlerInterfaces();

  // Then, depending on what the SSO view controller is presented on, dismiss
  // it.
  ProceduralBlock completionWithBVC = ^{
    DCHECK(self.currentInterface.viewController);
    DCHECK(!self.tabSwitcherIsActive);
    DCHECK(!self.signinCoordinator);
    // This will dismiss the SSO view controller.
    [self.interfaceProvider.currentInterface
        clearPresentedStateWithCompletion:completion
                           dismissOmnibox:dismissOmnibox];
  };
  ProceduralBlock completionWithoutBVC = ^{
    // |self.currentInterface.bvc| may exist but tab switcher should be
    // active.
    DCHECK(self.tabSwitcherIsActive);
    DCHECK(!self.signinCoordinator);
    // History coordinator can be started on top of the tab grid.
    // This is not true of the other tab switchers.
    DCHECK(self.mainCoordinator);
    [self.mainCoordinator stopChildCoordinatorsWithCompletion:completion];
  };

  // Select a completion based on whether the BVC is shown.
  ProceduralBlock chosenCompletion =
      self.tabSwitcherIsActive ? completionWithoutBVC : completionWithBVC;

  if (self.isSettingsViewPresented) {
    // In this case, the settings are up and the BVC is showing. Close the
    // settings then call the chosen completion.
    [self closeSettingsAnimated:NO completion:chosenCompletion];
  } else if (self.signinCoordinator) {
    // The sign-in screen is showing, interrupt it and call the chosen
    // completion.
    [self interruptSigninCoordinatorAnimated:NO completion:chosenCompletion];
  } else {
    // Does not require a special case. Run the chosen completion.
    chosenCompletion();
  }

  // Verify that no modal views are left presented.
  ios::GetChromeBrowserProvider()->LogIfModalViewsArePresented();
}

- (void)openMultipleTabsInMode:
            (ApplicationModeForTabOpening)tabOpeningTargetMode
                          URLs:(const std::vector<GURL>&)URLs
                    completion:(ProceduralBlock)completion {
  [self recursiveOpenURLs:URLs
                   inMode:tabOpeningTargetMode
             currentIndex:0
               totalCount:URLs.size()
               completion:completion];
}

// Call |dismissModalsAndOpenSelectedTabInMode| recursively to open the list of
// URLs contained in |URLs|. Achieved through chaining
// |dismissModalsAndOpenSelectedTabInMode| in its completion handler.
- (void)recursiveOpenURLs:(const std::vector<GURL>&)URLs
                   inMode:(ApplicationModeForTabOpening)mode
             currentIndex:(size_t)currentIndex
               totalCount:(size_t)totalCount
               completion:(ProceduralBlock)completion {
  if (currentIndex >= totalCount) {
    if (completion) {
      completion();
    }
    return;
  }

  GURL webpageGURL = URLs.at(currentIndex);

  __weak SceneController* weakSelf = self;

  if (!webpageGURL.is_valid()) {
    [self recursiveOpenURLs:URLs
                     inMode:mode
               currentIndex:(currentIndex + 1)
                 totalCount:totalCount
                 completion:completion];
    return;
  }

  UrlLoadParams param = UrlLoadParams::InNewTab(webpageGURL, webpageGURL);
  std::vector<GURL> copyURLs = URLs;

  [self dismissModalsAndOpenSelectedTabInMode:mode
                            withUrlLoadParams:param
                               dismissOmnibox:YES
                                   completion:^{
                                     [weakSelf
                                         recursiveOpenURLs:copyURLs
                                                    inMode:mode
                                              currentIndex:(currentIndex + 1)
                                                totalCount:totalCount
                                                completion:completion];
                                   }];
}

// Opens a tab in the target BVC, and switches to it in a way that's appropriate
// to the current UI, based on the |dismissModals| flag:
// - If a modal dialog is showing and |dismissModals| is NO, the selected tab of
// the main tab model will change in the background, but the view won't change.
// - Otherwise, any modal view will be dismissed, the tab switcher will animate
// out if it is showing, the target BVC will become active, and the new tab will
// be shown.
// If the current tab in |targetMode| is a NTP, it can be reused to open URL.
// |completion| is executed after the tab is opened. After Tab is open the
// virtual URL is set to the pending navigation item.
- (void)openSelectedTabInMode:(ApplicationModeForTabOpening)tabOpeningTargetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion {
  // Update the snapshot before opening a new tab. This ensures that the
  // snapshot is correct when tabs are openned via the dispatcher.
  [self updateActiveWebStateSnapshot];

  ApplicationMode targetMode;

  if (tabOpeningTargetMode == ApplicationModeForTabOpening::CURRENT) {
    targetMode = self.interfaceProvider.currentInterface.incognito
                     ? ApplicationMode::INCOGNITO
                     : ApplicationMode::NORMAL;
  } else if (tabOpeningTargetMode == ApplicationModeForTabOpening::NORMAL) {
    targetMode = ApplicationMode::NORMAL;
  } else {
    targetMode = ApplicationMode::INCOGNITO;
  }

  id<BrowserInterface> targetInterface =
      targetMode == ApplicationMode::NORMAL
          ? self.interfaceProvider.mainInterface
          : self.interfaceProvider.incognitoInterface;
  ProceduralBlock startupCompletion =
      [self completionBlockForTriggeringAction:[self.startupParameters
                                                       postOpeningAction]];

  // Commands are only allowed on NTP.
  DCHECK(IsURLNtp(urlLoadParams.web_params.url) || !startupCompletion);
  ProceduralBlock tabOpenedCompletion = nil;
  if (startupCompletion && completion) {
    tabOpenedCompletion = ^{
      // Order is important here. |completion| may do cleaning tasks that will
      // invalidate |startupCompletion|.
      startupCompletion();
      completion();
    };
  } else if (startupCompletion) {
    tabOpenedCompletion = startupCompletion;
  } else {
    tabOpenedCompletion = completion;
  }

  if (self.tabSwitcherIsActive) {
    // If the tab switcher is already being dismissed, simply add the tab and
    // note that when the tab switcher finishes dismissing, the current BVC
    // should be switched to be the main BVC if necessary.
    if (self.dismissingTabSwitcher) {
      self.modeToDisplayOnTabSwitcherDismissal =
          targetMode == ApplicationMode::NORMAL
              ? TabSwitcherDismissalMode::NORMAL
              : TabSwitcherDismissalMode::INCOGNITO;
      [targetInterface.bvc appendTabAddedCompletion:tabOpenedCompletion];
      UrlLoadParams savedParams = urlLoadParams;
      savedParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
      UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
          ->Load(savedParams);
    } else {
      // Voice search, QRScanner and the omnibox are presented by the BVC.
      // They must be started after the BVC view is added in the hierarchy.
      self.NTPActionAfterTabSwitcherDismissal =
          [self.startupParameters postOpeningAction];
      [self setStartupParameters:nil];

      [self addANewTabAndPresentBrowser:targetInterface.browser
                      withURLLoadParams:urlLoadParams];
      // In this particular usage, there should be no postOpeningAction,
      // as triggering voice search while there are multiple windows opened is probably
      // a bad idea both technically and as a user experience.
      // It should be the caller duty to not set a completion if they don't need it.
      if (completion) {
        completion();
      }
    }
  } else {
    if (!self.currentInterface.viewController.presentedViewController) {
      [targetInterface.bvc expectNewForegroundTab];
    }
    [self setCurrentInterfaceForMode:targetMode];
    [self openOrReuseTabInMode:targetMode
             withUrlLoadParams:urlLoadParams
           tabOpenedCompletion:tabOpenedCompletion];
  }

  if (self.sceneState.appState.startupInformation.restoreHelper) {
    // Now that all the operations on the tabs have been done, display the
    // restore infobar if needed.
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.sceneState.appState.startupInformation
              .restoreHelper showRestorePrompt];
      self.sceneState.appState.startupInformation.restoreHelper = nil;
    });
  }
}

- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command {
  [self openURLInNewTab:command];
}

- (void)expectNewForegroundTabForMode:(ApplicationMode)targetMode {
  id<BrowserInterface> interface =
      targetMode == ApplicationMode::INCOGNITO
          ? self.interfaceProvider.incognitoInterface
          : self.interfaceProvider.mainInterface;
  DCHECK(interface);
  [interface.bvc expectNewForegroundTab];
}

- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox {
  [self.currentInterface.bvc openNewTabFromOriginPoint:originPoint
                                          focusOmnibox:focusOmnibox];
}

- (Browser*)currentBrowserForURLLoading {
  return self.currentInterface.browser;
}

// Asks the respective Snapshot helper to update the snapshot for the active
// WebState.
- (void)updateActiveWebStateSnapshot {
  // Durinhg startup, there may be no current interface. Do nothing in that
  // case.
  if (!self.currentInterface)
    return;

  WebStateList* webStateList = self.currentInterface.browser->GetWebStateList();
  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    SnapshotTabHelper::FromWebState(webState)->UpdateSnapshotWithCallback(nil);
  }
}

// Checks the target BVC's current tab's URL. If this URL is chrome://newtab,
// loads |urlLoadParams| in this tab. Otherwise, open |urlLoadParams| in a new
// tab in the target BVC. |tabDisplayedCompletion| will be called on the new tab
// (if not nil).
- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion {
  id<BrowserInterface> targetInterface = targetMode == ApplicationMode::NORMAL
                                             ? self.mainInterface
                                             : self.incognitoInterface;

  BrowserViewController* targetBVC = targetInterface.bvc;
  web::WebState* currentWebState =
      targetInterface.browser->GetWebStateList()->GetActiveWebState();

  // Don't call loadWithParams for chrome://newtab when it's already loaded.
  // Note that it's safe to use -GetVisibleURL here, as it doesn't matter if the
  // NTP hasn't finished loading.
  if (currentWebState && IsURLNtp(currentWebState->GetVisibleURL()) &&
      IsURLNtp(urlLoadParams.web_params.url)) {
    if (tabOpenedCompletion) {
      tabOpenedCompletion();
    }
    return;
  }

  // With kBrowserContainerContainsNTP enabled paired with a restored NTP
  // session, the NTP may appear committed when it is still loading.  For the
  // time being, always load within a new tab when this feature is enabled.
  // TODO(crbug.com/931284): Revert this change when fixed.
  BOOL alwaysInsertNewTab =
      base::FeatureList::IsEnabled(kBlockNewTabPagePendingLoad);
  // If the current tab isn't an NTP, open a new tab.  Be sure to use
  // -GetLastCommittedURL incase the NTP is still loading.
  if (alwaysInsertNewTab ||
      !(currentWebState && IsURLNtp(currentWebState->GetVisibleURL()))) {
    [targetBVC appendTabAddedCompletion:tabOpenedCompletion];
    UrlLoadParams newTabParams = urlLoadParams;
    newTabParams.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    newTabParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
    UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
        ->Load(newTabParams);
    return;
  }
  // Otherwise, load |urlLoadParams| in the current tab.
  UrlLoadParams sameTabParams = urlLoadParams;
  sameTabParams.disposition = WindowOpenDisposition::CURRENT_TAB;
  UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
      ->Load(sameTabParams);
  if (tabOpenedCompletion) {
    tabOpenedCompletion();
  }
}

// Displays current (incognito/normal) BVC and optionally focuses the omnibox.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox {
  ProceduralBlock completion = nil;
  if (focusOmnibox) {
    id<OmniboxCommands> omniboxHandler = HandlerForProtocol(
        self.currentInterface.browser->GetCommandDispatcher(), OmniboxCommands);
    completion = ^{
      [omniboxHandler focusOmnibox];
    };
  }
  [self.mainCoordinator
      showTabViewController:self.currentInterface.viewController
                 completion:completion];
  [HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                      ApplicationCommands)
      setIncognitoContentVisible:self.currentInterface.incognito];
}

#pragma mark - Sign In UI presentation

- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ChromeBrowserState*)browserState {
  UMA_HISTOGRAM_BOOLEAN("Signin.SignedInAccountsViewImpression", true);
  id<ApplicationSettingsCommands> settingsHandler =
      HandlerForProtocol(self.mainInterface.browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  UIViewController* accountsViewController =
      [[SignedInAccountsViewController alloc]
          initWithBrowserState:browserState
                    dispatcher:settingsHandler];
  [[self topPresentedViewController]
      presentViewController:accountsViewController
                   animated:YES
                 completion:nil];
}

- (void)closeSettingsAnimated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  if (self.settingsNavigationController) {
    ProceduralBlock dismissSettings = ^() {
      [self.settingsNavigationController cleanUpSettings];
      UIViewController* presentingViewController =
          [self.settingsNavigationController presentingViewController];
      // If presentingViewController is nil it means the VC was already
      // dismissed by some other action like swiping down.
      DCHECK(presentingViewController);
      [presentingViewController dismissViewControllerAnimated:animated
                                                   completion:completion];
      self.settingsNavigationController = nil;
    };
    // |self.signinCoordinator| can be presented on top of the settings, to
    // present the Trusted Vault reauthentication |self.signinCoordinator| has
    // to be closed first.
    if (self.signinCoordinator) {
      [self interruptSigninCoordinatorAnimated:animated
                                    completion:dismissSettings];
    } else if (dismissSettings) {
      dismissSettings();
    }
    return;
  }
  // |self.signinCoordinator| can also present settings, like
  // the advanced sign-in settings navigation controller. If the settings has
  // to be closed, it is thus the responsibility of the main controller to
  // dismiss the advanced sign-in settings by dismssing the settings
  // presented by |self.signinCoordinator|.
  // To reproduce this case:
  //  - open Bookmark view
  //  - start sign-in
  //  - tap on "Settings" to open the advanced sign-in settings
  //  - tap on "Manage Your Google Account"
  DCHECK(self.signinCoordinator);
  [self interruptSigninCoordinatorAnimated:animated completion:completion];
}

- (UIViewController*)topPresentedViewController {
  // TODO(crbug.com/754642): Implement TopPresentedViewControllerFrom()
  // privately.
  return top_view_controller::TopPresentedViewControllerFrom(
      self.mainCoordinator.baseViewController);
}

// Interrupts the sign-in coordinator actions and dismisses its views either
// with or without animation.
- (void)interruptSigninCoordinatorAnimated:(BOOL)animated
                                completion:(ProceduralBlock)completion {
  SigninCoordinatorInterruptAction action =
      animated ? SigninCoordinatorInterruptActionDismissWithAnimation
               : SigninCoordinatorInterruptActionDismissWithoutAnimation;
  [self.signinCoordinator interruptWithAction:action completion:completion];
}

// Starts the sign-in coordinator with a default cleanup completion.
- (void)startSigninCoordinatorWithCompletion:
    (signin_ui::CompletionCallback)completion {
  __block std::unique_ptr<ScopedUIBlocker> uiBlocker =
      std::make_unique<ScopedUIBlocker>(self.sceneState);
  DCHECK(self.signinCoordinator);
  __weak SceneController* weakSelf = self;
  self.signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo*) {
        [weakSelf.signinCoordinator stop];
        weakSelf.signinCoordinator = nil;
        uiBlocker.reset();

        if (completion) {
          completion(result == SigninCoordinatorResultSuccess);
        }
      };

  [self.signinCoordinator start];
}

#pragma mark - WebStateListObserving

// Called when a WebState is removed. Triggers the switcher view when the last
// WebState is closed on a device that uses the switcher.
- (void)webStateList:(WebStateList*)notifiedWebStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  // Do nothing on initialization.
  if (!self.currentInterface.browser)
    return;

  if (notifiedWebStateList->empty()) {
    if (webState->GetBrowserState()->IsOffTheRecord()) {
      [self lastIncognitoTabClosed];
    } else {
      [self lastRegularTabClosed];
    }
  }
}

#pragma mark - Helpers for web state list events

// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed {
  // If no other window has incognito tab, then destroy and rebuild the
  // BrowserState. Otherwise, just do the state transition animation.
  if ([self shouldDestroyAndRebuildIncognitoBrowserState]) {
    // Incognito browser state cannot be deleted before all the requests are
    // deleted. Queue empty task on IO thread and destroy the BrowserState
    // when the task has executed.
    base::PostTaskAndReply(FROM_HERE, {web::WebThread::IO}, base::DoNothing(),
                           base::BindRepeating(^{
                             [self destroyAndRebuildIncognitoBrowserState];
                           }));
  }

  // a) The first condition can happen when the last incognito tab is closed
  // from the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the otr tab model when it's not current.
  // Nothing to do here. The next user action (like clicking on an existing
  // regular tab or creating a new incognito tab from the settings menu) will
  // take care of the logic to mode switch.
  if (self.tabSwitcherIsActive || !self.currentInterface.incognito) {
    return;
  }

  WebStateList* currentWebStateList =
      self.currentInterface.browser->GetWebStateList();
  if (currentWebStateList->empty()) {
    [self showTabSwitcher];
  } else {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }
}

// Called when the last regular tab was closed.
- (void)lastRegularTabClosed {
  // a) The first condition can happen when the last regular tab is closed from
  // the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the main tab model when the main tab model is not
  // current.
  // Nothing to do here.
  if (self.tabSwitcherIsActive || self.currentInterface.incognito) {
    return;
  }

  [self showTabSwitcher];
}

// Clears incognito data that is specific to iOS and won't be cleared by
// deleting the browser state.
- (void)clearIOSSpecificIncognitoData {
  DCHECK(self.sceneState.appState.mainBrowserState
             ->HasOffTheRecordChromeBrowserState());
  ChromeBrowserState* otrBrowserState =
      self.sceneState.appState.mainBrowserState
          ->GetOffTheRecordChromeBrowserState();
  [self.browsingDataCommandsHandler
      removeBrowsingDataForBrowserState:otrBrowserState
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:^{
                          [self activateBVCAndMakeCurrentBVCPrimary];
                        }];
}

- (void)activateBVCAndMakeCurrentBVCPrimary {
  // If there are pending removal operations, the activation will be deferred
  // until the callback is received.
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserStateIfExists(
          self.currentInterface.browser->GetBrowserState());
  if (browsingDataRemover && browsingDataRemover->IsRemoving())
    return;

  self.interfaceProvider.mainInterface.userInteractionEnabled = YES;
  self.interfaceProvider.incognitoInterface.userInteractionEnabled = YES;
  [self.currentInterface setPrimary:YES];
}

// Shows the tab switcher UI.
- (void)showTabSwitcher {
  DCHECK(self.mainCoordinator);
  [self.mainCoordinator setActivePage:[self activePage]];
  self.tabSwitcherIsActive = YES;
  self.mainCoordinator.delegate = self;

  [self.mainCoordinator showTabGrid];
}

- (void)openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts
    API_AVAILABLE(ios(13)) {
  if (self.sceneState.appState.isInSafeMode) {
    return;
  }

  NSMutableSet<URLOpenerParams*>* URLsToOpen = [[NSMutableSet alloc] init];
  for (UIOpenURLContext* context : URLContexts) {
    URLOpenerParams* options =
        [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
    NSSet* URLContextSet = [NSSet setWithObject:context];
    if (!ios::GetChromeBrowserProvider()
             ->GetChromeIdentityService()
             ->HandleSessionOpenURLContexts(self.sceneState.scene,
                                            URLContextSet)) {
      [URLsToOpen addObject:options];
    }
  }
  // When opening with URLs for GetChromeIdentityService, it is expected that a
  // single URL is passed.
  DCHECK(URLsToOpen.count == URLContexts.count || URLContexts.count == 1);
  BOOL active =
      _sceneState.activationLevel >= SceneActivationLevelForegroundActive;
  if (self.sceneState.appState.startupInformation.isPresentingFirstRunUI ||
      self.sceneState.presentingModalOverlay) {
    active = NO;
  }

  for (URLOpenerParams* options : URLsToOpen) {
    [URLOpener openURL:options
            applicationActive:active
                    tabOpener:self
        connectionInformation:self
           startupInformation:self.sceneState.appState.startupInformation];
  }
}

- (id<BrowserInterface>)extractInterfaceBaseOnMode:
    (ApplicationModeForTabOpening)targetMode {
  ApplicationMode applicationMode;

  if (targetMode == ApplicationModeForTabOpening::CURRENT) {
    applicationMode = self.interfaceProvider.currentInterface.incognito
                          ? ApplicationMode::INCOGNITO
                          : ApplicationMode::NORMAL;
  } else if (targetMode == ApplicationModeForTabOpening::NORMAL) {
    applicationMode = ApplicationMode::NORMAL;
  } else {
    applicationMode = ApplicationMode::INCOGNITO;
  }

  id<BrowserInterface> targetInterface =
      applicationMode == ApplicationMode::NORMAL
          ? self.interfaceProvider.mainInterface
          : self.interfaceProvider.incognitoInterface;

  return targetInterface;
}

#pragma mark - TabGrid helpers

// Returns the page that should be active in the TabGrid.
- (TabGridPage)activePage {
  if (self.currentInterface.browser == self.incognitoInterface.browser)
    return TabGridPageIncognitoTabs;
  return TabGridPageRegularTabs;
}

// Adds a new tab to the |browser| based on |urlLoadParams| and then presents
// it.
- (void)addANewTabAndPresentBrowser:(Browser*)browser
                  withURLLoadParams:(const UrlLoadParams&)urlLoadParams {
  TabInsertionBrowserAgent::FromBrowser(browser)->InsertWebState(
      urlLoadParams.web_params, nil, false, browser->GetWebStateList()->count(),
      false);
  [self beginDismissingTabSwitcherWithCurrentBrowser:browser focusOmnibox:NO];
}

#pragma mark - Handling of destroying the incognito BrowserState

// The incognito BrowserState should be closed when the last incognito tab is
// closed (i.e. if there are other incognito tabs open in another Scene, the
// BrowserState must not be destroyed).
- (BOOL)shouldDestroyAndRebuildIncognitoBrowserState {
  ChromeBrowserState* mainBrowserState =
      self.sceneState.appState.mainBrowserState;
  if (!mainBrowserState->HasOffTheRecordChromeBrowserState())
    return NO;

  ChromeBrowserState* otrBrowserState =
      mainBrowserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(otrBrowserState);
  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    if (!webStateList->empty())
      return NO;
  }

  return YES;
}

// Destroys and rebuilds the incognito BrowserState. This will inform all the
// other SceneController to destroy state tied to the BrowserState and to
// recreate it.
- (void)destroyAndRebuildIncognitoBrowserState {
  // This seems the best place to mark the start of destroying the incognito
  // browser state.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/true);

  [self clearIOSSpecificIncognitoData];

  ChromeBrowserState* mainBrowserState =
      self.sceneState.appState.mainBrowserState;
  DCHECK(mainBrowserState->HasOffTheRecordChromeBrowserState());

  NSMutableArray<SceneController*>* sceneControllers =
      [[NSMutableArray alloc] init];
  for (SceneState* sceneState in [self.sceneState.appState connectedScenes]) {
    SceneController* sceneController = sceneState.controller;
    // In some circumstances, the scene state may still exist while the
    // corresponding scene controller has been deallocated.
    // (see crbug.com/1142782).
    if (sceneController) {
      [sceneControllers addObject:sceneController];
    }
  }

  for (SceneController* sceneController in sceneControllers) {
    [sceneController willDestroyIncognitoBrowserState];
  }

  BreadcrumbPersistentStorageManager* persistentStorageManager = nullptr;
  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerKeyedService* service =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            mainBrowserState->GetOffTheRecordChromeBrowserState());
    persistentStorageManager = service->GetPersistentStorageManager();
    breakpad::StopMonitoringBreadcrumbManagerService(service);
    service->StopPersisting();
  }

  // Record off-the-record metrics before detroying the BrowserState.
  if (mainBrowserState->HasOffTheRecordChromeBrowserState()) {
    ChromeBrowserState* otrBrowserState =
        mainBrowserState->GetOffTheRecordChromeBrowserState();

    SessionMetrics::FromBrowserState(otrBrowserState)
        ->RecordAndClearSessionMetrics(MetricsToRecordFlags::kNoMetrics);
  }

  // Destroy and recreate the off-the-record BrowserState.
  mainBrowserState->DestroyOffTheRecordChromeBrowserState();
  mainBrowserState->GetOffTheRecordChromeBrowserState();

  for (SceneController* sceneController in sceneControllers) {
    [sceneController incognitoBrowserStateCreated];
  }

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerKeyedService* service =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            mainBrowserState->GetOffTheRecordChromeBrowserState());

    if (persistentStorageManager) {
      service->StartPersisting(persistentStorageManager);
    }
    breakpad::MonitorBreadcrumbManagerService(service);
  }

  // This seems the best place to deem the destroying and rebuilding the
  // incognito browser state to be completed.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/false);
}

- (void)willDestroyIncognitoBrowserState {
  // Clear the Incognito Browser and notify the TabGrid that its otrBrowser
  // will be destroyed.
  self.mainCoordinator.incognitoBrowser = nil;

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
        ->SetLoggingEnabled(false);
  }

  self.incognitoInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());
  [self.browserViewWrangler willDestroyIncognitoBrowserState];
}

- (void)incognitoBrowserStateCreated {
  [self.browserViewWrangler incognitoBrowserStateCreated];

  // There should be a new URL loading browser agent for the incognito browser,
  // so set the scene URL loading service on it.
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  self.incognitoInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());

  if (self.currentInterface.incognito) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  }

  // Always set the new otr Browser for the tablet or grid switcher.
  // Notify the TabGrid with the new Incognito Browser.
  self.mainCoordinator.incognitoBrowser = self.incognitoInterface.browser;
}
@end
