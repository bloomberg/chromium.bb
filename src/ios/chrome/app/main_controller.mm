// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"

#include <memory>

#include "base/ios/ios_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"
#include "components/breadcrumbs/core/features.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crl_set_remover.h"
#include "components/component_updater/installer_policies/autofill_states_component_installer.h"
#include "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#include "components/component_updater/installer_policies/safety_tips_component_installer.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#import "components/previous_session_info/previous_session_info.h"
#include "components/ukm/ios/features.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ios/chrome/app/app_metrics_app_state_agent.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/app/content_suggestions_scheduler_app_state_agent.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/memory_monitor.h"
#import "ios/chrome/app/safe_mode_app_state_agent.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/app/startup/chrome_main_starter.h"
#include "ios/chrome/app/startup/client_registration.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/app/startup/provider_registration.h"
#include "ios/chrome/app/startup/register_experimental_settings.h"
#include "ios/chrome/app/startup/setup_debugging.h"
#import "ios/chrome/app/startup_tasks.h"
#include "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/accessibility/window_accessibility_change_notifier_app_agent.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_removal_controller.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/sessions_storage_util.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/crash_helper.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#include "ios/chrome/browser/credential_provider/credential_provider_buildflags.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/external_files/external_file_remover_factory.h"
#import "ios/chrome/browser/external_files/external_file_remover_impl.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/incognito_usage_app_state_agent.h"
#import "ios/chrome/browser/metrics/window_configuration_recorder.h"
#import "ios/chrome/browser/net/cookie_util.h"
#import "ios/chrome/browser/omaha/omaha_service.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/screenshot/screenshot_metrics_recorder.h"
#import "ios/chrome/browser/search_engines/extension_search_engine_data_updater.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/scene_util.h"
#import "ios/chrome/browser/share_extension/share_extension_service.h"
#import "ios/chrome/browser/share_extension/share_extension_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_delegate.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/scene_delegate.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_utils.h"
#include "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/empty_nsurlcache.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#import "ios/public/provider/chrome/browser/overrides_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/web/common/features.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "net/base/mac/url_conversions.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#include "ios/chrome/browser/credential_provider/credential_provider_service_factory.h"
#include "ios/chrome/browser/credential_provider/credential_provider_support.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constants for deferring reseting the startup attempt count (to give the app
// a little while to make sure it says alive).
NSString* const kStartupAttemptReset = @"StartupAttempReset";

// Constants for deferring memory debugging tools startup.
NSString* const kMemoryDebuggingToolsStartup = @"MemoryDebuggingToolsStartup";

// Constant for deferring the cleanup of discarded sessions on disk.
NSString* const kCleanupDiscardedSessions = @"CleanupDiscardedSessions";

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

// Constants for deferred UMA logging of existing Siri User shortcuts.
NSString* const kLogSiriShortcuts = @"LogSiriShortcuts";

// Constants for deferred sending of queued feedback.
NSString* const kSendQueuedFeedback = @"SendQueuedFeedback";

// Constants for deferring the deletion of pre-upgrade crash reports.
NSString* const kCleanupCrashReports = @"CleanupCrashReports";

// Constants for deferring the cleanup of snapshots on disk.
NSString* const kCleanupSnapshots = @"CleanupSnapshots";

// Constants for deferring startup Spotlight bookmark indexing.
NSString* const kStartSpotlightBookmarksIndexing =
    @"StartSpotlightBookmarksIndexing";

// Constants for deferring the enterprise managed device check.
NSString* const kEnterpriseManagedDeviceCheck = @"EnterpriseManagedDeviceCheck";

// Constants for deferred deletion of leftover session state files.
NSString* const kPurgeWebSessionStates = @"PurgeWebSessionStates";

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

  RegisterOnDeviceHeadSuggestComponent(
      cus, GetApplicationContext()->GetApplicationLocale());
  RegisterSafetyTipsComponent(cus);
  RegisterAutofillStatesComponent(cus,
                                  GetApplicationContext()->GetLocalState());
}

// The delay, in seconds, for cleaning external files.
const int kExternalFilesCleanupDelaySeconds = 60;

