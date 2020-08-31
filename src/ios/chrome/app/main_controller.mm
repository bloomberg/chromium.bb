// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"

#include <memory>
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crl_set_remover.h"
#include "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/ukm/ios/features.h"
#include "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/memory_monitor.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#include "ios/chrome/app/startup/chrome_main_starter.h"
#include "ios/chrome/app/startup/client_registration.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/app/startup/provider_registration.h"
#include "ios/chrome/app/startup/register_experimental_settings.h"
#include "ios/chrome/app/startup/setup_debugging.h"
#import "ios/chrome/app/startup_tasks.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_removal_controller.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/external_files/external_file_remover_factory.h"
#import "ios/chrome/browser/external_files/external_file_remover_impl.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/net/cookie_util.h"
#import "ios/chrome/browser/ntp_snippets/content_suggestions_scheduler_notifications.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#import "ios/chrome/browser/omaha/omaha_service.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/search_engines/extension_search_engine_data_updater.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/share_extension/share_extension_service.h"
#import "ios/chrome/browser/share_extension/share_extension_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_delegate.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/scene_controller_guts.h"
#import "ios/chrome/browser/ui/main/scene_delegate.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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

// Constants for deferring the enterprise managed device check.
NSString* const kEnterpriseManagedDeviceCheck = @"EnterpriseManagedDeviceCheck";

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

@interface MainController () <PrefObserverDelegate> {
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

  // Hander for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;

  // If the animations were disabled.
  BOOL _animationDisabled;
}

// The ChromeBrowserState associated with the main (non-OTR) browsing mode.
@property(nonatomic, assign) ChromeBrowserState* mainBrowserState;  // Weak.

// Returns whether the restore infobar should be displayed.
- (bool)mustShowRestoreInfobar;
// Returns the set of the sessions ids of the tabs in the given |webStateList|.
- (NSMutableSet*)liveSessionsForWebStateList:(WebStateList*)webStateList;
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
- (void)scheduleDeleteTempDownloadsDirectory;
// Schedule the deletion of the temporary passwords files that might
// be left over from incomplete export operations.
- (void)scheduleDeleteTempPasswordsDirectory;
// Crashes the application if requested.
- (void)crashIfRequested;
// Handles the notification that first run modal dialog UI is about to complete.
- (void)handleFirstRunUIWillFinish;
// Handles the notification that first run modal dialog UI completed.
- (void)handleFirstRunUIDidFinish;
// Performs synchronous browser state initialization steps.
- (void)initializeBrowserState:(ChromeBrowserState*)browserState;
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
// Defined by MainControllerGuts.
@synthesize restoreHelper = _restoreHelper;

// Defined by public protocols.
// - BrowserLauncher
@synthesize launchOptions = _launchOptions;
@synthesize browserInitializationStage = _browserInitializationStage;
// - StartupInformation
@synthesize isColdStart = _isColdStart;
@synthesize startupParameters = _startupParameters;
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
  _appLaunchTime = IOSChromeMain::StartTime();
  _isColdStart = YES;

  [SetupDebugging setUpDebuggingOptions];

  // Register all providers before calling any Chromium code.
  [ProviderRegistration registerProviders];

  if (@available(iOS 13, *)) {
    if (IsSceneStartupSupported()) {
      // Subscribe for scene connection and disconnection notifications.
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(sceneWillConnect:)
                 name:UISceneWillConnectNotification
               object:nil];
    }
  }
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
    needRestoration = [CrashRestoreHelper
        moveAsideSessionInformationForBrowserState:chromeBrowserState];
  }

  // Initialize and set the main browser state.
  [self initializeBrowserState:chromeBrowserState];
  self.mainBrowserState = chromeBrowserState;

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    breakpad::MonitorBreadcrumbManagerService(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            self.mainBrowserState));
  }

  // Force an obvious initialization of the AuthenticationService. This must
  // be done before creation of the UI to ensure the service is initialised
  // before use (it is a security issue, so accessing the service CHECK if
  // this is not the case).
  DCHECK(self.mainBrowserState);
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      self.mainBrowserState,
      std::make_unique<MainControllerAuthenticationServiceDelegate>(
          self.mainBrowserState, self));

  // Send "Chrome Opened" event to the feature_engagement::Tracker on cold
  // start.
  feature_engagement::TrackerFactory::GetForBrowserState(chromeBrowserState)
      ->NotifyEvent(feature_engagement::events::kChromeOpened);

  _spotlightManager =
      [SpotlightManager spotlightManagerWithBrowserState:self.mainBrowserState];

  ShareExtensionService* service =
      ShareExtensionServiceFactory::GetForBrowserState(self.mainBrowserState);
  service->Initialize();

  if ([PreviousSessionInfo sharedInstance].isFirstSessionAfterLanguageChange) {
    IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
        chromeBrowserState)
        ->ClearAllCachedSuggestions();
  }

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
  if (self.isColdStart) {
    [ContentSuggestionsSchedulerNotifications
        notifyColdStart:self.mainBrowserState];
    [ContentSuggestionsSchedulerNotifications
        notifyForeground:self.mainBrowserState];
  }

  ios::GetChromeBrowserProvider()->GetOverridesProvider()->InstallOverrides();

  [self scheduleLowPriorityStartupTasks];

  // Now that everything is properly set up, run the tests.
  tests_hook::RunTestsIfPresent();
}

