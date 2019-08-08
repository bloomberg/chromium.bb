// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#include <memory>
#include <string>

#import <CoreSpotlight/CoreSpotlight.h>
#import <objc/objc.h>
#import <objc/runtime.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/ios/block_types.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crl_set_remover.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "components/payments/core/features.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/ios/features.h"
#include "components/unified_consent/feature.h"
#include "components/url_formatter/url_formatter.h"
#include "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/firebase_utils.h"
#import "ios/chrome/app/main_controller_private.h"
#import "ios/chrome/app/memory_monitor.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/app/startup/chrome_main_starter.h"
#include "ios/chrome/app/startup/client_registration.h"
#import "ios/chrome/app/startup/content_suggestions_scheduler_notifications.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/app/startup/provider_registration.h"
#include "ios/chrome/app/startup/register_experimental_settings.h"
#include "ios/chrome/app/startup/setup_debugging.h"
#import "ios/chrome/app/startup_tasks.h"
#include "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/app_startup_parameters.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_removal_controller.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/crash_loop_detection_util.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/external_files/external_file_remover_factory.h"
#import "ios/chrome/browser/external_files/external_file_remover_impl.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/feature_engagement/tracker_util.h"
#include "ios/chrome/browser/file_metadata_util.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/mailto/features.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/net/cookie_util.h"
#include "ios/chrome/browser/ntp/features.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/payments/ios_payment_instrument_launcher.h"
#include "ios/chrome/browser/payments/ios_payment_instrument_launcher_factory.h"
#import "ios/chrome/browser/payments/payment_request_constants.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/reading_list/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#import "ios/chrome/browser/search_engines/extension_search_engine_data_updater.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/share_extension/share_extension_service.h"
#import "ios/chrome/browser/share_extension/share_extension_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_delegate.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#include "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/promos/signin_promo_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"
#include "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_switcher.h"
#import "ios/chrome/browser/ui/tab_grid/view_controller_swapping.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_field_trial_version.h"
#include "ios/chrome/common/app_group/app_group_utils.h"
#include "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/empty_nsurlcache.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#import "ios/public/provider/chrome/browser/overrides_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MDCTypographyAdditions/MDFRobotoFontLoader+MDCTypographyAdditions.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_task_traits.h"
#import "ios/web/public/web_view_creation_util.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "mojo/core/embedder/embedder.h"
#import "net/base/mac/url_conversions.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Preference key used to store which profile is current.
NSString* kIncognitoCurrentKey = @"IncognitoActive";

// Constants for deferred initialization of preferences observer.
NSString* const kPrefObserverInit = @"PrefObserverInit";

// Constants for deferring notifying the AuthenticationService of a new cold
// start.
NSString* const kAuthenticationServiceNotification =
    @"AuthenticationServiceNotification";

// Constants for deferring reseting the startup attempt count (to give the app
// a little while to make sure it says alive).
NSString* const kStartupAttemptReset = @"StartupAttempReset";

// Constants for deferring memory debugging tools startup.
NSString* const kMemoryDebuggingToolsStartup = @"MemoryDebuggingToolsStartup";

// Constants for deferring mailto handling initialization.
NSString* const kMailtoHandlingInitialization = @"MailtoHandlingInitialization";

// Constants for deferring saving field trial values
NSString* const kSaveFieldTrialValues = @"SaveFieldTrialValues";

// Constants for deferred check if it is necessary to send pings to
// Chrome distribution related services.
NSString* const kSendInstallPingIfNecessary = @"SendInstallPingIfNecessary";

// Constants for deferred deletion of leftover user downloaded files.
NSString* const kDeleteDownloads = @"DeleteDownloads";

// Constants for deferred deletion of leftover temporary passwords files.
NSString* const kDeleteTempPasswords = @"DeleteTempPasswords";

// Constants for deferred sending of queued feedback.
NSString* const kSendQueuedFeedback = @"SendQueuedFeedback";

// Constants for deferring the deletion of pre-upgrade crash reports.
NSString* const kCleanupCrashReports = @"CleanupCrashReports";

// Constants for deferring the deletion of old snapshots.
NSString* const kPurgeSnapshots = @"PurgeSnapshots";

// Constants for deferring startup Spotlight bookmark indexing.
NSString* const kStartSpotlightBookmarksIndexing =
    @"StartSpotlightBookmarksIndexing";

// Constants for deferred promo display.
const NSTimeInterval kDisplayPromoDelay = 0.1;

// A rough estimate of the expected duration of a view controller transition
// animation. It's used to temporarily disable mutally exclusive chrome
// commands that trigger a view controller presentation.
const int64_t kExpectedTransitionDurationInNanoSeconds = 0.2 * NSEC_PER_SEC;

// Adapted from chrome/browser/ui/browser_init.cc.
void RegisterComponentsForUpdate() {
  component_updater::ComponentUpdateService* cus =
      GetApplicationContext()->GetComponentUpdateService();
  DCHECK(cus);
  base::FilePath path;
  const bool success = base::PathService::Get(ios::DIR_USER_DATA, &path);
  DCHECK(success);
  // Clean up any legacy CRLSet on the local disk - CRLSet used to be shipped
  // as a component on iOS but is not anymore.
  component_updater::DeleteLegacyCRLSet(path);
}

// Used to update the current BVC mode if a new tab is added while the tab
// switcher view is being dimissed.  This is different than ApplicationMode in
// that it can be set to |NONE| when not in use.
enum class TabSwitcherDismissalMode { NONE, NORMAL, INCOGNITO };

// The delay, in seconds, for cleaning external files.
const int kExternalFilesCleanupDelaySeconds = 60;

// Delegate for the AuthenticationService.
class MainControllerAuthenticationServiceDelegate
    : public AuthenticationServiceDelegate {
 public:
  MainControllerAuthenticationServiceDelegate(
      ios::ChromeBrowserState* browser_state,
      id<BrowsingDataCommands> dispatcher);
  ~MainControllerAuthenticationServiceDelegate() override;

  // AuthenticationServiceDelegate implementation.
  void ClearBrowsingData(ProceduralBlock completion) override;

 private:
  ios::ChromeBrowserState* browser_state_ = nullptr;
  __weak id<BrowsingDataCommands> dispatcher_ = nil;

  DISALLOW_COPY_AND_ASSIGN(MainControllerAuthenticationServiceDelegate);
};

MainControllerAuthenticationServiceDelegate::
    MainControllerAuthenticationServiceDelegate(
        ios::ChromeBrowserState* browser_state,
        id<BrowsingDataCommands> dispatcher)
    : browser_state_(browser_state), dispatcher_(dispatcher) {}

MainControllerAuthenticationServiceDelegate::
    ~MainControllerAuthenticationServiceDelegate() = default;

void MainControllerAuthenticationServiceDelegate::ClearBrowsingData(
    ProceduralBlock completion) {
  [dispatcher_
      removeBrowsingDataForBrowserState:browser_state_
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:completion];
}

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

}  // namespace

@interface MainController () <AppURLLoadingServiceDelegate,
                              BrowserStateStorageSwitching,
                              PrefObserverDelegate,
                              SettingsNavigationControllerDelegate,
                              TabSwitcherDelegate,
                              WebStateListObserving,
                              UserFeedbackDataSource> {
  IBOutlet UIWindow* _window;

  // Weak; owned by the ChromeBrowserProvider.
  ios::ChromeBrowserStateManager* _browserStateManager;

  // The object that drives the Chrome startup/shutdown logic.
  std::unique_ptr<IOSChromeMain> _chromeMain;

  // The ChromeBrowserState associated with the main (non-OTR) browsing mode.
  ios::ChromeBrowserState* _mainBrowserState;  // Weak.

  // Wrangler to handle BVC and tab model creation, access, and related logic.
  // Implements faetures exposed from this object through the
  // BrowserViewInformation protocol.
  BrowserViewWrangler* _browserViewWrangler;

  // Parameters received at startup time when the app is launched from another
  // app.
  AppStartupParameters* _startupParameters;

  // Navigation View controller for the settings.
  SettingsNavigationController* _settingsNavigationController;

  // TabSwitcher object -- the tab grid.
  id<TabSwitcher> _tabSwitcher;

  // YES while animating the dismissal of tab switcher.
  BOOL _dismissingTabSwitcher;

  // If not NONE, the current BVC should be switched to this BVC on completion
  // of tab switcher dismissal.
  TabSwitcherDismissalMode _modeToDisplayOnTabSwitcherDismissal;

  // If YES, the tab switcher is currently active.
  BOOL _tabSwitcherIsActive;

  // True if the current session began from a cold start. False if the app has
  // entered the background at least once since start up.
  BOOL _isColdStart;

  // Keeps track of the restore state during startup.
  CrashRestoreHelper* _restoreHelper;

  // An object to record metrics related to the user's first action.
  std::unique_ptr<FirstUserActionRecorder> _firstUserActionRecorder;

  // True if First Run UI (terms of service & sync sign-in) is being presented
  // in a modal dialog.
  BOOL _isPresentingFirstRunUI;

  // The tab switcher command and the voice search commands can be sent by views
  // that reside in a different UIWindow leading to the fact that the exclusive
  // touch property will be ineffective and a command for processing both
  // commands may be sent in the same run of the runloop leading to
  // inconsistencies. Those two boolean indicate if one of those commands have
  // been processed in the last 200ms in order to only allow processing one at
  // a time.
  // TODO(crbug.com/560296):  Provide a general solution for handling mutually
  // exclusive chrome commands sent at nearly the same time.
  BOOL _isProcessingTabSwitcherCommand;
  BOOL _isProcessingVoiceSearchCommand;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _localStatePrefObserverBridge;

  // Registrar for pref changes notifications to the local state.
  PrefChangeRegistrar _localStatePrefChangeRegistrar;

  // Updates data about the current default search engine to be accessed in
  // extensions.
  std::unique_ptr<ExtensionSearchEngineDataUpdater>
      _extensionSearchEngineDataUpdater;

  // The class in charge of showing/hiding the memory debugger when the
  // appropriate pref changes.
  MemoryDebuggerManager* _memoryDebuggerManager;

  // Responsible for indexing chrome links (such as bookmarks, most likely...)
  // in system Spotlight index.
  SpotlightManager* _spotlightManager;

  // Cached launchOptions from -didFinishLaunchingWithOptions.
  NSDictionary* _launchOptions;

  // Coordinator for displaying history.
  HistoryCoordinator* _historyCoordinator;

  // Variable backing metricsMediator property.
  __weak MetricsMediator* _metricsMediator;

  // Hander for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;

  // The application level component for url loading. Is passed down to
  // browser state level UrlLoadingService instances.
  AppUrlLoadingService* _appURLLoadingService;
}