// Delegate for the AuthenticationService.
class MainControllerAuthenticationServiceDelegate
    : public AuthenticationServiceDelegate {
 public:
  MainControllerAuthenticationServiceDelegate(
      ChromeBrowserState* browser_state,
      id<BrowsingDataCommands> dispatcher);
  ~MainControllerAuthenticationServiceDelegate() override;

  // AuthenticationServiceDelegate implementation.
  void ClearBrowsingData(ProceduralBlock completion) override;

 private:
  ChromeBrowserState* browser_state_ = nullptr;
  __weak id<BrowsingDataCommands> dispatcher_ = nil;

  DISALLOW_COPY_AND_ASSIGN(MainControllerAuthenticationServiceDelegate);
};

MainControllerAuthenticationServiceDelegate::
    MainControllerAuthenticationServiceDelegate(
        ChromeBrowserState* browser_state,
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

}  // namespace

@interface MainController () <PrefObserverDelegate, BlockingSceneCommands> {
  IBOutlet UIWindow* _window;

  // Weak; owned by the ChromeBrowserProvider.
  ios::ChromeBrowserStateManager* _browserStateManager;

  // The object that drives the Chrome startup/shutdown logic.
  std::unique_ptr<IOSChromeMain> _chromeMain;


  // True if the current session began from a cold start. False if the app has
  // entered the background at least once since start up.
  BOOL _isColdStart;

  // An object to record metrics related to the user's first action.
  std::unique_ptr<FirstUserActionRecorder> _firstUserActionRecorder;

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

  // Variable backing metricsMediator property.
  __weak MetricsMediator* _metricsMediator;

  WindowConfigurationRecorder* _windowConfigurationRecorder;

  // Hander for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;

  // List of closure to run as part of shutdown. The closure will be called
  // in reverse order of registration.
  std::vector<base::OnceClosure> _cleanupClosures;
}

// Handles collecting metrics on user triggered screenshots
@property(nonatomic, strong)
    ScreenshotMetricsRecorder* screenshotMetricsRecorder;

// Returns whether the restore infobar should be displayed.
- (bool)mustShowRestoreInfobar;
// Cleanup snapshots on disk.
- (void)cleanupSnapshots;
// Cleanup discarded sessions on disk.
- (void)cleanupDiscardedSessions;
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
// Starts logging breadcrumbs.
- (void)startLoggingBreadcrumbs;
// Asynchronously kick off regular free memory checks.
- (void)startFreeMemoryMonitoring;
// Asynchronously schedules the reset of the failed startup attempt counter.
- (void)scheduleStartupAttemptReset;
// Asynchronously schedules the cleanup of crash reports.
- (void)scheduleCrashReportCleanup;
// Asynchronously schedules the cleanup of discarded session files on disk.
- (void)scheduleDiscardedSessionsCleanup;
// Asynchronously schedules the cleanup of snapshots on disk.
- (void)scheduleSnapshotsCleanup;
// Schedules various cleanup tasks that are performed after launch.
- (void)scheduleStartupCleanupTasks;
// Schedules various tasks to be performed after the application becomes active.
- (void)scheduleLowPriorityStartupTasks;
// Schedules tasks that require a fully-functional BVC to be performed.
- (void)scheduleTasksRequiringBVCWithBrowserState;
// Schedules the deletion of user downloaded files that might be leftover
// from the last time Chrome was run.
- (void)scheduleDeleteTempDownloadsDirectory;
// Schedule the deletion of the temporary passwords files that might
// be left over from incomplete export operations.
- (void)scheduleDeleteTempPasswordsDirectory;
// Crashes the application if requested.
- (void)crashIfRequested;
// Performs synchronous browser state initialization steps.
- (void)initializeBrowserState:(ChromeBrowserState*)browserState;
// Initializes the application to the minimum initialization needed in all
// cases.
- (void)startUpBrowserBasicInitialization;
//  Initializes the browser objects for the background handlers.
- (void)startUpBrowserBackgroundInitialization;
// Initializes the browser objects for the browser UI (e.g., the browser
// state).
- (void)startUpBrowserForegroundInitialization;
// Register a closure to be called as part of app cleanup.
- (void)registerCleanupClosure:(base::OnceClosure)closure;
@end

@implementation MainController
// Defined by MainControllerGuts.
@synthesize restoreHelper = _restoreHelper;

// Defined by public protocols.
// - BrowserLauncher
@synthesize launchOptions = _launchOptions;
// - StartupInformation
@synthesize isColdStart = _isColdStart;
@synthesize appLaunchTime = _appLaunchTime;

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

