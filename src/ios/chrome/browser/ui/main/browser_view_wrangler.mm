// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#import "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_browser_agent.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Internal implementation of BrowserInterface -- for the most part a wrapper
// around BrowserCoordinator.
@interface WrangledBrowser : NSObject <BrowserInterface>

@property(nonatomic, weak, readonly) BrowserCoordinator* coordinator;

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator;

@end

@implementation WrangledBrowser

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator {
  if (self = [super init]) {
    DCHECK(coordinator.browser);
    _coordinator = coordinator;
  }
  return self;
}

- (UIViewController*)viewController {
  return self.coordinator.viewController;
}

- (BrowserViewController*)bvc {
  return self.coordinator.viewController;
}

- (Browser*)browser {
  return self.coordinator.browser;
}

- (ChromeBrowserState*)browserState {
  return self.browser->GetBrowserState();
}

- (BOOL)userInteractionEnabled {
  return self.coordinator.active;
}

- (void)setUserInteractionEnabled:(BOOL)userInteractionEnabled {
  self.coordinator.active = userInteractionEnabled;
}

- (BOOL)incognito {
  return self.browserState->IsOffTheRecord();
}

- (void)setPrimary:(BOOL)primary {
  [self.coordinator.viewController setPrimary:primary];
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.coordinator clearPresentedStateWithCompletion:completion
                                       dismissOmnibox:dismissOmnibox];
}

@end

@interface BrowserViewWrangler () {
  ChromeBrowserState* _browserState;
  SceneState* _sceneState;
  __weak id<ApplicationCommands> _applicationCommandEndpoint;
  __weak id<BrowsingDataCommands> _browsingDataCommandEndpoint;
  BOOL _isShutdown;

  std::unique_ptr<Browser> _mainBrowser;
  std::unique_ptr<Browser> _otrBrowser;
}

@property(nonatomic, strong, readwrite) WrangledBrowser* mainInterface;
@property(nonatomic, strong, readwrite) WrangledBrowser* incognitoInterface;

// Backing objects.
@property(nonatomic) BrowserCoordinator* mainBrowserCoordinator;
@property(nonatomic) BrowserCoordinator* incognitoBrowserCoordinator;
@property(nonatomic, readonly) Browser* mainBrowser;
@property(nonatomic, readonly) Browser* otrBrowser;

// The main browser can't be set after creation, but they can be
// cleared (setting them to nullptr).
- (void)clearMainBrowser;
// The OTR browser can be reset after creation.
- (void)setOtrBrowser:(std::unique_ptr<Browser>)browser;

// Creates and sets up a new Browser for the given BrowserState, optionally
// loading the session from disk.
- (std::unique_ptr<Browser>)buildBrowserForBrowserState:
                                (ChromeBrowserState*)browserState
                                         restoreSession:(BOOL)restoreSession;

// Creates the correct BrowserCoordinator for the corresponding browser state
// and Browser.
- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser;
@end

@implementation BrowserViewWrangler