// The main coordinator, lazily created the first time it is accessed. Manages
// the main view controller. This property should not be accessed before the
// browser has started up to the FOREGROUND stage.
@property(nonatomic, readonly) TabGridCoordinator* mainCoordinator;

// A property to track whether the QR Scanner should be started upon tab
// switcher dismissal. It can only be YES if the QR Scanner experiment is
// enabled.
@property(nonatomic, readwrite)
    NTPTabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// The SigninInteractionCoordinator to present Sign In UI. It is created the
// first time Sign In UI is needed to be presented and should not be destroyed
// while the UI is presented.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

// Returns YES if the settings are presented, either from
// _settingsNavigationController or from SigninInteractionCoordinator.
@property(nonatomic, assign, readonly, getter=isSettingsViewPresented)
    BOOL settingsViewPresented;

// Activates |mainBVC| and |otrBVC| and sets |currentBVC| as primary iff
// |currentBVC| can be made active.
- (void)activateBVCAndMakeCurrentBVCPrimary;
// Sets |currentBVC| as the root view controller for the window.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox;
// Shows the tab switcher UI.
- (void)showTabSwitcher;
// Starts a voice search on the current BVC.
- (void)startVoiceSearchInCurrentBVC;
// Dismisses |signinInteractionCoordinator|.
- (void)dismissSigninInteractionCoordinator;
// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed;
// Called when the last regular tab was closed.
- (void)lastRegularTabClosed;
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
- (void)openSelectedTabInMode:(ApplicationMode)targetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion;
// Checks the target BVC's current tab's URL. If this URL is chrome://newtab,
// loads |urlLoadParams| in this tab. Otherwise, open |urlLoadParams| in a new
// tab in the target BVC. |tabDisplayedCompletion| will be called on the new tab
// (if not nil).
- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion;
// Returns whether the restore infobar should be displayed.
- (bool)mustShowRestoreInfobar;
// Begins the process of dismissing the tab switcher with the given current
// model, switching which BVC is suspended if necessary, but not updating the
// UI.  The omnibox will be focused after the tab switcher dismissal is
// completed if |focusOmnibox| is YES.
- (void)beginDismissingTabSwitcherWithCurrentModel:(TabModel*)tabModel
                                      focusOmnibox:(BOOL)focusOmnibox;
// Completes the process of dismissing the tab switcher, removing it from the
// screen and showing the appropriate BVC.
- (void)finishDismissingTabSwitcher;
// Opens an url from a link in the settings UI.
- (void)openUrlFromSettings:(OpenNewTabCommand*)command;
// Switch all global states for the given mode (normal or incognito).
- (void)switchGlobalStateToMode:(ApplicationMode)mode;
// Updates the local storage, cookie store, and sets the global state.
- (void)changeStorageFromBrowserState:(ios::ChromeBrowserState*)oldState
                       toBrowserState:(ios::ChromeBrowserState*)newState;
// Returns the set of the sessions ids of the tabs in the given |tabModel|.
- (NSMutableSet*)liveSessionsForTabModel:(TabModel*)tabModel;
// Purge the unused snapshots.
- (void)purgeSnapshots;
// Sets a LocalState pref marking the TOS EULA as accepted.
- (void)markEulaAsAccepted;
// Sends any feedback that happens to still be on local storage.
- (void)sendQueuedFeedback;
// Called whenever an orientation change is received.
- (void)orientationDidChange:(NSNotification*)notification;
// Register to receive orientation change notification to update breakpad
// report.
- (void)registerForOrientationChangeNotifications;
// Asynchronously creates the pref observers.
- (void)schedulePrefObserverInitialization;
// Asynchronously schedules pings to distribution services.
- (void)scheduleAppDistributionPings;
// Asynchronously schedule the init of the memoryDebuggerManager.
- (void)scheduleMemoryDebuggingTools;
// Asynchronously kick off regular free memory checks.
- (void)startFreeMemoryMonitoring;
// Asynchronously schedules the notification of the AuthenticationService.
- (void)scheduleAuthenticationServiceNotification;
// Asynchronously schedules the reset of the failed startup attempt counter.
- (void)scheduleStartupAttemptReset;
// Asynchronously schedules the cleanup of crash reports.
- (void)scheduleCrashReportCleanup;
// Asynchronously schedules the deletion of old snapshots.
- (void)scheduleSnapshotPurge;
// Schedules various cleanup tasks that are performed after launch.
- (void)scheduleStartupCleanupTasks;
// Schedules various tasks to be performed after the application becomes active.
- (void)scheduleLowPriorityStartupTasks;
// Schedules tasks that require a fully-functional BVC to be performed.
- (void)scheduleTasksRequiringBVCWithBrowserState;
// Schedules the deletion of user downloaded files that might be leftover
// from the last time Chrome was run.
- (void)scheduleDeleteDownloadsDirectory;
// Schedule the deletion of the temporary passwords files that might
// be left over from incomplete export operations.
- (void)scheduleDeleteTempPasswordsDirectory;
// Returns whether or not the app can launch in incognito mode.
- (BOOL)canLaunchInIncognito;
// Determines which UI should be shown on startup, and shows it.
- (void)createInitialUI:(ApplicationMode)launchMode;
// Initializes the first run UI and presents it to the user.
- (void)showFirstRunUI;
// Schedules presentation of the first eligible promo found, if any.
- (void)scheduleShowPromo;
// Crashes the application if requested.
- (void)crashIfRequested;
// Clears incognito data that is specific to iOS and won't be cleared by
// deleting the browser state.
- (void)clearIOSSpecificIncognitoData;
// Destroys and rebuilds the incognito browser state.
- (void)destroyAndRebuildIncognitoBrowserState;
// Handles the notification that first run modal dialog UI is about to complete.
- (void)handleFirstRunUIWillFinish;
// Handles the notification that first run modal dialog UI completed.
- (void)handleFirstRunUIDidFinish;
// Performs synchronous browser state initialization steps.
- (void)initializeBrowserState:(ios::ChromeBrowserState*)browserState;
// Helper methods to initialize the application to a specific stage.
// Setting |_browserInitializationStage| to a specific stage requires the
// corresponding function to return YES.
// Initializes the application to INITIALIZATION_STAGE_BASIC, which is the
// minimum initialization needed in all cases.
- (void)startUpBrowserBasicInitialization;
// Initializes the application to INITIALIZATION_STAGE_BACKGROUND, which is
// needed by background handlers.
- (void)startUpBrowserBackgroundInitialization;
// Initializes the application to INITIALIZATION_STAGE_FOREGROUND, which is
// needed when application runs in foreground.
- (void)startUpBrowserForegroundInitialization;
@end

@implementation MainController

// Defined by public protocols.
// - AppNavigation
@synthesize settingsNavigationController = _settingsNavigationController;
// - BrowserLauncher
@synthesize launchOptions = _launchOptions;
@synthesize browserInitializationStage = _browserInitializationStage;
// - StartupInformation
@synthesize isPresentingFirstRunUI = _isPresentingFirstRunUI;
@synthesize isColdStart = _isColdStart;
@synthesize startupParameters = _startupParameters;
@synthesize appLaunchTime = _appLaunchTime;
// Defined in private interface
@synthesize mainCoordinator = _mainCoordinator;
@synthesize NTPActionAfterTabSwitcherDismissal =
    _NTPActionAfterTabSwitcherDismissal;
@synthesize signinInteractionCoordinator = _signinInteractionCoordinator;

#pragma mark - Application lifecycle

- (instancetype)init {
  if ((self = [super init])) {
    _startupTasks = [[StartupTasks alloc] init];
  }
  return self;
}

- (void)dealloc {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

// This function starts up to only what is needed at each stage of the
// initialization. It is possible to continue initialization later.
- (void)startUpBrowserToStage:(BrowserInitializationStageType)stage {
  if (_browserInitializationStage < INITIALIZATION_STAGE_BASIC &&
      stage >= INITIALIZATION_STAGE_BASIC) {
    [self startUpBrowserBasicInitialization];
    _browserInitializationStage = INITIALIZATION_STAGE_BASIC;
  }

  if (_browserInitializationStage < INITIALIZATION_STAGE_BACKGROUND &&
      stage >= INITIALIZATION_STAGE_BACKGROUND) {
    [self startUpBrowserBackgroundInitialization];
    _browserInitializationStage = INITIALIZATION_STAGE_BACKGROUND;
  }

  if (_browserInitializationStage < INITIALIZATION_STAGE_FOREGROUND &&
      stage >= INITIALIZATION_STAGE_FOREGROUND) {
    // When adding a new initialization flow, consider setting
    // |_appState.userInteracted| at the appropriate time.
    DCHECK(_appState.userInteracted);
    [self startUpBrowserForegroundInitialization];
    _browserInitializationStage = INITIALIZATION_STAGE_FOREGROUND;
  }
}

- (void)startUpBrowserBasicInitialization {
  _appLaunchTime = base::TimeTicks::Now();
  _isColdStart = YES;

  [SetupDebugging setUpDebuggingOptions];

  // Register all providers before calling any Chromium code.
  [ProviderRegistration registerProviders];
}

- (void)startUpBrowserBackgroundInitialization {
  DCHECK(![self.appState isInSafeMode]);

  NSBundle* baseBundle = base::mac::OuterBundle();
  base::mac::SetBaseBundleID(
      base::SysNSStringToUTF8([baseBundle bundleIdentifier]).c_str());

  // Register default values for experimental settings (Application Preferences)
  // and set the "Version" key in the UserDefaults.
  [RegisterExperimentalSettings
      registerExperimentalSettingsWithUserDefaults:[NSUserDefaults
                                                       standardUserDefaults]
                                            bundle:base::mac::
                                                       FrameworkBundle()];

  // Register all clients before calling any web code.
  [ClientRegistration registerClients];

  _chromeMain = [ChromeMainStarter startChromeMain];

  // Initialize the ChromeBrowserProvider.
  ios::GetChromeBrowserProvider()->Initialize();

  // If the user is interacting, crashes affect the user experience. Start
  // reporting as soon as possible.
  // TODO(crbug.com/507633): Call this even sooner.
  if (_appState.userInteracted)
    GetApplicationContext()->GetMetricsService()->OnAppEnterForeground();

  web::WebUIIOSControllerFactory::RegisterFactory(
      ChromeWebUIIOSControllerFactory::GetInstance());

  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];
}