- (void)startUpBrowserBasicInitialization {
  _appLaunchTime = IOSChromeMain::StartTime();
  _isColdStart = YES;

  [SetupDebugging setUpDebuggingOptions];

  // Register all providers before calling any Chromium code.
  [ProviderRegistration registerProviders];

  // Start dispatching for blocking UI commands.
  [self.appState.appCommandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BlockingSceneCommands)];
  [self.appState.appCommandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BrowsingDataCommands)];
}

- (void)startUpBrowserBackgroundInitialization {
  DCHECK(self.appState.initStage > InitStageSafeMode);

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

// This initialization must happen before any windows are created.
// Returns YES iff there's a session restore available.
- (BOOL)startUpBeforeFirstWindowCreatedAndPrepareForRestorationPostCrash:
    (BOOL)isPostCrashLaunch {
  // Give tests a chance to prepare for testing.
  tests_hook::SetUpTestsIfPresent();

  GetApplicationContext()->OnAppEnterForeground();

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
  ChromeBrowserState* chromeBrowserState =
      _browserStateManager->GetLastUsedBrowserState();

  // The CrashRestoreHelper must clean up the old browser state information.
  // |self.restoreHelper| must be kept alive until the BVC receives the
  // browser state.
  BOOL needRestoration = NO;
  if (isPostCrashLaunch) {
    NSSet<NSString*>* sessions = nil;
    if (@available(ios 13, *)) {
      sessions =
          [[PreviousSessionInfo sharedInstance] connectedSceneSessionsIDs];
    } else {
      sessions = [NSSet setWithObjects:SessionIdentifierForScene(nil), nil];
    }

    needRestoration = [CrashRestoreHelper moveAsideSessions:sessions
                                            forBrowserState:chromeBrowserState];
  }
  if (!base::ios::IsMultipleScenesSupported() &&
      base::ios::IsMultiwindowSupported()) {
    NSSet<NSString*>* previousSessions =
        [PreviousSessionInfo sharedInstance].connectedSceneSessionsIDs;
    DCHECK(previousSessions.count <= 1);
    self.appState.previousSingleWindowSessionID = [previousSessions anyObject];
  }
  [[PreviousSessionInfo sharedInstance] resetConnectedSceneSessionIDs];

  // Initialize and set the main browser state.
  [self initializeBrowserState:chromeBrowserState];
  self.appState.mainBrowserState = chromeBrowserState;

  if (base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs)) {
    [self startLoggingBreadcrumbs];
  }

  // Force an obvious initialization of the AuthenticationService. This must
  // be done before creation of the UI to ensure the service is initialised
  // before use (it is a security issue, so accessing the service CHECK if
  // this is not the case).
  DCHECK(self.appState.mainBrowserState);
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      self.appState.mainBrowserState,
      std::make_unique<MainControllerAuthenticationServiceDelegate>(
          self.appState.mainBrowserState, self));

  // Send "Chrome Opened" event to the feature_engagement::Tracker on cold
  // start.
  feature_engagement::TrackerFactory::GetForBrowserState(chromeBrowserState)
      ->NotifyEvent(feature_engagement::events::kChromeOpened);

  _spotlightManager = [SpotlightManager
      spotlightManagerWithBrowserState:self.appState.mainBrowserState];

  ShareExtensionService* service =
      ShareExtensionServiceFactory::GetForBrowserState(
          self.appState.mainBrowserState);
  service->Initialize();

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  if (IsCredentialProviderExtensionSupported()) {
    CredentialProviderServiceFactory::GetForBrowserState(
        self.appState.mainBrowserState);
  }
#endif

  _windowConfigurationRecorder = [[WindowConfigurationRecorder alloc] init];

  return needRestoration;
}

// This initialization must only happen once there's at least one Chrome window
// open.
- (void)startUpAfterFirstWindowCreated {
  // "Low priority" tasks
  [_startupTasks registerForApplicationWillResignActiveNotification];
  [self registerForOrientationChangeNotifications];

  _launchOptions = nil;

  [self scheduleTasksRequiringBVCWithBrowserState];

  CustomizeUIAppearance();

  [self scheduleStartupCleanupTasks];
  [MetricsMediator
      logLaunchMetricsWithStartupInformation:self
                             connectedScenes:self.appState.connectedScenes];

  ios::GetChromeBrowserProvider()->GetOverridesProvider()->InstallOverrides();

  [self scheduleLowPriorityStartupTasks];

  // Now that everything is properly set up, run the tests.
  tests_hook::RunTestsIfPresent();

  self.screenshotMetricsRecorder = [[ScreenshotMetricsRecorder alloc] init];
  [self.screenshotMetricsRecorder startRecordingMetrics];
}

