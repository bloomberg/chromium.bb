// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"

#include <memory>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/ios/block_types.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_ui::CompletionCallback;

namespace {

const int64_t kAuthenticationFlowTimeoutSeconds = 10;

}  // namespace

@interface AuthenticationFlowPerformer ()<ImportDataControllerDelegate,
                                          SettingsNavigationControllerDelegate>

// Starts the watchdog timer with a timeout of
// |kAuthenticationFlowTimeoutSeconds| for the fetching managed status
// operation. It will notify |_delegate| of the failure unless
// |stopWatchdogTimer| is called before it times out.
- (void)startWatchdogTimerForManagedStatus;

// Stops the watchdog timer, and doesn't call the |timeoutDelegateSelector|.
// Returns whether the watchdog was actually running.
- (BOOL)stopWatchdogTimer;

// Callback for when the alert is dismissed.
- (void)alertControllerDidDisappear:(AlertCoordinator*)alertCoordinator;

@end

@implementation AuthenticationFlowPerformer {
  __weak id<AuthenticationFlowPerformerDelegate> _delegate;
  AlertCoordinator* _alertCoordinator;
  SettingsNavigationController* _navigationController;
  std::unique_ptr<base::OneShotTimer> _watchdogTimer;
}

- (id<AuthenticationFlowPerformerDelegate>)delegate {
  return _delegate;
}

- (instancetype)initWithDelegate:
    (id<AuthenticationFlowPerformerDelegate>)delegate {
  self = [super init];
  if (self)
    _delegate = delegate;
  return self;
}

- (void)cancelAndDismiss {
  [_alertCoordinator executeCancelHandler];
  [_alertCoordinator stop];
  if (_navigationController) {
    [_navigationController cleanUpSettings];
    _navigationController = nil;
    [_delegate dismissPresentingViewControllerAnimated:NO completion:nil];
  }
  [self stopWatchdogTimer];
}

- (void)commitSyncForBrowserState:(ChromeBrowserState*)browserState {
  SyncSetupServiceFactory::GetForBrowserState(browserState)
      ->CommitSyncChanges();
}

- (void)startWatchdogTimerForManagedStatus {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock onTimeout = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    NSError* error = [NSError errorWithDomain:kAuthenticationErrorDomain
                                         code:TIMED_OUT_FETCH_POLICY
                                     userInfo:nil];
    [strongSelf->_delegate didFailFetchManagedStatus:error];
  };
  _watchdogTimer.reset(new base::OneShotTimer());
  _watchdogTimer->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kAuthenticationFlowTimeoutSeconds),
      base::Bind(onTimeout));
}

- (BOOL)stopWatchdogTimer {
  if (_watchdogTimer) {
    _watchdogTimer->Stop();
    _watchdogTimer.reset();
    return YES;
  }
  return NO;
}

- (void)fetchManagedStatus:(ChromeBrowserState*)browserState
               forIdentity:(ChromeIdentity*)identity {
  ios::ChromeIdentityService* identityService =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  NSString* hostedDomain =
      identityService->GetCachedHostedDomainForIdentity(identity);
  if (hostedDomain) {
    [_delegate didFetchManagedStatus:hostedDomain];
    return;
  }

  [self startWatchdogTimerForManagedStatus];
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->GetHostedDomainForIdentity(
          identity, ^(NSString* hosted_domain, NSError* error) {
            [weakSelf handleGetHostedDomain:hosted_domain
                                      error:error
                               browserState:browserState];
          });
}

- (void)handleGetHostedDomain:(NSString*)hostedDomain
                        error:(NSError*)error
                 browserState:(ChromeBrowserState*)browserState {
  if (![self stopWatchdogTimer]) {
    // Watchdog timer has already fired, don't notify the delegate.
    return;
  }
  if (error) {
    [_delegate didFailFetchManagedStatus:error];
    return;
  }
  [_delegate didFetchManagedStatus:hostedDomain];
}

- (void)signInIdentity:(ChromeIdentity*)identity
      withHostedDomain:(NSString*)hostedDomain
        toBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignIn(identity);
}

- (void)signOutBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                /*force_clear_browsing_data=*/false, ^{
                  [_delegate didSignOut];
                });
}

- (void)signOutImmediatelyFromBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignOut(signin_metrics::ABORT_SIGNIN,
                /*force_clear_browsing_data=*/false, nil);
}