- (void)startUpBrowserForegroundInitialization {
  // Give tests a chance to prepare for testing.
  tests_hook::SetUpTestsIfPresent();

  GetApplicationContext()->OnAppEnterForeground();

  // TODO(crbug.com/546171): Audit all the following code to see if some of it
  // should move into BrowserMainParts or BrowserProcess.
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];

  // Although this duplicates some metrics_service startup logic also in
  // IOSChromeMain(), this call does additional work, checking for wifi-only
  // and setting up the required support structures.
  [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];

  // Crash the app during startup if requested but only after we have enabled
  // uploading crash reports.
  [self crashIfRequested];

  if (experimental_flags::MustClearApplicationGroupSandbox()) {
    // Clear the Application group sandbox if requested. This operation take
    // some time and will access the file system synchronously as the rest of
    // the startup sequence requires it to be completed before continuing.
    app_group::ClearAppGroupSandbox();
  }

  RegisterComponentsForUpdate();

  // Remove the extra browser states as Chrome iOS is single profile in M48+.
  ChromeBrowserStateRemovalController::GetInstance()
      ->RemoveBrowserStatesIfNecessary();

  _browserStateManager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  ios::ChromeBrowserState* chromeBrowserState =
      _browserStateManager->GetLastUsedBrowserState();

  // The CrashRestoreHelper must clean up the old browser state information
  // before the tabModels can be created.  |_restoreHelper| must be kept alive
  // until the BVC receives the browser state and tab model.
  BOOL postCrashLaunch = [self mustShowRestoreInfobar];
  if (postCrashLaunch) {
    _restoreHelper =
        [[CrashRestoreHelper alloc] initWithBrowserState:chromeBrowserState];
    [_restoreHelper moveAsideSessionInformation];
  }

  _appURLLoadingService = new AppUrlLoadingService();
  _appURLLoadingService->SetDelegate(self);

  // Initialize and set the main browser state.
  [self initializeBrowserState:chromeBrowserState];
  _mainBrowserState = chromeBrowserState;
  [_browserViewWrangler shutdown];
  _browserViewWrangler =
      [[BrowserViewWrangler alloc] initWithBrowserState:_mainBrowserState
                                   webStateListObserver:self
                             applicationCommandEndpoint:self
                                   appURLLoadingService:_appURLLoadingService
                                        storageSwitcher:self];

  // Force an obvious initialization of the AuthenticationService. This must
  // be done before creation of the UI to ensure the service is initialised
  // before use (it is a security issue, so accessing the service CHECK if
  // this is not the case).
  DCHECK(_mainBrowserState);
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      _mainBrowserState,
      std::make_unique<MainControllerAuthenticationServiceDelegate>(
          _mainBrowserState, self));

  // Send "Chrome Opened" event to the feature_engagement::Tracker on cold
  // start.
  feature_engagement::TrackerFactory::GetForBrowserState(chromeBrowserState)
      ->NotifyEvent(feature_engagement::events::kChromeOpened);

  // Make sure Roboto is available before any UI is created.
  [MDCTypography setFontLoader:[MDFRobotoFontLoader sharedInstance]];

  // Ensure the main tab model is created. This also creates the BVC.
  [_browserViewWrangler createMainBrowser];

  _spotlightManager =
      [SpotlightManager spotlightManagerWithBrowserState:_mainBrowserState];

  ShareExtensionService* service =
      ShareExtensionServiceFactory::GetForBrowserState(_mainBrowserState);
  service->Initialize();

  // Before bringing up the UI, make sure the launch mode is correct, and
  // check for previous crashes.
  BOOL startInIncognito = [standardDefaults boolForKey:kIncognitoCurrentKey];
  BOOL switchFromIncognito = startInIncognito && ![self canLaunchInIncognito];

  if (postCrashLaunch || switchFromIncognito) {
    [self clearIOSSpecificIncognitoData];
    if (switchFromIncognito)
      [self switchGlobalStateToMode:ApplicationMode::NORMAL];
  }
  if (switchFromIncognito)
    startInIncognito = NO;

  if ([PreviousSessionInfo sharedInstance].isFirstSessionAfterLanguageChange) {
    IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
        chromeBrowserState)
        ->ClearAllCachedSuggestions();
  }

  [self createInitialUI:(startInIncognito ? ApplicationMode::INCOGNITO
                                          : ApplicationMode::NORMAL)];

  [self scheduleStartupCleanupTasks];
  [MetricsMediator
      logLaunchMetricsWithStartupInformation:self
                           interfaceProvider:self.interfaceProvider];
  if (self.isColdStart) {
    [ContentSuggestionsSchedulerNotifications
        notifyColdStart:_mainBrowserState];
    [ContentSuggestionsSchedulerNotifications
        notifyForeground:_mainBrowserState];
  }

  ios::GetChromeBrowserProvider()->GetOverridesProvider()->InstallOverrides();

  [self scheduleLowPriorityStartupTasks];

  [_browserViewWrangler updateDeviceSharingManager];

  [self openTabFromLaunchOptions:_launchOptions
              startupInformation:self
                        appState:self.appState];
  _launchOptions = nil;

  if (!_startupParameters) {
    // The startup parameters may create new tabs or navigations. If the restore
    // infobar is displayed now, it may be dismissed immediately and the user
    // will never be able to restore the session.
    TabModel* currentTabModel = [self currentTabModel];
    [_restoreHelper
        showRestoreIfNeededUsingWebState:currentTabModel.webStateList
                                             ->GetActiveWebState()
                         sessionRestorer:currentTabModel];
    _restoreHelper = nil;
  }

  [self scheduleTasksRequiringBVCWithBrowserState];

  // Now that everything is properly set up, run the tests.
  tests_hook::RunTestsIfPresent();
}

- (void)initializeBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(!browserState->IsOffTheRecord());
  search_engines::UpdateSearchEnginesIfNeeded(
      browserState->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState));
}

- (void)handleFirstRunUIWillFinish {
  DCHECK(_isPresentingFirstRunUI);
  _isPresentingFirstRunUI = NO;
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIWillFinishNotification
              object:nil];

  [self markEulaAsAccepted];

  if (_startupParameters) {
    UrlLoadParams params =
        UrlLoadParams::InNewTab(_startupParameters.externalURL);
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationMode::NORMAL
                              withUrlLoadParams:params
                                 dismissOmnibox:YES
                                     completion:^{
                                       [self setStartupParameters:nil];
                                     }];
  }
}

- (void)handleFirstRunUIDidFinish {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIDidFinishNotification
              object:nil];

  // As soon as First Run has finished, give OmniboxGeolocationController an
  // opportunity to present the iOS system location alert.
  [[OmniboxGeolocationController sharedInstance]
      triggerSystemPromptForNewUser:YES];
}

- (void)clearIOSSpecificIncognitoData {
  DCHECK(_mainBrowserState->HasOffTheRecordChromeBrowserState());
  ios::ChromeBrowserState* otrBrowserState =
      _mainBrowserState->GetOffTheRecordChromeBrowserState();
  [self removeBrowsingDataForBrowserState:otrBrowserState
                               timePeriod:browsing_data::TimePeriod::ALL_TIME
                               removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                          completionBlock:^{
                            [self activateBVCAndMakeCurrentBVCPrimary];
                          }];
}

- (void)destroyAndRebuildIncognitoBrowserState {
  BOOL otrBVCIsCurrent = (self.currentBVC == self.otrBVC);

  // Clear the OTR tab model and notify the _tabSwitcher that its otrBVC will
  // be destroyed.
  [_tabSwitcher setOtrTabModel:nil];

  [_browserViewWrangler destroyAndRebuildIncognitoBrowser];

  if (otrBVCIsCurrent) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  }

  // Always set the new otr tab model for the tablet or grid switcher.
  // Notify the _tabSwitcher with the new otrBVC.
  [_tabSwitcher setOtrTabModel:self.otrTabModel];

  // This seems the best place to deem the destroying and rebuilding the
  // incognito browser state to be completed.
  breakpad_helper::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/false);
}

- (void)activateBVCAndMakeCurrentBVCPrimary {
  // If there are pending removal operations, the activation will be deferred
  // until the callback is received.
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserStateIfExists(
          self.currentBrowserState);
  if (browsingDataRemover && browsingDataRemover->IsRemoving())
    return;

  self.interfaceProvider.mainInterface.userInteractionEnabled = YES;
  self.interfaceProvider.incognitoInterface.userInteractionEnabled = YES;
  [self.currentBVC setPrimary:YES];
}

#pragma mark - Property implementation.

- (id<BrowserInterfaceProvider>)interfaceProvider {
  return _browserViewWrangler;
}