- (void)startUpBrowserForegroundInitialization {
  self.appState.postCrashLaunch = [self mustShowRestoreInfobar];
  self.appState.sessionRestorationRequired =
      [self startUpBeforeFirstWindowCreatedAndPrepareForRestorationPostCrash:
                self.appState.postCrashLaunch];
}

- (void)registerCleanupClosure:(base::OnceClosure)closure {
  _cleanupClosures.push_back(std::move(closure));
}

- (void)initializeBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(!browserState->IsOffTheRecord());
  search_engines::UpdateSearchEnginesIfNeeded(
      browserState->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState));
}

#pragma mark - AppStateObserver

// Called when the first scene becomes active.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  DCHECK(self.appState.initStage > InitStageSafeMode);
  [self startUpAfterFirstWindowCreated];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  switch (appState.initStage) {
    case InitStageStart:
      [appState queueTransitionToNextInitStage];
      break;
    case InitStageBrowserBasic:
      [self startUpBrowserBasicInitialization];
      break;
    case InitStageSafeMode:
      [self addPostSafeModeAgents];
      break;
    case InitStageBrowserObjectsForBackgroundHandlers:
      [self startUpBrowserBackgroundInitialization];
      [appState queueTransitionToNextInitStage];
      break;
    case InitStageBrowserObjectsForUI:
      // When adding a new initialization flow, consider setting
      // |_appState.userInteracted| at the appropriate time.
      DCHECK(_appState.userInteracted);
      [self startUpBrowserForegroundInitialization];
      [appState queueTransitionToNextInitStage];
      break;
    case InitStageFirstRun:
      // TODO(crbug.com/1178821): Move this to the FRE agent.
      if (!ShouldPresentFirstRunExperience()) {
        [appState queueTransitionToNextInitStage];
      }
      break;
    case InitStageFinal:
      break;
  }
}

- (void)addPostSafeModeAgents {
  [self.appState addAgent:[[ContentSuggestionsSchedulerAppAgent alloc] init]];
  [self.appState addAgent:[[IncognitoUsageAppStateAgent alloc] init]];
}

#pragma mark - Property implementation.

- (void)setAppState:(AppState*)appState {
  DCHECK(!_appState);
  _appState = appState;
  [appState addObserver:self];

  // Create app state agents.
  [appState addAgent:[[AppMetricsAppStateAgent alloc] init]];
  [appState addAgent:[[SafeModeAppAgent alloc] init]];

  // Create the window accessibility agent only when multuple windows are
  // possible.
  if (base::ios::IsMultipleScenesSupported()) {
    [appState addAgent:[[WindowAccessibityChangeNotifierAppAgent alloc] init]];
  }
}

- (id<BrowserInterfaceProvider>)interfaceProvider {
  if (self.appState.foregroundActiveScene) {
    return self.appState.foregroundActiveScene.interfaceProvider;
  }
  NSArray<SceneState*>* connectedScenes = self.appState.connectedScenes;

  return connectedScenes.count == 0 ? nil
                                    : connectedScenes[0].interfaceProvider;
}

- (BOOL)isFirstLaunchAfterUpgrade {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

#pragma mark - StartupInformation implementation.

- (BOOL)isPresentingFirstRunUI {
  BOOL isPresentingFirstRunUI = NO;
  for (SceneState* scene in self.appState.connectedScenes) {
    isPresentingFirstRunUI |= scene.presentingFirstRunUI;
  }

  return isPresentingFirstRunUI;
}

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
  OmahaService::Stop();

  [_spotlightManager shutdown];
  _spotlightManager = nil;

  if (base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs)) {
    if (self.appState.mainBrowserState->HasOffTheRecordChromeBrowserState()) {
      breadcrumbs::BreadcrumbManagerKeyedService* service =
          BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
              self.appState.mainBrowserState
                  ->GetOffTheRecordChromeBrowserState());
      service->StopPersisting();
      breakpad::StopMonitoringBreadcrumbManagerService(service);
    }

    breadcrumbs::BreadcrumbManagerKeyedService* service =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            self.appState.mainBrowserState);
    service->StopPersisting();
    breakpad::StopMonitoringBreadcrumbManagerService(service);
  }

  _extensionSearchEngineDataUpdater = nullptr;

  if (!_cleanupClosures.empty()) {
    std::vector<base::OnceClosure> cleanupClosures;
    cleanupClosures.swap(_cleanupClosures);

    while (!cleanupClosures.empty()) {
      base::OnceClosure closure = std::move(cleanupClosures.back());
      cleanupClosures.pop_back();
      std::move(closure).Run();
    }

    DCHECK(_cleanupClosures.empty())
        << "-registerCleanupClosure must not be called during shutdown";
  }

  // _localStatePrefChangeRegistrar is observing the PrefService, which is owned
  // indirectly by _chromeMain (through the ChromeBrowserState).
  // Unregister the observer before the service is destroyed.
  _localStatePrefChangeRegistrar.RemoveAll();

  // Under the UIScene API, the scene delegate does not receive
  // sceneDidDisconnect: notifications on app termination. We mark remaining
  // connected scene states as diconnected in order to allow services to
  // properly unregister their observers and tear down remaining UI.
  for (SceneState* sceneState in self.appState.connectedScenes) {
    sceneState.activationLevel = SceneActivationLevelUnattached;
  }

  _chromeMain.reset();
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
  crash_keys::SetCurrentOrientation(GetInterfaceOrientation(),
                                    [[UIDevice currentDevice] orientation]);
}