@synthesize currentInterface = _currentInterface;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                          sceneState:(SceneState*)sceneState
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
         browsingDataCommandEndpoint:
             (id<BrowsingDataCommands>)browsingDataCommandEndpoint {
  if ((self = [super init])) {
    _browserState = browserState;
    _sceneState = sceneState;
    _applicationCommandEndpoint = applicationCommandEndpoint;
    _browsingDataCommandEndpoint = browsingDataCommandEndpoint;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before -dealloc";
}

- (void)createMainBrowser {
  _mainBrowser = [self buildBrowserForBrowserState:_browserState
                                    restoreSession:YES];

  // Create the main coordinator, and thus the main interface.
  _mainBrowserCoordinator = [self coordinatorForBrowser:self.mainBrowser];
  [_mainBrowserCoordinator start];
  DCHECK(_mainBrowserCoordinator.viewController);
  _mainInterface =
      [[WrangledBrowser alloc] initWithCoordinator:_mainBrowserCoordinator];
}

#pragma mark - BrowserViewInformation property implementations

- (void)setCurrentInterface:(WrangledBrowser*)interface {
  DCHECK(interface);
  // |interface| must be one of the interfaces this class already owns.
  DCHECK(self.mainInterface == interface ||
         self.incognitoInterface == interface);
  if (self.currentInterface == interface) {
    return;
  }

  if (self.currentInterface) {
    // Tell the current BVC it moved to the background.
    [self.currentInterface setPrimary:NO];
  }

  _currentInterface = interface;

  // Update the shared active URL for the new interface.
  DeviceSharingBrowserAgent::FromBrowser(_currentInterface.browser)
      ->UpdateForActiveBrowser();
}

- (id<BrowserInterface>)incognitoInterface {
  if (!_mainInterface)
    return nil;
  if (!_incognitoInterface) {
    // The backing coordinator should not have been created yet.
    DCHECK(!_incognitoBrowserCoordinator);
    ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    DCHECK(otrBrowserState);
    _incognitoBrowserCoordinator = [self coordinatorForBrowser:self.otrBrowser];
    [_incognitoBrowserCoordinator start];
    DCHECK(_incognitoBrowserCoordinator.viewController);
    _incognitoInterface = [[WrangledBrowser alloc]
        initWithCoordinator:_incognitoBrowserCoordinator];
  }
  return _incognitoInterface;
}

- (BOOL)hasIncognitoInterface {
  return _incognitoInterface;
}

- (Browser*)mainBrowser {
  DCHECK(_mainBrowser.get())
      << "-createMainBrowser must be called before -mainBrowser is accessed.";
  return _mainBrowser.get();
}

- (Browser*)otrBrowser {
  if (!_otrBrowser) {
    // Ensure the incognito BrowserState is created.
    DCHECK(_browserState);
    ChromeBrowserState* incognitoBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    _otrBrowser = [self buildBrowserForBrowserState:incognitoBrowserState
                                     restoreSession:YES];
  }
  return _otrBrowser.get();
}

- (void)clearMainBrowser {
  if (_mainBrowser.get()) {
    WebStateList* webStateList = self.mainBrowser->GetWebStateList();
    breakpad::StopMonitoringTabStateForWebStateList(webStateList);
    breakpad::StopMonitoringURLsForWebStateList(webStateList);
    [self.mainBrowser->GetTabModel() disconnect];
  }

  _mainBrowser = nullptr;
}

- (void)setOtrBrowser:(std::unique_ptr<Browser>)otrBrowser {
  if (_otrBrowser.get()) {
    WebStateList* webStateList = self.otrBrowser->GetWebStateList();
    breakpad::StopMonitoringTabStateForWebStateList(webStateList);
    [self.otrBrowser->GetTabModel() disconnect];
  }

  _otrBrowser = std::move(otrBrowser);
}

#pragma mark - Other public methods

- (void)willDestroyIncognitoBrowserState {
  // It is theoretically possible that a Tab has been added to the webStateList
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(self.otrBrowser->GetWebStateList()->empty());
  DCHECK(_browserState);

  // Remove the OTR browser from the browser list. The browser itself is
  // still alive during this call, so any observers can act on it.
  BrowserList* browserList = BrowserListFactory::GetForBrowserState(
      self.otrBrowser->GetBrowserState());
  browserList->RemoveIncognitoBrowser(self.otrBrowser);

  // Stop watching the OTR webStateList's state for crashes.
  breakpad::StopMonitoringTabStateForWebStateList(
      self.otrBrowser->GetWebStateList());

  // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
  // constructed by calling the property getter.
  BOOL otrBVCIsCurrent = self.currentInterface == self.incognitoInterface;
  @autoreleasepool {
    // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
    // constructed by calling the property getter.
    [_incognitoBrowserCoordinator stop];
    _incognitoBrowserCoordinator = nil;
    _incognitoInterface = nil;

    // There's no guarantee the tab model was ever added to the BVC (or even
    // that the BVC was created), so ensure the tab model gets notified.
    [self setOtrBrowser:nullptr];
    if (otrBVCIsCurrent) {
      _currentInterface = nil;
    }
  }
}

- (void)incognitoBrowserStateCreated {
  DCHECK(_browserState);
  DCHECK(_browserState->HasOffTheRecordChromeBrowserState());

  // An empty _otrBrowser must be created at this point, because it is then
  // possible to prevent the tabChanged notification being sent. Otherwise,
  // when it is created, a notification with no tabs will be sent, and it will
  // be immediately deleted.
  ChromeBrowserState* incognitoBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();

  [self setOtrBrowser:[self buildBrowserForBrowserState:incognitoBrowserState
                                         restoreSession:NO]];
  DCHECK(self.otrBrowser->GetWebStateList()->empty());

  if (_currentInterface == nil) {
    self.currentInterface = self.incognitoInterface;
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  [self.mainBrowser->GetCommandDispatcher() prepareForShutdown];
  if ([self hasIncognitoInterface]) {
    [self.otrBrowser->GetCommandDispatcher() prepareForShutdown];
  }

  // At this stage, new BrowserCoordinators shouldn't be lazily constructed by
  // calling their property getters.
  [_mainBrowserCoordinator stop];
  _mainBrowserCoordinator = nil;
  [_incognitoBrowserCoordinator stop];
  _incognitoBrowserCoordinator = nil;

  BrowserList* browserList = BrowserListFactory::GetForBrowserState(
      self.mainBrowser->GetBrowserState());
  browserList->RemoveBrowser(self.mainBrowser);
  BrowserList* otrBrowserList = BrowserListFactory::GetForBrowserState(
      self.otrBrowser->GetBrowserState());
  otrBrowserList->RemoveIncognitoBrowser(self.otrBrowser);

  // Handles removing observers, stopping breakpad monitoring, and closing all
  // tabs.
  [self clearMainBrowser];
  [self setOtrBrowser:nullptr];

  _browserState = nullptr;
}

#pragma mark - Internal methods

- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser {
  BrowserCoordinator* coordinator =
      [[BrowserCoordinator alloc] initWithBaseViewController:nil
                                                     browser:browser];
  return coordinator;
}

- (void)dispatchToEndpointsForBrowser:(Browser*)browser {
  IncognitoReauthSceneAgent* reauthAgent =
      [IncognitoReauthSceneAgent agentFromScene:_sceneState];

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:reauthAgent
                           forProtocol:@protocol(IncognitoReauthCommands)];

  [dispatcher startDispatchingToTarget:_applicationCommandEndpoint
                           forProtocol:@protocol(ApplicationCommands)];

  // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
  // passed protocol conforms to, so ApplicationSettingsCommands is explicitly
  // dispatched to the endpoint as well. Since this is potentially
  // fragile, DCHECK that it should still work (if the endpoint is non-nil).
  DCHECK(!_applicationCommandEndpoint ||
         [_applicationCommandEndpoint
             conformsToProtocol:@protocol(ApplicationSettingsCommands)]);
  [dispatcher startDispatchingToTarget:_applicationCommandEndpoint
                           forProtocol:@protocol(ApplicationSettingsCommands)];
  [dispatcher startDispatchingToTarget:_browsingDataCommandEndpoint
                           forProtocol:@protocol(BrowsingDataCommands)];
}

- (std::unique_ptr<Browser>)buildBrowserForBrowserState:
                                (ChromeBrowserState*)browserState
                                         restoreSession:(BOOL)restoreSession {
  auto browser = Browser::Create(browserState);
  DCHECK_EQ(browser->GetBrowserState(), browserState);

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);
  if (browserState->IsOffTheRecord()) {
    browserList->AddIncognitoBrowser(browser.get());
  } else {
    browserList->AddBrowser(browser.get());
  }

  // Associate the current SceneState with the new browser.
  SceneStateBrowserAgent::CreateForBrowser(browser.get(), _sceneState);

  [self dispatchToEndpointsForBrowser:browser.get()];

  [self setSessionIDForBrowser:browser.get() restoreSession:restoreSession];

  breakpad::MonitorTabStateForWebStateList(browser->GetWebStateList());

  // Follow loaded URLs in the non-incognito browser to send those in case of
  // crashes.
  if (!browserState->IsOffTheRecord()) {
    breakpad::MonitorURLsForWebStateList(browser->GetWebStateList());
  }

  return browser;
}