- (TabGridCoordinator*)mainCoordinator {
  if (_browserInitializationStage == INITIALIZATION_STAGE_BASIC) {
    NOTREACHED() << "mainCoordinator accessed too early in initialization.";
    return nil;
  }
  if (!_mainCoordinator) {
    // Lazily create the main coordinator.
    TabGridCoordinator* tabGridCoordinator =
        [[TabGridCoordinator alloc] initWithWindow:self.window
                        applicationCommandEndpoint:self];
    tabGridCoordinator.regularTabModel = self.mainTabModel;
    tabGridCoordinator.incognitoTabModel = self.otrTabModel;
    _mainCoordinator = tabGridCoordinator;
  }
  return _mainCoordinator;
}

- (BOOL)isFirstLaunchAfterUpgrade {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

- (BOOL)isSettingsViewPresented {
  return _settingsNavigationController ||
         self.signinInteractionCoordinator.isSettingsViewPresented;
}

#pragma mark - StartupInformation implementation.

- (FirstUserActionRecorder*)firstUserActionRecorder {
  return _firstUserActionRecorder.get();
}

- (void)resetFirstUserActionRecorder {
  _firstUserActionRecorder.reset();
}

- (void)expireFirstUserActionRecorderAfterDelay:(NSTimeInterval)delay {
  [self performSelector:@selector(expireFirstUserActionRecorder)
             withObject:nil
             afterDelay:delay];
}

- (void)activateFirstUserActionRecorderWithBackgroundTime:
    (NSTimeInterval)backgroundTime {
  base::TimeDelta delta = base::TimeDelta::FromSeconds(backgroundTime);
  _firstUserActionRecorder.reset(new FirstUserActionRecorder(delta));
}

- (void)stopChromeMain {
  // The UI should be stopped before the models they observe are stopped.
  [_mainCoordinator stop];
  _mainCoordinator = nil;

  [_spotlightManager shutdown];
  _spotlightManager = nil;

  // Invariant: The UI is stopped before the model is shutdown.
  DCHECK(!_mainCoordinator);
  [_browserViewWrangler shutdown];
  _browserViewWrangler = nil;

  _extensionSearchEngineDataUpdater = nullptr;

  [_historyCoordinator stop];
  _historyCoordinator = nil;

  ios::GetChromeBrowserProvider()
      ->GetMailtoHandlerProvider()
      ->RemoveMailtoHandling();
  // _localStatePrefChangeRegistrar is observing the PrefService, which is owned
  // indirectly by _chromeMain (through the ChromeBrowserState).
  // Unregister the observer before the service is destroyed.
  _localStatePrefChangeRegistrar.RemoveAll();

  _chromeMain.reset();
}

- (BOOL)isTabSwitcherActive {
  return _tabSwitcherIsActive;
}

#pragma mark - Startup tasks

- (void)sendQueuedFeedback {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSendQueuedFeedback
                  block:^{
                    ios::GetChromeBrowserProvider()
                        ->GetUserFeedbackProvider()
                        ->Synchronize();
                  }];
}

- (void)orientationDidChange:(NSNotification*)notification {
  breakpad_helper::SetCurrentOrientation(
      [[UIApplication sharedApplication] statusBarOrientation],
      [[UIDevice currentDevice] orientation]);
}

- (void)registerForOrientationChangeNotifications {
  // Register to both device orientation and UI orientation did change
  // notification as these two events may be triggered independantely.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationDidChange:)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationDidChange:)
             name:UIApplicationDidChangeStatusBarOrientationNotification
           object:nil];
}

- (void)schedulePrefObserverInitialization {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPrefObserverInit
                  block:^{
                    // Track changes to local state prefs.
                    _localStatePrefObserverBridge.reset(
                        new PrefObserverBridge(self));
                    _localStatePrefChangeRegistrar.Init(
                        GetApplicationContext()->GetLocalState());
                    _localStatePrefObserverBridge->ObserveChangesForPreference(
                        metrics::prefs::kMetricsReportingEnabled,
                        &_localStatePrefChangeRegistrar);
                    if (!base::FeatureList::IsEnabled(kUmaCellular)) {
                      _localStatePrefObserverBridge
                          ->ObserveChangesForPreference(
                              prefs::kMetricsReportingWifiOnly,
                              &_localStatePrefChangeRegistrar);
                    }

                    // Calls the onPreferenceChanged function in case there was
                    // a change to the observed preferences before the observer
                    // bridge was set up.
                    [self onPreferenceChanged:metrics::prefs::
                                                  kMetricsReportingEnabled];
                    [self onPreferenceChanged:prefs::kMetricsReportingWifiOnly];

                    // Track changes to default search engine.
                    TemplateURLService* service =
                        ios::TemplateURLServiceFactory::GetForBrowserState(
                            _mainBrowserState);
                    _extensionSearchEngineDataUpdater =
                        std::make_unique<ExtensionSearchEngineDataUpdater>(
                            service);
                  }];
}

- (void)scheduleAppDistributionPings {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSendInstallPingIfNecessary
                  block:^{
                    auto URLLoaderFactory =
                        _mainBrowserState->GetSharedURLLoaderFactory();
                    bool is_first_run = FirstRun::IsChromeFirstRun();
                    ios::GetChromeBrowserProvider()
                        ->GetAppDistributionProvider()
                        ->ScheduleDistributionNotifications(URLLoaderFactory,
                                                            is_first_run);
                    InitializeFirebase(is_first_run);
                  }];
}

- (void)scheduleAuthenticationServiceNotification {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kAuthenticationServiceNotification
                  block:^{
                    // Active browser state should have been set before
                    // scheduling any authentication service notification.
                    DCHECK([self currentBrowserState]);
                    if ([SignedInAccountsViewController
                            shouldBePresentedForBrowserState:
                                [self currentBrowserState]]) {
                      [self
                          presentSignedInAccountsViewControllerForBrowserState:
                              [self currentBrowserState]];
                    }
                  }];
}

- (void)scheduleStartupAttemptReset {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartupAttemptReset
                  block:^{
                    crash_util::ResetFailedStartupAttemptCount();
                  }];
}

- (void)scheduleCrashReportCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCleanupCrashReports
                  block:^{
                    breakpad_helper::CleanupCrashReports();
                  }];
}

- (void)scheduleSnapshotPurge {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPurgeSnapshots
                  block:^{
                    [self purgeSnapshots];
                  }];
}

- (void)scheduleStartupCleanupTasks {
  // Cleanup crash reports if this is the first run after an update.
  if ([self isFirstLaunchAfterUpgrade]) {
    [self scheduleCrashReportCleanup];
  }

  // ClearSessionCookies() is not synchronous.
  if (cookie_util::ShouldClearSessionCookies()) {
    cookie_util::ClearSessionCookies(
        _mainBrowserState->GetOriginalChromeBrowserState());
    if (![self.otrTabModel isEmpty]) {
      cookie_util::ClearSessionCookies(
          _mainBrowserState->GetOffTheRecordChromeBrowserState());
    }
  }

  // If the user chooses to restore their session, some cached snapshots may
  // be needed. Otherwise, purge the cached snapshots.
  if (![self mustShowRestoreInfobar]) {
    [self scheduleSnapshotPurge];
  }
}

- (void)scheduleMemoryDebuggingTools {
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    [[DeferredInitializationRunner sharedInstance]
        enqueueBlockNamed:kMemoryDebuggingToolsStartup
                    block:^{
                      _memoryDebuggerManager = [[MemoryDebuggerManager alloc]
                          initWithView:self.window
                                 prefs:GetApplicationContext()
                                           ->GetLocalState()];
                    }];
  }
}

- (void)initializeMailtoHandling {
  __weak __typeof(self) weakSelf = self;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kMailtoHandlingInitialization
                  block:^{
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    if (!strongSelf || !strongSelf->_mainBrowserState) {
                      return;
                    }
                    ios::GetChromeBrowserProvider()
                        ->GetMailtoHandlerProvider()
                        ->PrepareMailtoHandling(strongSelf->_mainBrowserState);
                  }];
}

/**
 Schedule a call to |saveFieldTrialValuesForExtensions| for deferred execution.
 */
- (void)scheduleSaveFieldTrialValuesForExtensions {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSaveFieldTrialValues
                  block:^{
                    [self saveFieldTrialValuesForExtensions];
                  }];
}

/**
 Some extensions need the value of field trials but can't get them because
 the field trial infrastruction isn't in extensions. Save the necessary
 values to NSUserDefaults here.
 */
- (void)saveFieldTrialValuesForExtensions {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  NSString* fieldTrialValueKey =
      base::SysUTF8ToNSString(app_group::kChromeExtensionFieldTrialPreference);
  NSNumber* copiedContentBehaviorValue = [NSNumber
      numberWithBool:base::FeatureList::IsEnabled(kCopiedContentBehavior)];

  /**
   Add other field trial values here if they are needed by extensions.
   The general format is
   {
     name: {
       value: bool,
       version: bool
     }
   }
   */
  NSDictionary* fieldTrialValues = @{
    base::SysUTF8ToNSString(kCopiedContentBehavior.name) : @{
      kFieldTrialValueKey : copiedContentBehaviorValue,
      kFieldTrialVersionKey : kCopiedContentBehaviorVersion
    }
  };
  [sharedDefaults setObject:fieldTrialValues forKey:fieldTrialValueKey];
}

- (void)startFreeMemoryMonitoring {
  // No need for a post-task or a deferred initialisation as the memory
  // monitoring already happens on a background sequence.
  StartFreeMemoryMonitor();
}

- (void)scheduleLowPriorityStartupTasks {
  [_startupTasks initializeOmaha];
  [_startupTasks donateIntents];
  [_startupTasks registerForApplicationWillResignActiveNotification];
  [self registerForOrientationChangeNotifications];

  // Deferred tasks.
  [self schedulePrefObserverInitialization];
  [self scheduleMemoryDebuggingTools];
  [StartupTasks scheduleDeferredBrowserStateInitialization:_mainBrowserState];
  [self scheduleAuthenticationServiceNotification];
  [self sendQueuedFeedback];
  [self scheduleSpotlightResync];
  [self scheduleDeleteDownloadsDirectory];
  [self scheduleDeleteTempPasswordsDirectory];
  [self scheduleStartupAttemptReset];
  [self startFreeMemoryMonitoring];
  [self scheduleAppDistributionPings];
  [self initializeMailtoHandling];
  [self scheduleSaveFieldTrialValuesForExtensions];
}