- (void)registerForOrientationChangeNotifications {
  // Register device orientation. UI orientation will be registered by
  // each window BVC. These two events may be triggered independantely.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationDidChange:)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];
}

- (void)schedulePrefObserverInitialization {
  __weak MainController* weakSelf = self;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPrefObserverInit
                  block:^{
                    [weakSelf initializePrefObservers];
                  }];
}

- (void)initializePrefObservers {
  // Track changes to local state prefs.
  _localStatePrefChangeRegistrar.Init(GetApplicationContext()->GetLocalState());
  _localStatePrefObserverBridge = std::make_unique<PrefObserverBridge>(self);
  _localStatePrefObserverBridge->ObserveChangesForPreference(
      metrics::prefs::kMetricsReportingEnabled,
      &_localStatePrefChangeRegistrar);
  if (!base::FeatureList::IsEnabled(kUmaCellular)) {
    _localStatePrefObserverBridge->ObserveChangesForPreference(
        prefs::kMetricsReportingWifiOnly, &_localStatePrefChangeRegistrar);
  }

  // Calls the onPreferenceChanged function in case there was
  // a change to the observed preferences before the observer
  // bridge was set up.
  [self onPreferenceChanged:metrics::prefs::kMetricsReportingEnabled];
  [self onPreferenceChanged:prefs::kMetricsReportingWifiOnly];

  // Track changes to default search engine.
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.appState.mainBrowserState);
  _extensionSearchEngineDataUpdater =
      std::make_unique<ExtensionSearchEngineDataUpdater>(service);
}

- (void)scheduleAppDistributionPings {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSendInstallPingIfNecessary
                  block:^{
                    auto URLLoaderFactory = self.appState.mainBrowserState
                                                ->GetSharedURLLoaderFactory();
                    const bool is_first_run = FirstRun::IsChromeFirstRun();
                    ios::GetChromeBrowserProvider()
                        ->GetAppDistributionProvider()
                        ->ScheduleDistributionNotifications(URLLoaderFactory,
                                                            is_first_run);

                    const base::Time install_date = base::Time::FromTimeT(
                        GetApplicationContext()->GetLocalState()->GetInt64(
                            metrics::prefs::kInstallDate));

                    ios::GetChromeBrowserProvider()
                        ->GetAppDistributionProvider()
                        ->InitializeFirebase(install_date, is_first_run);
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
                    bool afterUpgrade = [self isFirstLaunchAfterUpgrade];
                    crash_helper::CleanupCrashReports(afterUpgrade);
                  }];
}

- (void)scheduleDiscardedSessionsCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCleanupDiscardedSessions
                  block:^{
                    [self cleanupDiscardedSessions];
                  }];
}

- (void)scheduleSnapshotsCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCleanupSnapshots
                  block:^{
                    [self cleanupSnapshots];
                  }];
}

- (void)scheduleSessionStateCacheCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPurgeWebSessionStates
                  block:^{
                    WebSessionStateCache* cache =
                        WebSessionStateCacheFactory::GetForBrowserState(
                            self.appState.mainBrowserState);
                    [cache purgeUnassociatedData];
                  }];
}