- (void)promptSwitchFromManagedEmail:(NSString*)managedEmail
                    withHostedDomain:(NSString*)hostedDomain
                             toEmail:(NSString*)toEmail
                      viewController:(UIViewController*)viewController
                             browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);
  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SWITCH_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SWITCH_SUBTITLE, base::SysNSStringToUTF16(managedEmail),
      base::SysNSStringToUTF16(toEmail),
      base::SysNSStringToUTF16(hostedDomain));
  NSString* acceptLabel =
      l10n_util::GetNSString(IDS_IOS_MANAGED_SWITCH_ACCEPT_BUTTON);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock acceptBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate]
        didChooseClearDataPolicy:SHOULD_CLEAR_DATA_CLEAR_DATA];
  };
  ProceduralBlock cancelBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didChooseCancel];
  };

  [_alertCoordinator addItemWithTitle:cancelLabel
                               action:cancelBlock
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:acceptLabel
                               action:acceptBlock
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator setCancelAction:cancelBlock];
  [_alertCoordinator start];
}

- (void)promptMergeCaseForIdentity:(ChromeIdentity*)identity
                           browser:(Browser*)browser
                    viewController:(UIViewController*)viewController {
  DCHECK(browser);
  ChromeBrowserState* browserState = browser->GetBrowserState();
  BOOL isSignedIn = YES;
  NSString* lastSignedInEmail =
      [AuthenticationServiceFactory::GetForBrowserState(browserState)
              ->GetAuthenticatedIdentity() userEmail];
  if (!lastSignedInEmail) {
    lastSignedInEmail =
        base::SysUTF8ToNSString(browserState->GetPrefs()->GetString(
            prefs::kGoogleServicesLastUsername));
    isSignedIn = NO;
  }

  if (AuthenticationServiceFactory::GetForBrowserState(browserState)
          ->IsAuthenticatedIdentityManaged()) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForBrowserState(browserState);
    base::Optional<AccountInfo> primary_account_info =
        identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
            identity_manager->GetPrimaryAccountInfo());
    DCHECK(primary_account_info);
    NSString* hostedDomain =
        base::SysUTF8ToNSString(primary_account_info->hosted_domain);
    [self promptSwitchFromManagedEmail:lastSignedInEmail
                      withHostedDomain:hostedDomain
                               toEmail:[identity userEmail]
                        viewController:viewController
                               browser:browser];
    return;
  }
  _navigationController = [SettingsNavigationController
      importDataControllerForBrowser:browser
                            delegate:self
                  importDataDelegate:self
                           fromEmail:lastSignedInEmail
                             toEmail:[identity userEmail]
                          isSignedIn:isSignedIn];
  [_delegate presentViewController:_navigationController
                          animated:YES
                        completion:nil];
}

- (void)clearDataFromBrowser:(Browser*)browser
              commandHandler:(id<BrowsingDataCommands>)handler {
  DCHECK(browser);
  ChromeBrowserState* browserState = browser->GetBrowserState();

  DCHECK(!AuthenticationServiceFactory::GetForBrowserState(browserState)
              ->IsAuthenticated());

  // Workaround for crbug.com/1003578
  //
  // During the Chrome sign-in flow, if the user chooses to clear the cookies
  // when switching their primary account, then the following actions take
  // place (in the following order):
  //   1. All cookies are cleared.
  //   2. The user is signed in to Chrome (aka the primary account set)
  //   2.a. All requests to Gaia page will include Mirror header.
  //   2.b. Account reconcilor will rebuild the Gaia cookies having the Chrome
  //      primary account as the Gaia default web account.
  //
  // The Gaia sign-in webpage monitors changes to its cookies and reloads the
  // page whenever they change. Reloading the webpage while the cookies are
  // cleared and just before they are rebuilt seems to confuse WKWebView that
  // ends up with a nil URL, which in turns translates in about:blank URL shown
  // in the Omnibox.
  //
  // This CL works around this issue by waiting for 1 second between steps 1
  // and 2 above to allow the WKWebView to initiate the reload after the
  // cookies are cleared.
  WebStateList* webStateList = browser->GetWebStateList();
  web::WebState* activeWebState = webStateList->GetActiveWebState();
  bool activeWebStateHasGaiaOrigin =
      activeWebState && (activeWebState->GetVisibleURL().GetOrigin() ==
                         GaiaUrls::GetInstance()->gaia_url());
  int64_t dispatchDelaySecs = activeWebStateHasGaiaOrigin ? 1 : 0;

  [handler
      removeBrowsingDataForBrowserState:browserState
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:^{
                          dispatch_after(
                              dispatch_time(DISPATCH_TIME_NOW,
                                            dispatchDelaySecs * NSEC_PER_SEC),
                              dispatch_get_main_queue(), ^{
                                [_delegate didClearData];
                              });
                        }];
}