- (void)scheduleTasksRequiringBVCWithBrowserState {
  if (GetApplicationContext()->WasLastShutdownClean()) {
    // Delay the cleanup of the unreferenced files to not impact startup
    // performance.
    ExternalFileRemoverFactory::GetForBrowserState(_mainBrowserState)
        ->RemoveAfterDelay(
            base::TimeDelta::FromSeconds(kExternalFilesCleanupDelaySeconds),
            base::OnceClosure());
  }
  [self scheduleShowPromo];
}

- (void)scheduleDeleteDownloadsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteDownloads
                  block:^{
                    DeleteDownloadsDirectory();
                  }];
}

- (void)scheduleDeleteTempPasswordsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteTempPasswords
                  block:^{
                    password_manager::DeletePasswordsDirectory();
                  }];
}

- (void)scheduleSpotlightResync {
  if (!_spotlightManager) {
    return;
  }
  ProceduralBlock block = ^{
    [_spotlightManager resyncIndex];
  };
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartSpotlightBookmarksIndexing
                  block:block];
}

- (void)expireFirstUserActionRecorder {
  // Clear out any scheduled calls to this method. For example, the app may have
  // been backgrounded before the |kFirstUserActionTimeout| expired.
  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:@selector(
                                                  expireFirstUserActionRecorder)
                                       object:nil];

  if (_firstUserActionRecorder) {
    _firstUserActionRecorder->Expire();
    _firstUserActionRecorder.reset();
  }
}

- (BOOL)canLaunchInIncognito {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  if (![standardDefaults boolForKey:kIncognitoCurrentKey])
    return NO;
  // If the application crashed in incognito mode, don't stay in incognito
  // mode, since the prompt to restore should happen in non-incognito
  // context.
  if ([self mustShowRestoreInfobar])
    return NO;
  // If there are no incognito tabs, then ensure the app starts in normal mode,
  // since the UI isn't supposed to ever put the user in incognito mode without
  // any incognito tabs.
  return ![self.otrTabModel isEmpty];
}

- (void)createInitialUI:(ApplicationMode)launchMode {
  DCHECK(_mainBrowserState);

  // In order to correctly set the mode switch icon, we need to know how many
  // tabs are in the other tab model. That means loading both models.  They
  // may already be loaded.
  // TODO(crbug.com/546203): Find a way to handle this that's closer to the
  // point where it is necessary.
  TabModel* mainTabModel = self.mainTabModel;
  TabModel* otrTabModel = self.otrTabModel;

  // MainCoordinator shouldn't have been initialized yet.
  DCHECK(!_mainCoordinator);

  // Enables UI initializations to query the keyWindow's size.
  [self.window makeKeyAndVisible];

  // Lazy init of mainCoordinator.
  [self.mainCoordinator start];
  _tabSwitcher = self.mainCoordinator.tabSwitcher;
  // Call -restoreInternalState so that the grid shows the correct panel.
  [_tabSwitcher restoreInternalStateWithMainTabModel:self.mainTabModel
                                         otrTabModel:self.otrTabModel
                                      activeTabModel:self.currentTabModel];

  // Decide if the First Run UI needs to run.
  BOOL firstRun = (FirstRun::IsChromeFirstRun() ||
                   experimental_flags::AlwaysDisplayFirstRun()) &&
                  !tests_hook::DisableFirstRun();

  ios::ChromeBrowserState* browserState =
      (launchMode == ApplicationMode::INCOGNITO)
          ? _mainBrowserState->GetOffTheRecordChromeBrowserState()
          : _mainBrowserState;
  [self changeStorageFromBrowserState:nullptr toBrowserState:browserState];

  TabModel* tabModel;
  if (launchMode == ApplicationMode::INCOGNITO) {
    tabModel = otrTabModel;
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  } else {
    tabModel = mainTabModel;
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }
  if (_tabSwitcherIsActive) {
    DCHECK(!_dismissingTabSwitcher);
    [self beginDismissingTabSwitcherWithCurrentModel:self.mainTabModel
                                        focusOmnibox:NO];
    [self finishDismissingTabSwitcher];
  }
  if (firstRun || [self shouldOpenNTPTabOnActivationOfTabModel:tabModel]) {
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:(self.currentBVC == self.otrBVC)];
    command.userInitiated = NO;
    [self.currentBVC.dispatcher openURLInNewTab:command];
  }

  if (firstRun) {
    [self showFirstRunUI];
    // Do not ever show the 'restore' infobar during first run.
    _restoreHelper = nil;
  }
}

- (void)showFirstRunUI {
  // Register for notification when First Run is completed.
  // Some initializations are held back until First Run modal dialog
  // is dismissed.
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

  WelcomeToChromeViewController* welcomeToChrome =
      [[WelcomeToChromeViewController alloc]
          initWithBrowserState:_mainBrowserState
                      tabModel:self.mainTabModel
                     presenter:self.mainBVC
                    dispatcher:self.mainBVC.dispatcher];
  UINavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:welcomeToChrome];
  [navController setModalTransitionStyle:UIModalTransitionStyleCrossDissolve];
  CGRect appFrame = [[UIScreen mainScreen] bounds];
  [[navController view] setFrame:appFrame];
  _isPresentingFirstRunUI = YES;
  [self.mainBVC presentViewController:navController animated:NO completion:nil];
}

- (void)crashIfRequested {
  if (experimental_flags::IsStartupCrashEnabled()) {
    // Flush out the value cached for breakpad::SetUploadingEnabled().
    [[NSUserDefaults standardUserDefaults] synchronize];

    int* x = NULL;
    *x = 0;
  }
}

#pragma mark - Promo support

- (void)scheduleShowPromo {
  // Don't show promos if first run is shown.  (Note:  This flag is only YES
  // while the first run UI is visible.  However, as this function is called
  // immediately after the UI is shown, it's a safe check.)
  if (_isPresentingFirstRunUI)
    return;
  // Don't show promos in Incognito mode.
  if (self.currentBVC == self.otrBVC)
    return;
  // Don't show promos if the app was launched from a URL.
  if (_startupParameters)
    return;

  // Show the sign-in promo if needed
  if ([SigninPromoViewController
          shouldBePresentedForBrowserState:_mainBrowserState]) {
    UIViewController* promoController = [[SigninPromoViewController alloc]
        initWithBrowserState:_mainBrowserState
                  dispatcher:self.mainBVC.dispatcher];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(kDisplayPromoDelay * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [self showPromo:promoController];
                   });
  }
}

- (void)showPromo:(UIViewController*)promo {
  // Make sure we have the BVC here with a valid profile.
  DCHECK([self.currentBVC browserState]);

  OrientationLimitingNavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:promo];

  // Avoid presenting the promo if the current device orientation is not
  // supported. The promo will be presented at a later moment, when the device
  // orientation is supported.
  UIInterfaceOrientation orientation =
      [UIApplication sharedApplication].statusBarOrientation;
  NSUInteger supportedOrientationsMask =
      [navController supportedInterfaceOrientations];
  if (!((1 << orientation) & supportedOrientationsMask))
    return;

  [navController setModalTransitionStyle:[promo modalTransitionStyle]];
  [navController setNavigationBarHidden:YES];
  [[navController view] setFrame:[[UIScreen mainScreen] bounds]];

  [self.mainBVC presentViewController:navController
                             animated:YES
                           completion:nil];
}

#pragma mark - ApplicationCommands

- (void)dismissModalDialogs {
  [self dismissModalDialogsWithCompletion:nil dismissOmnibox:YES];
}

- (void)showHistory {
  _historyCoordinator =
      [[HistoryCoordinator alloc] initWithBaseViewController:self.currentBVC
                                                browserState:_mainBrowserState];
  _historyCoordinator.loadStrategy = [self currentPageIsIncognito]
                                         ? UrlLoadStrategy::ALWAYS_IN_INCOGNITO
                                         : UrlLoadStrategy::NORMAL;
  _historyCoordinator.dispatcher = self.mainBVC.dispatcher;
  [_historyCoordinator start];
}

- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command {
  [self openUrlFromSettings:command];
}

- (void)closeSettingsUI {
  [self closeSettingsAnimated:YES completion:NULL];
}

- (void)prepareTabSwitcher {
  web::WebState* currentWebState = self.currentBVC.tabModel.currentTab.webState;
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
  [self.mainCoordinator prepareToShowTabSwitcher:_tabSwitcher];
}

- (void)displayTabSwitcher {
  DCHECK(!_tabSwitcherIsActive);
  if (!_isProcessingVoiceSearchCommand) {
    [self.currentBVC userEnteredTabSwitcher];
    [self showTabSwitcher];
    _isProcessingTabSwitcherCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     _isProcessingTabSwitcherCommand = NO;
                   });
  }
}