- (void)startUpBrowserForegroundInitialization {
  BOOL postCrashLaunch = [self mustShowRestoreInfobar];
  BOOL needRestore =
      [self startUpBeforeFirstWindowCreatedAndPrepareForRestorationPostCrash:
                postCrashLaunch];

  if (@available(iOS 13, *)) {
    if (IsSceneStartupSupported()) {
      // The rest of the startup sequence is handled by the Scenes and in
      // response to notifications.
      return;
    }
  }

  [self.sceneController startUpChromeUIPostCrash:postCrashLaunch
                                 needRestoration:needRestore];
  [self startUpAfterFirstWindowCreated];
}

- (void)initializeBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(!browserState->IsOffTheRecord());
  search_engines::UpdateSearchEnginesIfNeeded(
      browserState->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState));
}

- (void)handleFirstRunUIWillFinish {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIWillFinishNotification
              object:nil];

  [self markEulaAsAccepted];

  if (self.startupParameters) {
    UrlLoadParams params =
        UrlLoadParams::InNewTab(self.startupParameters.externalURL);
    [self.sceneController
        dismissModalsAndOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                  NORMAL
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

#pragma mark - AppStateObserver

// Called when the first scene becomes active.
- (void)appState:(AppState*)appState
    firstSceneActivated:(SceneState*)sceneState {
  if (appState.isInSafeMode) {
    return;
  }
  [self startUpAfterFirstWindowCreated];
}

- (void)appStateDidExitSafeMode:(AppState*)appState {
  [self startUpAfterFirstWindowCreated];
}

#pragma mark - Scene notifications

// Handler for UISceneWillConnectNotification.
- (void)sceneWillConnect:(NSNotification*)notification {
  DCHECK(IsSceneStartupSupported());
  if (@available(iOS 13, *)) {
    UIWindowScene* scene =
        base::mac::ObjCCastStrict<UIWindowScene>(notification.object);
    SceneDelegate* sceneDelegate =
        base::mac::ObjCCastStrict<SceneDelegate>(scene.delegate);
    self.sceneController = sceneDelegate.sceneController;
    sceneDelegate.sceneController.mainController = self;
  }
}

#pragma mark - Property implementation.

- (id<BrowserInterfaceProvider>)interfaceProvider {
  return self.appState.connectedScenes[0].interfaceProvider;
}

- (BOOL)isFirstLaunchAfterUpgrade {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

#pragma mark - StartupInformation implementation.

- (BOOL)isPresentingFirstRunUI {
  BOOL isPresentingFirstRunUI = NO;
  for (SceneState* scene in self.appState.connectedScenes) {
    isPresentingFirstRunUI &= scene.presentingFirstRunUI;
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
  // This code is per-window.

  // Teardown UI state that is associated with scenes.
  [self.sceneController teardownUI];
  // End of per-window code.

  OmahaService::Stop();

  [_spotlightManager shutdown];
  _spotlightManager = nil;

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    if (self.mainBrowserState->HasOffTheRecordChromeBrowserState()) {
      breakpad::StopMonitoringBreadcrumbManagerService(
          BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
              self.mainBrowserState->GetOffTheRecordChromeBrowserState()));
    }
    breakpad::StopMonitoringBreadcrumbManagerService(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            self.mainBrowserState));
  }

  _extensionSearchEngineDataUpdater = nullptr;

  ios::GetChromeBrowserProvider()
      ->GetMailtoHandlerProvider()
      ->RemoveMailtoHandling();
  // _localStatePrefChangeRegistrar is observing the PrefService, which is owned
  // indirectly by _chromeMain (through the ChromeBrowserState).
  // Unregister the observer before the service is destroyed.
  _localStatePrefChangeRegistrar.RemoveAll();

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
  crash_keys::SetCurrentOrientation(
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

- (void)registerBatteryMonitoringNotifications {
  if (base::FeatureList::IsEnabled(kDisableAnimationOnLowBattery)) {
    [[UIDevice currentDevice] setBatteryMonitoringEnabled:YES];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(batteryLevelDidChange:)
               name:UIDeviceBatteryLevelDidChangeNotification
             object:nil];
    [self batteryLevelDidChange:nil];
  }
}

- (void)batteryLevelDidChange:(NSNotification*)notification {
  if (![[UIDevice currentDevice] isBatteryMonitoringEnabled]) {
    return;
  }
  CGFloat level = [UIDevice currentDevice].batteryLevel;
  if (level < web::features::kLowBatteryLevelThreshold) {
    if (!_animationDisabled) {
      _animationDisabled = YES;
      [UIView setAnimationsEnabled:NO];
    }
  } else if (_animationDisabled) {
    _animationDisabled = NO;
    [UIView setAnimationsEnabled:YES];
  }
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
                            self.mainBrowserState);
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
                        self.mainBrowserState->GetSharedURLLoaderFactory();
                    const bool is_first_run = FirstRun::IsChromeFirstRun();
                    ios::GetChromeBrowserProvider()
                        ->GetAppDistributionProvider()
                        ->ScheduleDistributionNotifications(URLLoaderFactory,
                                                            is_first_run);

                    const int64_t install_date =
                        GetApplicationContext()->GetLocalState()->GetInt64(
                            metrics::prefs::kInstallDate);
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
        self.mainBrowserState->GetOriginalChromeBrowserState());
    if (!(self.otrBrowser->GetWebStateList()->empty())) {
      cookie_util::ClearSessionCookies(
          self.mainBrowserState->GetOffTheRecordChromeBrowserState());
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
                    if (!strongSelf || !strongSelf.mainBrowserState) {
                      return;
                    }
                    ios::GetChromeBrowserProvider()
                        ->GetMailtoHandlerProvider()
                        ->PrepareMailtoHandling(strongSelf.mainBrowserState);
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

  base::UmaHistogramBoolean("EnterpriseCheck.IsManaged", isManagedDevice);
}

- (void)startFreeMemoryMonitoring {
  // No need for a post-task or a deferred initialisation as the memory
  // monitoring already happens on a background sequence.
  StartFreeMemoryMonitor();
}

- (void)scheduleLowPriorityStartupTasks {
  [_startupTasks initializeOmaha];
  [_startupTasks donateIntents];

  [self registerBatteryMonitoringNotifications];

  // Deferred tasks.
  [self schedulePrefObserverInitialization];
  [self scheduleMemoryDebuggingTools];
  [StartupTasks
      scheduleDeferredBrowserStateInitialization:self.mainBrowserState];
  [self sendQueuedFeedback];
  [self scheduleSpotlightResync];
  [self scheduleDeleteTempDownloadsDirectory];
  [self scheduleDeleteTempPasswordsDirectory];
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
    ExternalFileRemoverFactory::GetForBrowserState(self.mainBrowserState)
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
  return !(self.otrBrowser->GetWebStateList()->empty());
}

- (void)prepareForFirstRunUI {
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

- (NSMutableSet*)liveSessionsForWebStateList:(WebStateList*)webStateList {
  NSMutableSet* result = [NSMutableSet setWithCapacity:webStateList->count()];
  for (int index = 0; index < webStateList->count(); ++index) {
    web::WebState* webState = webStateList->GetWebStateAt(index);
    [result addObject:TabIdTabHelper::FromWebState(webState)->tab_id()];
  }
  return result;
}

- (void)purgeSnapshots {
  NSMutableSet* liveSessions =
      [self liveSessionsForWebStateList:self.mainBrowser->GetWebStateList()];
  [liveSessions
      unionSet:[self liveSessionsForWebStateList:self.otrBrowser
                                                     ->GetWebStateList()]];

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
  BOOL showActivityIndicator = NO;

  if (@available(iOS 13, *)) {
    // TODO(crbug.com/632772): Visited links clearing doesn't require disabling
    // web usage with iOS 13. Stop disabling web usage once iOS 12 is not
    // supported.
    showActivityIndicator = disableWebUsageDuringRemoval;
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
    } else if (showActivityIndicator) {
      // Show activity overlay so users know that clear browsing data is in
      // progress.
      // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
      // clean up.
      id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
          sceneInterface.mainInterface.browser->GetCommandDispatcher());
      [handler showActivityOverlay:YES];
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

      if (showActivityIndicator) {
        // User interaction still needs to be disabled as a way to
        // force reload all the web states and to reset NTPs.
        sceneInterface.mainInterface.userInteractionEnabled = NO;
        sceneInterface.incognitoInterface.userInteractionEnabled = NO;

        // TODO(crbug.com/1045047): Use HandlerForProtocol after commands
        // protocol clean up.
        id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
            sceneInterface.mainInterface.browser->GetCommandDispatcher());
        [handler showActivityOverlay:NO];
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


@end

#pragma mark - TestingOnly

@implementation MainController (TestingOnly)

- (void)setStartupParametersWithURL:(const GURL&)launchURL {
  NSString* sourceApplication = @"Fake App";
  self.startupParameters = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:net::NSURLWithGURL(launchURL)
                     fromSourceApplication:sourceApplication];
}

@end