- (BOOL)shouldHandleMergeCaseForIdentity:(ChromeIdentity*)identity
                            browserState:(ChromeBrowserState*)browserState {
  CoreAccountId lastSignedInAccountId = CoreAccountId::FromString(
      browserState->GetPrefs()->GetString(prefs::kGoogleServicesLastAccountId));
  CoreAccountId currentSignedInAccountId =
      IdentityManagerFactory::GetForBrowserState(browserState)
          ->PickAccountIdForAccount(
              base::SysNSStringToUTF8([identity gaiaID]),
              base::SysNSStringToUTF8([identity userEmail]));
  if (!lastSignedInAccountId.empty()) {
    // Merge case exists if the id of the previously signed in account is
    // different from the one of the account being signed in.
    return lastSignedInAccountId != currentSignedInAccountId;
  }

  // kGoogleServicesLastAccountId pref might not have been populated yet,
  // check the old kGoogleServicesLastUsername pref.
  std::string lastSignedInEmail =
      browserState->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
  std::string currentSignedInEmail =
      base::SysNSStringToUTF8([identity userEmail]);
  return !lastSignedInEmail.empty() &&
         !gaia::AreEmailsSame(currentSignedInEmail, lastSignedInEmail);
}

- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);
  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SIGNIN_SUBTITLE, base::SysNSStringToUTF16(hostedDomain));
  NSString* acceptLabel =
      l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock acceptBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didAcceptManagedConfirmation];
  };
  ProceduralBlock cancelBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didCancelManagedConfirmation];
  };

  [_alertCoordinator addItemWithTitle:cancelLabel
                               action:cancelBlock
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:acceptLabel
                               action:acceptBlock
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator setCancelAction:cancelBlock];
  [_alertCoordinator start];
}

- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);

  _alertCoordinator = ErrorCoordinatorNoItem(error, viewController, browser);

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock dismissAction = ^{
    [weakSelf alertControllerDidDisappear:weakAlert];
    if (callback)
      callback();
  };

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [_alertCoordinator addItemWithTitle:okButtonLabel
                               action:dismissAction
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator setCancelAction:dismissAction];

  [_alertCoordinator start];
}

- (void)alertControllerDidDisappear:(AlertCoordinator*)alertCoordinator {
  if (_alertCoordinator != alertCoordinator) {
    // Do not reset the |_alertCoordinator| if it has changed. This typically
    // happens when the user taps on any of the actions on "Clear Data Before
    // Syncing?" dialog, as the sign-in confirmation dialog is created before
    // the "Clear Data Before Syncing?" dialog is dismissed.
    return;
  }
  _alertCoordinator = nil;
}

#pragma mark - ImportDataControllerDelegate

- (void)didChooseClearDataPolicy:(ImportDataTableViewController*)controller
                 shouldClearData:(ShouldClearData)shouldClearData {
  DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE, shouldClearData);
  if (shouldClearData == SHOULD_CLEAR_DATA_CLEAR_DATA) {
    base::RecordAction(
        base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
  }

  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock block = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    strongSelf->_navigationController = nil;
    [[strongSelf delegate] didChooseClearDataPolicy:shouldClearData];
  };
  [_navigationController cleanUpSettings];
  [_delegate dismissPresentingViewControllerAnimated:YES completion:block];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  base::RecordAction(base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));

  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock block = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    strongSelf->_navigationController = nil;
    [[strongSelf delegate] didChooseCancel];
  };
  [_navigationController cleanUpSettings];
  [_delegate dismissPresentingViewControllerAnimated:YES completion:block];
}

- (void)settingsWasDismissed {
  base::RecordAction(base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
  [self.delegate didChooseCancel];
  [_navigationController cleanUpSettings];
  _navigationController = nil;
}

- (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
    handlerForSettings {
  NOTREACHED();
  return nil;
}

@end