// TODO(crbug.com/779791) : Remove showing settings from MainController.
- (void)showAutofillSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController)
    return;
  _settingsNavigationController = [SettingsNavigationController
      newAutofillProfilleController:_mainBrowserState
                           delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showReportAnIssueFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(baseViewController);
  DCHECK(![baseViewController presentedViewController]);
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  dispatch_async(dispatch_get_main_queue(), ^{
    DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
    if (_settingsNavigationController)
      return;
    _settingsNavigationController = [SettingsNavigationController
        newUserFeedbackController:_mainBrowserState
                         delegate:self
               feedbackDataSource:self];
    [baseViewController presentViewController:_settingsNavigationController
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
  _appURLLoadingService->LoadUrlInNewTab(params);
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  if (!self.signinInteractionCoordinator) {
    self.signinInteractionCoordinator = [[SigninInteractionCoordinator alloc]
        initWithBrowserState:_mainBrowserState
                  dispatcher:self.mainBVC.dispatcher];
  }

  switch (command.operation) {
    case AUTHENTICATION_OPERATION_REAUTHENTICATE:
      [self.signinInteractionCoordinator
          reAuthenticateWithAccessPoint:command.accessPoint
                            promoAction:command.promoAction
               presentingViewController:baseViewController
                             completion:command.callback];
      break;
    case AUTHENTICATION_OPERATION_SIGNIN:
      [self.signinInteractionCoordinator signInWithIdentity:command.identity
                                                accessPoint:command.accessPoint
                                                promoAction:command.promoAction
                                   presentingViewController:baseViewController
                                                 completion:command.callback];
      break;
  }
}

- (void)showAdvancedSigninSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(unified_consent::IsUnifiedConsentFeatureEnabled());
  self.signinInteractionCoordinator = [[SigninInteractionCoordinator alloc]
      initWithBrowserState:_mainBrowserState
                dispatcher:self.mainBVC.dispatcher];
  [self.signinInteractionCoordinator
      showAdvancedSigninSettingsWithPresentingViewController:
          baseViewController];
}

// TODO(crbug.com/779791) : Remove settings commands from MainController.
- (void)showAddAccountFromViewController:(UIViewController*)baseViewController {
  if (!self.signinInteractionCoordinator) {
    self.signinInteractionCoordinator = [[SigninInteractionCoordinator alloc]
        initWithBrowserState:_mainBrowserState
                  dispatcher:self.mainBVC.dispatcher];
  }

  [self.signinInteractionCoordinator
      addAccountWithAccessPoint:signin_metrics::AccessPoint::
                                    ACCESS_POINT_UNKNOWN
                    promoAction:signin_metrics::PromoAction::
                                    PROMO_ACTION_NO_SIGNIN_PROMO
       presentingViewController:baseViewController
                     completion:nil];
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  _incognitoContentVisible = incognitoContentVisible;
}

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Remove show settings from MainController.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (!baseViewController) {
    DCHECK_EQ(self.currentBVC, self.mainCoordinator.activeViewController);
    baseViewController = self.currentBVC;
  }
  DCHECK(![baseViewController presentedViewController]);
  if ([self currentBrowserState]->IsOffTheRecord()) {
    NOTREACHED();
    return;
  }
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      newAccountsController:self.currentBrowserState
                   delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove Google services settings from MainController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (!baseViewController) {
    DCHECK_EQ(self.currentBVC, self.mainCoordinator.activeViewController);
    baseViewController = self.currentBVC;
  }
  DCHECK(![baseViewController presentedViewController]);
  if (_settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [_settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  ios::ChromeBrowserState* originalBrowserState =
      self.currentBrowserState->GetOriginalChromeBrowserState();
  _settingsNavigationController = [SettingsNavigationController
      newGoogleServicesController:originalBrowserState
                         delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  // When unified consent feather is enabled,
  // |showGoogleServicesSettingsFromViewController:| should be called.
  DCHECK(!unified_consent::IsUnifiedConsentFeatureEnabled());
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showSyncSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController =
      [SettingsNavigationController newSyncController:_mainBrowserState
                                             delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      newSyncEncryptionPassphraseController:_mainBrowserState
                                   delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController =
      [SettingsNavigationController newSavePasswordsController:_mainBrowserState
                                                      delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      newAutofillProfilleController:_mainBrowserState
                           delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showCreditCardSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showCreditCardSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      newAutofillCreditCardController:_mainBrowserState
                             delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

#pragma mark - AppURLLoadingServiceDelegate

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
  [self.currentBVC openNewTabFromOriginPoint:originPoint
                                focusOmnibox:focusOmnibox];
}

#pragma mark - ApplicationCommands helpers

- (void)openUrlFromSettings:(OpenNewTabCommand*)command {
  DCHECK([command fromChrome]);
  UrlLoadParams params = UrlLoadParams::InNewTab([command URL]);
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  ProceduralBlock completion = ^{
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationMode::NORMAL
                              withUrlLoadParams:params
                                 dismissOmnibox:YES
                                     completion:nil];
  };
  [self closeSettingsAnimated:YES completion:completion];
}

- (void)startVoiceSearchInCurrentBVC {
  // If the background (non-current) BVC is playing TTS audio, call
  // -startVoiceSearch on it to stop the TTS.
  BrowserViewController* backgroundBVC =
      self.mainBVC == self.currentBVC ? self.otrBVC : self.mainBVC;
  if (backgroundBVC.playingTTS)
    [backgroundBVC startVoiceSearch];
  else
    [self.currentBVC startVoiceSearch];
}

#pragma mark - Preferences Management

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Turn on or off metrics & crash reporting when either preference changes.
  if (preferenceName == metrics::prefs::kMetricsReportingEnabled ||
      preferenceName == prefs::kMetricsReportingWifiOnly) {
    [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:YES];
  }
}

#pragma mark - Helper methods backed by interfaces.

- (BrowserViewController*)mainBVC {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.mainInterface.bvc;
}

- (TabModel*)mainTabModel {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.mainInterface.tabModel;
}

- (BrowserViewController*)otrBVC {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.incognitoInterface.bvc;
}

- (TabModel*)otrTabModel {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.incognitoInterface.tabModel;
}

- (BrowserViewController*)currentBVC {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.currentInterface.bvc;
}

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

  self.interfaceProvider.currentInterface = newInterface;

  if (!_dismissingTabSwitcher)
    [self displayCurrentBVCAndFocusOmnibox:NO];

  // Tell the BVC that was made current that it can use the web.
  [self activateBVCAndMakeCurrentBVCPrimary];
}

#pragma mark - Tab closure handlers

- (void)lastIncognitoTabClosed {
  // This seems the best place to mark the start of destroying the incognito
  // browser state.
  breakpad_helper::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/true);
  DCHECK(_mainBrowserState->HasOffTheRecordChromeBrowserState());
  [self clearIOSSpecificIncognitoData];

  // OffTheRecordProfileIOData cannot be deleted before all the requests are
  // deleted. Queue browser state recreation on IO thread.
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {web::WebThread::IO}, base::DoNothing(), base::BindRepeating(^{
        [self destroyAndRebuildIncognitoBrowserState];
      }));

  // a) The first condition can happen when the last incognito tab is closed
  // from the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the otr tab model when it's not current.
  // Nothing to do here. The next user action (like clicking on an existing
  // regular tab or creating a new incognito tab from the settings menu) will
  // take care of the logic to mode switch.
  if (_tabSwitcherIsActive || ![self.currentTabModel isOffTheRecord]) {
    return;
  }

  if ([self.currentTabModel count] == 0U) {
    [self showTabSwitcher];
  } else {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }
}

- (void)lastRegularTabClosed {
  // a) The first condition can happen when the last regular tab is closed from
  // the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the main tab model when the main tab model is not
  // current.
  // Nothing to do here.
  if (_tabSwitcherIsActive || [self.currentTabModel isOffTheRecord]) {
    return;
  }

  [self showTabSwitcher];
}

#pragma mark - Mode Switching

- (void)startVoiceSearch {
  if (!_isProcessingTabSwitcherCommand) {
    [self startVoiceSearchInCurrentBVC];
    _isProcessingVoiceSearchCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     _isProcessingVoiceSearchCommand = NO;
                   });
  }
}

- (void)switchGlobalStateToMode:(ApplicationMode)mode {
  const BOOL incognito = (mode == ApplicationMode::INCOGNITO);
  // Write the state to disk of what is "active".
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:incognito forKey:kIncognitoCurrentKey];
  // Save critical state information for switching between normal and
  // incognito.
  [standardDefaults synchronize];
}

- (void)changeStorageFromBrowserState:(ios::ChromeBrowserState*)oldState
                       toBrowserState:(ios::ChromeBrowserState*)newState {
  ApplicationMode mode = newState->IsOffTheRecord() ? ApplicationMode::INCOGNITO
                                                    : ApplicationMode::NORMAL;
  [self switchGlobalStateToMode:mode];
}

- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox {
  ProceduralBlock completion = nil;
  if (focusOmnibox) {
    __weak BrowserViewController* weakCurrentBVC = self.currentBVC;
    completion = ^{
      [weakCurrentBVC.dispatcher focusOmnibox];
    };
  }
  [self.mainCoordinator showTabViewController:self.currentBVC
                                   completion:completion];
  [self.currentBVC.dispatcher
      setIncognitoContentVisible:(self.currentBVC == self.otrBVC)];
}

- (TabModel*)currentTabModel {
  return self.currentBVC.tabModel;
}

- (ios::ChromeBrowserState*)currentBrowserState {
  return self.currentBVC.browserState;
}

- (void)showTabSwitcher {
  DCHECK(_tabSwitcher);
  // Tab switcher implementations may need to rebuild state before being
  // displayed.
  [_tabSwitcher restoreInternalStateWithMainTabModel:self.mainTabModel
                                         otrTabModel:self.otrTabModel
                                      activeTabModel:self.currentTabModel];
  _tabSwitcherIsActive = YES;
  [_tabSwitcher setDelegate:self];

  [self.mainCoordinator showTabSwitcher:_tabSwitcher];
}

- (BOOL)shouldOpenNTPTabOnActivationOfTabModel:(TabModel*)tabModel {
  if (_tabSwitcherIsActive) {
    // Only attempt to dismiss the tab switcher and open a new tab if:
    // - there are no tabs open in either tab model, and
    // - the tab switcher controller is not directly or indirectly presenting
    // another view controller.
    if (![self.mainTabModel isEmpty] || ![self.otrTabModel isEmpty])
      return NO;

    // If the tabSwitcher is contained, check if the parent container is
    // presenting another view controller.
    if ([[_tabSwitcher viewController]
                .parentViewController presentedViewController]) {
      return NO;
    }

    // Check if the tabSwitcher is directly presenting another view controller.
    if ([_tabSwitcher viewController].presentedViewController) {
      return NO;
    }

    return YES;
  }
  return ![tabModel count] && [tabModel browserState] &&
         ![tabModel browserState]->IsOffTheRecord();
}