- (void)scheduleStartupCleanupTasks {
  [self scheduleCrashReportCleanup];

  // ClearSessionCookies() is not synchronous.
  if (cookie_util::ShouldClearSessionCookies()) {
    cookie_util::ClearSessionCookies(
        self.appState.mainBrowserState->GetOriginalChromeBrowserState());
    if (!(self.otrBrowser->GetWebStateList()->empty())) {
      cookie_util::ClearSessionCookies(
          self.appState.mainBrowserState->GetOffTheRecordChromeBrowserState());
    }
  }

  // Remove all discarded sessions from disk.
  [self scheduleDiscardedSessionsCleanup];

  // If the user chooses to restore their session, some cached snapshots and
  // session states may be needed. Otherwise, cleanup the snapshots and session
  // states
  if (![self mustShowRestoreInfobar]) {
    [self scheduleSnapshotsCleanup];
    [self scheduleSessionStateCacheCleanup];
  }
}

- (void)scheduleMemoryDebuggingTools {
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    __weak MainController* weakSelf = self;
    [[DeferredInitializationRunner sharedInstance]
        enqueueBlockNamed:kMemoryDebuggingToolsStartup
                    block:^{
                      [weakSelf initializedMemoryDebuggingTools];
                    }];
  }
}

- (void)initializedMemoryDebuggingTools {
  DCHECK(!_memoryDebuggerManager);
  DCHECK(experimental_flags::IsMemoryDebuggingEnabled());
  _memoryDebuggerManager = [[MemoryDebuggerManager alloc]
      initWithView:self.window
             prefs:GetApplicationContext()->GetLocalState()];
}

- (void)initializeMailtoHandling {
  __weak __typeof(self) weakSelf = self;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kMailtoHandlingInitialization
                  block:^{
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    if (!strongSelf || !strongSelf.appState.mainBrowserState) {
                      return;
                    }
                    ios::GetChromeBrowserProvider()
                        ->GetMailtoHandlerProvider()
                        ->PrepareMailtoHandling(
                            strongSelf.appState.mainBrowserState);

                    [strongSelf registerCleanupClosure:base::BindOnce([] {
                                  ios::GetChromeBrowserProvider()
                                      ->GetMailtoHandlerProvider()
                                      ->RemoveMailtoHandling();
                                })];
                  }];
}

// Schedule a call to |saveFieldTrialValuesForExtensions| for deferred
// execution.
- (void)scheduleSaveFieldTrialValuesForExtensions {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSaveFieldTrialValues
                  block:^{
                    [self saveFieldTrialValuesForExtensions];
                  }];
}

// Some extensions need the value of field trials but can't get them because the
// field trial infrastruction isn't in extensions. Save the necessary values to
// NSUserDefaults here.
- (void)saveFieldTrialValuesForExtensions {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  NSString* fieldTrialValueKey =
      base::SysUTF8ToNSString(app_group::kChromeExtensionFieldTrialPreference);

  // Add other field trial values here if they are needed by extensions.
  // The general format is
  // {
  //   name: {
  //     value: bool,
  //     version: bool
  //   }
  // }
  NSDictionary* fieldTrialValues = @{
  };
  [sharedDefaults setObject:fieldTrialValues forKey:fieldTrialValueKey];
}

// Schedules a call to |logIfEnterpriseManagedDevice| for deferred
// execution.
- (void)scheduleEnterpriseManagedDeviceCheck {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kEnterpriseManagedDeviceCheck
                  block:^{
                    [self logIfEnterpriseManagedDevice];
                  }];
}

- (void)logIfEnterpriseManagedDevice {
  NSString* managedKey = @"com.apple.configuration.managed";
  BOOL isManagedDevice = [[NSUserDefaults standardUserDefaults]
                             dictionaryForKey:managedKey] != nil;

  base::UmaHistogramBoolean("EnterpriseCheck.IsManaged2", isManagedDevice);
}

- (void)startFreeMemoryMonitoring {
  // No need for a post-task or a deferred initialisation as the memory
  // monitoring already happens on a background sequence.
  StartFreeMemoryMonitor();
}

- (void)startLoggingBreadcrumbs {
  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumbService =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          self.appState.mainBrowserState);
  breakpad::MonitorBreadcrumbManagerService(breadcrumbService);

  breadcrumbs::BreadcrumbPersistentStorageManager* persistentStorageManager =
      GetApplicationContext()->GetBreadcrumbPersistentStorageManager();

  // Application context can return a null persistent storage manager if
  // breadcrumbs are not being persisted.
  if (persistentStorageManager) {
    breadcrumbService->StartPersisting(persistentStorageManager);
  }

  // Get stored persistent breadcrumbs from last run to set on crash reports.
  persistentStorageManager->GetStoredEvents(
      base::BindOnce(^(std::vector<std::string> events) {
        breakpad::SetPreviousSessionEvents(events);
      }));
}