- (void)setSessionIDForBrowser:(Browser*)browser
                restoreSession:(BOOL)restoreSession {
  SnapshotBrowserAgent::FromBrowser(browser)->SetSessionID(
      base::SysNSStringToUTF8(_sceneState.sceneSessionID));

  SessionRestorationBrowserAgent* restorationAgent =
      SessionRestorationBrowserAgent::FromBrowser(browser);

  // crbug.com/1153606: when the app is distributed with multi-window enabled,
  // the "swipe gesture" is sometimes interpreted as a "close window" gesture
  // by iOS (reproduce easily if the user launch app, swipe it away in a loop).
  // On the next start after iOS has decided the gesture is "close window", the
  // session identifier will be reset to a different value.
  //
  // For device that support multiple windows (recent iPads running iOS 13+),
  // the user has the option to restore recently closed windows (presented by
  // iOS when user ask to see all windows). For other devices however they have
  // no option to re-open the closed window and lose their data.
  //
  // To workaround this behaviour, the session identifier is saved, and on each
  // run, if the device does not support multi-window, compared to the current
  // session identifier. If there is a mismatch, load the session using the old
  // identifier (which is used to construct path to Chrome's session data), and
  // immediately save it with the new identifier.
  //
  // TODO(crbug.com/1165798): clean up this by using fixed identifier when the
  // device do no support multi-windows.
  if (!base::ios::IsMultipleScenesSupported() && restoreSession) {
    NSString* previousSessionID =
        _sceneState.appState.previousSingleWindowSessionID;
    if (previousSessionID &&
        ![_sceneState.sceneSessionID isEqualToString:previousSessionID]) {
      restorationAgent->SetSessionID(
          base::SysNSStringToUTF8(previousSessionID));
      restorationAgent->RestoreSession();

      restorationAgent->SetSessionID(
          base::SysNSStringToUTF8(_sceneState.sceneSessionID));
      restorationAgent->SaveSession(true);

      // Fallback to the normal codepath. It will set the session identifier
      // in the SessionRestorationBrowserAgent. Since the session has been
      // loaded already, skip this step by setting |restoreSession| to NO.
      restoreSession = NO;
    }
  }

  restorationAgent->SetSessionID(
      base::SysNSStringToUTF8(_sceneState.sceneSessionID));
  if (restoreSession)
    restorationAgent->RestoreSession();
}

@end