#pragma mark - TabSwitching implementation.

- (BOOL)openNewTabFromTabSwitcher {
  if (!_tabSwitcher)
    return NO;

  UrlLoadParams urlLoadParams =
      UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  urlLoadParams.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;

  [_tabSwitcher dismissWithNewTabAnimationToModel:self.mainTabModel
                                withUrlLoadParams:urlLoadParams
                                          atIndex:self.mainTabModel.count];
  return YES;
}

#pragma mark - TabSwitcherDelegate

- (void)tabSwitcher:(id<TabSwitcher>)tabSwitcher
    shouldFinishWithActiveModel:(TabModel*)tabModel
                   focusOmnibox:(BOOL)focusOmnibox {
  [self beginDismissingTabSwitcherWithCurrentModel:tabModel
                                      focusOmnibox:focusOmnibox];
}

- (void)tabSwitcherDismissTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
  [self finishDismissingTabSwitcher];
}

#pragma mark - TabSwitcherDelegate helper methods

- (void)beginDismissingTabSwitcherWithCurrentModel:(TabModel*)tabModel
                                      focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(tabModel == self.mainTabModel || tabModel == self.otrTabModel);

  _dismissingTabSwitcher = YES;
  ApplicationMode mode = (tabModel == self.mainTabModel)
                             ? ApplicationMode::NORMAL
                             : ApplicationMode::INCOGNITO;
  [self setCurrentInterfaceForMode:mode];

  // The call to set currentBVC above does not actually display the BVC, because
  // _dismissingTabSwitcher is YES.  So: Force the BVC transition to start.
  [self displayCurrentBVCAndFocusOmnibox:focusOmnibox];
}

- (void)finishDismissingTabSwitcher {
  // In real world devices, it is possible to have an empty tab model at the
  // finishing block of a BVC presentation animation. This can happen when the
  // following occur: a) There is JS that closes the last incognito tab, b) that
  // JS was paused while the user was in the tab switcher, c) the user enters
  // the tab, activating the JS while the tab is being presented. Effectively,
  // the BVC finishes the presentation animation, but there are no tabs to
  // display. The only appropriate action is to dismiss the BVC and return the
  // user to the tab switcher.
  if (self.currentTabModel.count == 0U) {
    _tabSwitcherIsActive = NO;
    _dismissingTabSwitcher = NO;
    _modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;
    self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
    [self showTabSwitcher];
    return;
  }

  // The tab switcher dismissal animation runs
  // as part of the BVC presentation process.  The BVC is presented before the
  // animations begin, so it should be the current active VC at this point.
  DCHECK_EQ(self.mainCoordinator.activeViewController, self.currentBVC);

  if (_modeToDisplayOnTabSwitcherDismissal ==
      TabSwitcherDismissalMode::NORMAL) {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  } else if (_modeToDisplayOnTabSwitcherDismissal ==
             TabSwitcherDismissalMode::INCOGNITO) {
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  }

  _modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;

  ProceduralBlock action = [self completionBlockForTriggeringAction:
                                     self.NTPActionAfterTabSwitcherDismissal];
  self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
  if (action) {
    action();
  }

  _tabSwitcherIsActive = NO;
  _dismissingTabSwitcher = NO;
}

#pragma mark - BrowsingDataCommands

- (void)removeBrowsingDataForBrowserState:(ios::ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  // TODO(crbug.com/632772): https://bugs.webkit.org/show_bug.cgi?id=149079
  // makes it necessary to disable web usage while clearing browsing data.
  // It is however unnecessary for off-the-record BrowserState (as the code
  // is not invoked) and has undesired side-effect (cause all regular tabs
  // to reload, see http://crbug.com/821753 for details).

  const BOOL disableWebUsageDuringRemoval =
      !browserState->IsOffTheRecord() &&
      IsRemoveDataMaskSet(removeMask, BrowsingDataRemoveMask::REMOVE_SITE_DATA);

  ProceduralBlock removeBrowsingDataBlock = ^{
    if (disableWebUsageDuringRemoval) {
      // Disables browsing and purges web views.
      // Must be called only on the main thread.
      DCHECK([NSThread isMainThread]);
      self.interfaceProvider.mainInterface.userInteractionEnabled = NO;
      self.interfaceProvider.incognitoInterface.userInteractionEnabled = NO;
    }

    BrowsingDataRemoverFactory::GetForBrowserState(browserState)
        ->Remove(timePeriod, removeMask, base::BindOnce(^{
                   // Activates browsing and enables web views.
                   // Must be called only on the main thread.
                   DCHECK([NSThread isMainThread]);
                   self.interfaceProvider.mainInterface.userInteractionEnabled =
                       YES;
                   self.interfaceProvider.incognitoInterface
                       .userInteractionEnabled = YES;
                   [self.currentBVC setPrimary:YES];

                   if (completionBlock)
                     completionBlock();
                 }));
  };

  // Removing browsing data triggers session restore in navigation manager. If
  // there is an in-progress session restore, wait for it to finish before
  // attempting to clear browsing data again.
  web::WebState* webState = [[[self.currentBVC tabModel] currentTab] webState];
  if (webState && webState->GetNavigationManager()) {
    webState->GetNavigationManager()->AddRestoreCompletionCallback(
        base::BindOnce(^{
          removeBrowsingDataBlock();
        }));
    return;
  }

  removeBrowsingDataBlock();
}

#pragma mark - Navigation Controllers

- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  UIViewController* accountsViewController =
      [[SignedInAccountsViewController alloc]
          initWithBrowserState:browserState
                    dispatcher:self.mainBVC.dispatcher];
  [[self topPresentedViewController]
      presentViewController:accountsViewController
                   animated:YES
                 completion:nil];
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinInteractionCoordinator.isSettingsViewPresented);
  if (_settingsNavigationController)
    return;
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];
  DCHECK(_localStatePrefObserverBridge);
  _settingsNavigationController = [SettingsNavigationController
      newSettingsMainControllerWithBrowserState:_mainBrowserState
                                       delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)dismissSigninInteractionCoordinator {
  // The SigninInteractionCoordinator must not be destroyed at this point, as
  // it may dismiss the sign in UI in a future callback.
  [self.signinInteractionCoordinator cancelAndDismiss];
}

- (void)closeSettingsAnimated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  if (_settingsNavigationController) {
    [_settingsNavigationController settingsWillBeDismissed];
    UIViewController* presentingViewController =
        [_settingsNavigationController presentingViewController];
    DCHECK(presentingViewController);
    [presentingViewController dismissViewControllerAnimated:animated
                                                 completion:completion];
    _settingsNavigationController = nil;
    return;
  }
  // |self.signinInteractionCoordinator| can also present settings, like
  // the advanced sign-in settings navigation controller. If the settings has
  // to be cloase, it is thus the responsibility of the main controller to
  // dismiss the the advanced sign-in settings by dismssing the settings
  // presented by |self.signinInteractionCoordinator|.
  DCHECK(self.signinInteractionCoordinator.isSettingsViewPresented);
  [self.signinInteractionCoordinator
      abortAndDismissSettingsViewAnimated:animated
                               completion:completion];
}

#pragma mark - WebStateListObserving

// Called when a WebState is removed. Triggers the switcher view when the last
// WebState is closed on a device that uses the switcher.
- (void)webStateList:(WebStateList*)notifiedWebStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  // Do nothing on initialization.
  if (![self currentTabModel].webStateList)
    return;

  if (notifiedWebStateList->empty()) {
    if (webState->GetBrowserState()->IsOffTheRecord()) {
      [self lastIncognitoTabClosed];
    } else {
      [self lastRegularTabClosed];
    }
  }
}

#pragma mark - Tab opening utility methods.

- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion {
  BrowserViewController* targetBVC =
      targetMode == ApplicationMode::NORMAL ? self.mainBVC : self.otrBVC;
  TabModel* targetTabModel = targetBVC.tabModel;

  Tab* currentTabInTargetBVC = [targetTabModel currentTab];

  // Don't call loadWithParams for chrome://newtab when it's already loaded.
  // Note that it's safe to use -GetVisibleURL here, as it doesn't matter if the
  // NTP hasn't finished loading.
  if (currentTabInTargetBVC.webState &&
      IsURLNtp(currentTabInTargetBVC.webState->GetVisibleURL()) &&
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
      !(currentTabInTargetBVC.webState &&
        IsURLNtp(currentTabInTargetBVC.webState->GetVisibleURL()))) {
    [targetBVC appendTabAddedCompletion:tabOpenedCompletion];
    UrlLoadParams newTabParams = urlLoadParams;
    newTabParams.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    newTabParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
    UrlLoadingServiceFactory::GetForBrowserState([targetBVC browserState])
        ->Load(newTabParams);
    return;
  }

  // Otherwise, load |urlLoadParams| in the current tab.
  UrlLoadParams sameTabParams = urlLoadParams;
  sameTabParams.disposition = WindowOpenDisposition::CURRENT_TAB;
  UrlLoadingServiceFactory::GetForBrowserState([targetBVC browserState])
      ->Load(sameTabParams);
  if (tabOpenedCompletion) {
    tabOpenedCompletion();
  }
}

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action {
  switch (action) {
    case START_VOICE_SEARCH:
      return ^{
        [self startVoiceSearchInCurrentBVC];
      };
    case START_QR_CODE_SCANNER:
      return ^{
        [self.currentBVC.dispatcher showQRScanner];
      };
    case FOCUS_OMNIBOX:
      return ^{
        [self.currentBVC.dispatcher focusOmnibox];
      };
    default:
      return nil;
  }
}