- (void)scheduleLowPriorityStartupTasks {
  [_startupTasks initializeOmaha];

  // Deferred tasks.
  [self schedulePrefObserverInitialization];
  [self scheduleMemoryDebuggingTools];
  [StartupTasks
      scheduleDeferredBrowserStateInitialization:self.appState
                                                     .mainBrowserState];
  [self sendQueuedFeedback];
  [self scheduleSpotlightResync];
  [self scheduleDeleteTempDownloadsDirectory];
  [self scheduleDeleteTempPasswordsDirectory];
  [self scheduleLogSiriShortcuts];
  [self scheduleStartupAttemptReset];
  [self startFreeMemoryMonitoring];
  [self scheduleAppDistributionPings];
  [self initializeMailtoHandling];
  [self scheduleSaveFieldTrialValuesForExtensions];
  [self scheduleEnterpriseManagedDeviceCheck];
}

- (void)scheduleTasksRequiringBVCWithBrowserState {
  if (GetApplicationContext()->WasLastShutdownClean()) {
    // Delay the cleanup of the unreferenced files to not impact startup
    // performance.
    ExternalFileRemoverFactory::GetForBrowserState(
        self.appState.mainBrowserState)
        ->RemoveAfterDelay(
            base::TimeDelta::FromSeconds(kExternalFilesCleanupDelaySeconds),
            base::OnceClosure());
  }
}

- (void)scheduleDeleteTempDownloadsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteDownloads
                  block:^{
                    DeleteTempDownloadsDirectory();
                  }];
}

- (void)scheduleDeleteTempPasswordsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteTempPasswords
                  block:^{
                    password_manager::DeletePasswordsDirectory();
                  }];
}

- (void)scheduleLogSiriShortcuts {
  __weak StartupTasks* startupTasks = _startupTasks;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kLogSiriShortcuts
                  block:^{
                    [startupTasks logSiriShortcuts];
                  }];
}

- (void)scheduleSpotlightResync {
  __weak SpotlightManager* spotlightManager = _spotlightManager;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartSpotlightBookmarksIndexing
                  block:^{
                    [spotlightManager resyncIndex];
                  }];
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

- (void)crashIfRequested {
  if (experimental_flags::IsStartupCrashEnabled()) {
    // Flush out the value cached for breakpad::SetUploadingEnabled().
    [[NSUserDefaults standardUserDefaults] synchronize];

    int* x = NULL;
    *x = 0;
  }
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

- (Browser*)mainBrowser {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.mainInterface.browser;
}

- (Browser*)otrBrowser {
  DCHECK(self.interfaceProvider);
  return self.interfaceProvider.incognitoInterface.browser;
}

- (Browser*)currentBrowser {
  return self.interfaceProvider.currentInterface.browser;
}

- (ChromeBrowserState*)currentBrowserState {
  if (!self.interfaceProvider.currentInterface.browser) {
    return nullptr;
  }
  return self.interfaceProvider.currentInterface.browser->GetBrowserState();
}

- (bool)mustShowRestoreInfobar {
  if ([self isFirstLaunchAfterUpgrade])
    return false;
  return !GetApplicationContext()->WasLastShutdownClean();
}

- (void)cleanupSnapshots {
  // TODO(crbug.com/1116496): Browsers for disconnected scenes are not in the
  // BrowserList, so this may not reach all folders.
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(self.appState.mainBrowserState);
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    SnapshotBrowserAgent::FromBrowser(browser)->PerformStorageMaintenance();
  }
  for (Browser* browser : browser_list->AllIncognitoBrowsers()) {
    SnapshotBrowserAgent::FromBrowser(browser)->PerformStorageMaintenance();
  }
}

- (void)cleanupDiscardedSessions {
  NSArray<NSString*>* sessionIDs =
      sessions_storage_util::GetDiscardedSessions();
  if (!sessionIDs)
    return;
  BrowsingDataRemoverFactory::GetForBrowserState(
      self.appState.mainBrowserState->GetOriginalChromeBrowserState())
      ->RemoveSessionsData(sessionIDs);
  BrowsingDataRemoverFactory::GetForBrowserState(
      self.appState.mainBrowserState->GetOffTheRecordChromeBrowserState())
      ->RemoveSessionsData(sessionIDs);
  sessions_storage_util::ResetDiscardedSessions();
}