- (void)openSelectedTabInMode:(ApplicationMode)targetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion {
  id<BrowserInterface> targetInterface =
      targetMode == ApplicationMode::NORMAL
          ? self.interfaceProvider.mainInterface
          : self.interfaceProvider.incognitoInterface;
  NSUInteger tabIndex = NSNotFound;
  ProceduralBlock startupCompletion =
      [self completionBlockForTriggeringAction:[_startupParameters
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

  if (_tabSwitcherIsActive) {
    // If the tab switcher is already being dismissed, simply add the tab and
    // note that when the tab switcher finishes dismissing, the current BVC
    // should be switched to be the main BVC if necessary.
    if (_dismissingTabSwitcher) {
      _modeToDisplayOnTabSwitcherDismissal =
          targetMode == ApplicationMode::NORMAL
              ? TabSwitcherDismissalMode::NORMAL
              : TabSwitcherDismissalMode::INCOGNITO;
      [targetInterface.bvc appendTabAddedCompletion:tabOpenedCompletion];
      UrlLoadParams savedParams = urlLoadParams;
      savedParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
      UrlLoadingServiceFactory::GetForBrowserState(
          [targetInterface.bvc browserState])
          ->Load(savedParams);
    } else {
      // Voice search, QRScanner and the omnibox are presented by the BVC.
      // They must be started after the BVC view is added in the hierarchy.
      self.NTPActionAfterTabSwitcherDismissal =
          [_startupParameters postOpeningAction];
      [self setStartupParameters:nil];
      [_tabSwitcher dismissWithNewTabAnimationToModel:targetInterface.tabModel
                                    withUrlLoadParams:urlLoadParams
                                              atIndex:tabIndex];
    }
  } else {
    if (!self.currentBVC.presentedViewController) {
      [targetInterface.bvc expectNewForegroundTab];
    }
    [self setCurrentInterfaceForMode:targetMode];
    [self openOrReuseTabInMode:targetMode
             withUrlLoadParams:urlLoadParams
           tabOpenedCompletion:tabOpenedCompletion];
  }

  if (_restoreHelper) {
    // Now that all the operations on the tabs have been done, display the
    // restore infobar if needed.
    dispatch_async(dispatch_get_main_queue(), ^{
      TabModel* currentTabModel = [self currentTabModel];
      [_restoreHelper
          showRestoreIfNeededUsingWebState:currentTabModel.webStateList
                                               ->GetActiveWebState()
                           sessionRestorer:currentTabModel];
      _restoreHelper = nil;
    });
  }
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

  // Cancel interaction with SSO.
  // First, cancel the signin interaction.
  [self.signinInteractionCoordinator cancel];

  // Then, depending on what the SSO view controller is presented on, dismiss
  // it.
  ProceduralBlock completionWithBVC = ^{
    DCHECK(self.currentBVC);
    DCHECK(![self isTabSwitcherActive]);
    // This will dismiss the SSO view controller.
    [self.interfaceProvider.currentInterface
        clearPresentedStateWithCompletion:completion
                           dismissOmnibox:dismissOmnibox];
  };
  ProceduralBlock completionWithoutBVC = ^{
    // |self.currentBVC| may exist but tab switcher should be active.
    DCHECK([self isTabSwitcherActive]);
    // This will dismiss the SSO view controller.
    [self dismissSigninInteractionCoordinator];
    // History coordinator can be started on top of the tab grid. This is not
    // true of the other tab switchers.
    DCHECK(self.mainCoordinator);
    [self.mainCoordinator stopChildCoordinatorsWithCompletion:completion];
  };

  // As a top level rule, if the settings are showing, they need to be
  // dismissed. Then, based on whether the BVC is present or not, a different
  // completion callback is called.
  if (![self isTabSwitcherActive] && self.isSettingsViewPresented) {
    // In this case, the settings are up and the BVC is showing. Close the
    // settings then call the BVC completion.
    DCHECK(!self.signinInteractionCoordinator.isActive);
    [self closeSettingsAnimated:NO completion:completionWithBVC];
  } else if (self.isSettingsViewPresented) {
    // In this case, the settings are up but the BVC is not showing. Close the
    // settings then call the no-BVC completion.
    [self closeSettingsAnimated:NO completion:completionWithoutBVC];
  } else if (![self isTabSwitcherActive]) {
    // In this case, the settings are not shown but the BVC is showing. Call the
    // BVC completion.
    completionWithBVC();
  } else {
    // In this case, neither the settings nor the BVC are shown. Call the no-BVC
    // completion.
    completionWithoutBVC();
  }

  // Verify that no modal views are left presented.
  ios::GetChromeBrowserProvider()->LogIfModalViewsArePresented();
}

- (bool)mustShowRestoreInfobar {
  if ([self isFirstLaunchAfterUpgrade])
    return false;
  return !GetApplicationContext()->WasLastShutdownClean();
}

- (NSMutableSet*)liveSessionsForTabModel:(TabModel*)tabModel {
  WebStateList* webStateList = tabModel.webStateList;
  NSMutableSet* result = [NSMutableSet setWithCapacity:webStateList->count()];
  for (int index = 0; index < webStateList->count(); ++index) {
    web::WebState* webState = webStateList->GetWebStateAt(index);
    [result addObject:TabIdTabHelper::FromWebState(webState)->tab_id()];
  }
  return result;
}

- (void)purgeSnapshots {
  NSMutableSet* liveSessions = [self liveSessionsForTabModel:self.mainTabModel];
  [liveSessions unionSet:[self liveSessionsForTabModel:self.otrTabModel]];

  // Keep snapshots that are less than one minute old, to prevent a concurrency
  // issue if they are created while the purge is running.
  const base::Time oneMinuteAgo =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);
  [SnapshotCacheFactory::GetForBrowserState([self currentBrowserState])
      purgeCacheOlderThan:oneMinuteAgo
                  keeping:liveSessions];
}

- (void)markEulaAsAccepted {
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  if (!prefs->GetBoolean(prefs::kEulaAccepted))
    prefs->SetBoolean(prefs::kEulaAccepted, true);
  prefs->CommitPendingWrite();
}

#pragma mark - TabOpening implementation.

- (void)dismissModalsAndOpenSelectedTabInMode:(ApplicationMode)targetMode
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

- (void)openTabFromLaunchOptions:(NSDictionary*)launchOptions
              startupInformation:(id<StartupInformation>)startupInformation
                        appState:(AppState*)appState {
  if (launchOptions) {
    BOOL applicationIsActive =
        [[UIApplication sharedApplication] applicationState] ==
        UIApplicationStateActive;

    [URLOpener handleLaunchOptions:launchOptions
                 applicationActive:applicationIsActive
                         tabOpener:self
                startupInformation:startupInformation
                          appState:appState];
  }
}

- (BOOL)shouldCompletePaymentRequestOnCurrentTab:
    (id<StartupInformation>)startupInformation {
  if (!startupInformation.startupParameters)
    return NO;

  if (!startupInformation.startupParameters.completePaymentRequest)
    return NO;

  if (!base::FeatureList::IsEnabled(payments::features::kWebPaymentsNativeApps))
    return NO;

  payments::IOSPaymentInstrumentLauncher* paymentAppLauncher =
      payments::IOSPaymentInstrumentLauncherFactory::GetInstance()
          ->GetForBrowserState(_mainBrowserState);

  if (!paymentAppLauncher->delegate())
    return NO;

  std::string payment_id =
      startupInformation.startupParameters.externalURLParams
          .find(payments::kPaymentRequestIDExternal)
          ->second;
  if (paymentAppLauncher->payment_request_id() != payment_id)
    return NO;

  std::string payment_response =
      startupInformation.startupParameters.externalURLParams
          .find(payments::kPaymentRequestDataExternal)
          ->second;
  paymentAppLauncher->ReceiveResponseFromIOSPaymentInstrument(payment_response);
  [startupInformation setStartupParameters:nil];
  return YES;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (id<ApplicationCommands, BrowserCommands>)dispatcherForSettings {
  // Assume that settings always wants the dispatcher from the main BVC.
  return self.mainBVC.dispatcher;
}

#pragma mark - UserFeedbackDataSource

- (NSString*)currentPageDisplayURL {
  if (_tabSwitcherIsActive)
    return nil;
  web::WebState* webState = [[[self currentTabModel] currentTab] webState];
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
  if (_tabSwitcherIsActive)
    scale = 1.0;
  return CaptureView(lastView, scale);
}

- (BOOL)currentPageIsIncognito {
  return [self currentBrowserState]->IsOffTheRecord();
}

- (NSString*)currentPageSyncedUserName {
  ios::ChromeBrowserState* browserState = [self currentBrowserState];
  if (browserState->IsOffTheRecord())
    return nil;
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  std::string username = identity_manager->GetPrimaryAccountInfo().email;
  return username.empty() ? nil : base::SysUTF8ToNSString(username);
}

@end

#pragma mark - TestingOnly

@implementation MainController (TestingOnly)

- (DeviceSharingManager*)deviceSharingManager {
  return [_browserViewWrangler deviceSharingManager];
}

- (id<TabSwitcher>)tabSwitcher {
  return _tabSwitcher;
}

- (void)setTabSwitcher:(id<TabSwitcher>)switcher {
  _tabSwitcher = switcher;
}

- (UIViewController*)topPresentedViewController {
  // TODO(crbug.com/754642): Implement TopPresentedViewControllerFrom()
  // privately.
  return top_view_controller::TopPresentedViewControllerFrom(
      self.mainCoordinator.viewController);
}

- (void)setTabSwitcherActive:(BOOL)active {
  _tabSwitcherIsActive = active;
}

- (BOOL)dismissingTabSwitcher {
  return _dismissingTabSwitcher;
}

- (void)setStartupParametersWithURL:(const GURL&)launchURL {
  NSString* sourceApplication = @"Fake App";
  _startupParameters = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:net::NSURLWithGURL(launchURL)
                     fromSourceApplication:sourceApplication];
}

@end