#pragma mark - BrowsingDataCommands

- (void)removeBrowsingDataForBrowserState:(ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  // TODO(crbug.com/632772): https://bugs.webkit.org/show_bug.cgi?id=149079
  // makes it necessary to disable web usage while clearing browsing data.
  // It is however unnecessary for off-the-record BrowserState (as the code
  // is not invoked) and has undesired side-effect (cause all regular tabs
  // to reload, see http://crbug.com/821753 for details).
  BOOL disableWebUsageDuringRemoval =
      !browserState->IsOffTheRecord() &&
      IsRemoveDataMaskSet(removeMask, BrowsingDataRemoveMask::REMOVE_SITE_DATA);
  BOOL willShowActivityIndicator = NO;
  BOOL didShowActivityIndicator = NO;

  if (@available(iOS 13, *)) {
    // TODO(crbug.com/632772): Visited links clearing doesn't require disabling
    // web usage with iOS 13. Stop disabling web usage once iOS 12 is not
    // supported.
    willShowActivityIndicator = disableWebUsageDuringRemoval;
    disableWebUsageDuringRemoval = NO;
  }

  for (SceneState* sceneState in self.appState.connectedScenes) {
    // Assumes all scenes share |browserState|.
    id<BrowserInterfaceProvider> sceneInterface = sceneState.interfaceProvider;
    if (disableWebUsageDuringRemoval) {
      // Disables browsing and purges web views.
      // Must be called only on the main thread.
      DCHECK([NSThread isMainThread]);
      sceneInterface.mainInterface.userInteractionEnabled = NO;
      sceneInterface.incognitoInterface.userInteractionEnabled = NO;
    } else if (willShowActivityIndicator) {
      // Show activity overlay so users know that clear browsing data is in
      // progress.
      // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
      // clean up.
      if (sceneInterface.mainInterface.browser) {
        didShowActivityIndicator = YES;
        id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
            sceneInterface.mainInterface.browser->GetCommandDispatcher());
        [handler showActivityOverlay:YES];
      }
    }
  }

  auto removalCompletion = ^{
    // Activates browsing and enables web views.
    // Must be called only on the main thread.
    DCHECK([NSThread isMainThread]);
    for (SceneState* sceneState in self.appState.connectedScenes) {
      // Assumes all scenes share |browserState|.
      id<BrowserInterfaceProvider> sceneInterface =
          sceneState.interfaceProvider;

      if (willShowActivityIndicator) {
        // User interaction still needs to be disabled as a way to
        // force reload all the web states and to reset NTPs.
        sceneInterface.mainInterface.userInteractionEnabled = NO;
        sceneInterface.incognitoInterface.userInteractionEnabled = NO;

        // TODO(crbug.com/1045047): Use HandlerForProtocol after commands
        // protocol clean up.
        if (didShowActivityIndicator && sceneInterface.mainInterface.browser) {
          id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
              sceneInterface.mainInterface.browser->GetCommandDispatcher());
          [handler showActivityOverlay:NO];
        }
      }
      sceneInterface.mainInterface.userInteractionEnabled = YES;
      sceneInterface.incognitoInterface.userInteractionEnabled = YES;
      [sceneInterface.currentInterface setPrimary:YES];
    }
    // |completionBlock is run once, not once per scene.
    if (completionBlock)
      completionBlock();
  };

  BrowsingDataRemoverFactory::GetForBrowserState(browserState)
      ->Remove(timePeriod, removeMask, base::BindOnce(removalCompletion));
}

#pragma mark - BlockingSceneCommands

- (void)activateBlockingScene:(UIScene*)requestingScene
    API_AVAILABLE(ios(13.0)) {
  id<UIBlockerTarget> uiBlocker = self.appState.currentUIBlocker;
  if (!uiBlocker) {
    return;
  }

  [uiBlocker bringBlockerToFront:requestingScene];
}

@end

#pragma mark - TestingOnly

@implementation MainController (TestingOnly)

- (void)setStartupParametersWithURL:(const GURL&)launchURL {
  NSString* sourceApplication = @"Fake App";
  SceneState* sceneState = self.appState.foregroundActiveScene;
  sceneState.controller.startupParameters = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:net::NSURLWithGURL(launchURL)
                     fromSourceApplication:sourceApplication];
}

@end
