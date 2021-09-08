// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"

#import "base/check.h"
#import "base/format_macros.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Enum to describe all 4 cases for a user being signed-in. This enum is used
// internaly by SignoutActionSheetCoordinator().
typedef NS_ENUM(NSUInteger, SignedInUserState) {
  // Sign-in with a managed account and sync is turned on.
  SignedInUserStateWithManagedAccountAndSyncing,
  // Sign-in with a managed account and sync is turned off.
  SignedInUserStateWithManagedAccountAndNotSyncing,
  // Sign-in with a regular account and sync is turned on.
  SignedInUserStateWithNonManagedAccountAndSyncing,
  // Sign-in with a regular account and sync is turned off.
  SignedInUserStateWithNoneManagedAccountAndNotSyncing
};

@interface SignoutActionSheetCoordinator () {
  // Rectangle for the popover alert.
  CGRect _rect;
  // View for the popovert alert.
  __weak UIView* _view;
}

// Service for managing identity authentication.
@property(nonatomic, assign, readonly)
    AuthenticationService* authenticationService;
// Action sheet to display sign-out actions.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

@end

@implementation SignoutActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      rect:(CGRect)rect
                                      view:(UIView*)view {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _rect = rect;
    _view = view;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.completion);
  DCHECK(self.authenticationService->IsAuthenticated());

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:self.actionSheetCoordinatorTitle
                         message:nil
                            rect:_rect
                            view:_view];

  __weak SignoutActionSheetCoordinator* weakSelf = self;
  switch (self.signedInUserState) {
    case SignedInUserStateWithManagedAccountAndSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:YES];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      NSString* const keepOnDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:YES];
                    }
                     style:UIAlertActionStyleDestructive];
      [self.actionSheetCoordinator
          addItemWithTitle:keepOnDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                    }
                     style:UIAlertActionStyleDefault];
      break;
    }
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      NSString* const signOutButtonTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
  }
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  if (weakSelf) {
                    weakSelf.completion(NO);
                  }
                }
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

- (void)stop {
  self.actionSheetCoordinator = nil;
}

#pragma mark - ActionSheetCoordinator properties

- (NSString*)title {
  return self.actionSheetCoordinator.title;
}

#pragma mark - Browser-based properties

- (AuthenticationService*)authenticationService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

// Returns the user's sign-in and syncing state.
- (SignedInUserState)signedInUserState {
  DCHECK(self.browser);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  BOOL syncEnabled = syncSetupService->IsFirstSetupComplete();
  SignedInUserState signedInUserState;
  if (self.authenticationService->IsAuthenticatedIdentityManaged()) {
    signedInUserState = syncEnabled
                            ? SignedInUserStateWithManagedAccountAndSyncing
                            : SignedInUserStateWithManagedAccountAndNotSyncing;
  } else {
    signedInUserState =
        syncEnabled ? SignedInUserStateWithNonManagedAccountAndSyncing
                    : SignedInUserStateWithNoneManagedAccountAndNotSyncing;
  }
  return signedInUserState;
}

// Returns the title associated to the given user sign-in state or nil if no
// title is defined for the state.
- (NSString*)actionSheetCoordinatorTitle {
  DCHECK(self.browser);
  NSString* title = nil;
  switch (self.signedInUserState) {
    case SignedInUserStateWithManagedAccountAndSyncing: {
      std::u16string hostedDomain = HostedDomainForPrimaryAccount(self.browser);
      title = l10n_util::GetNSStringF(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_MANAGED_ACCOUNT,
          hostedDomain);
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      title = l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_ACCOUNT);
      break;
    }
    case SignedInUserStateWithManagedAccountAndNotSyncing:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      // No title.
      break;
    }
  }
  return title;
}

#pragma mark - Private

// Signs the user out of the primary account and clears the data from their
// device if specified to do so.
- (void)handleSignOutWithForceClearData:(BOOL)forceClearData {
  if (!self.browser)
    return;

  if (!self.authenticationService->IsAuthenticated()) {
    self.completion(YES);
    return;
  }

  [self.delegate didSelectSignoutDataRetentionStrategy];

  [self.baseViewController.view setUserInteractionEnabled:NO];
  __weak SignoutActionSheetCoordinator* weakSelf = self;
  self.authenticationService->SignOut(
      signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, forceClearData, ^{
        if (!weakSelf) {
          return;
        }
        [weakSelf.baseViewController.view setUserInteractionEnabled:YES];
        weakSelf.completion(YES);
      });
  // Get UMA metrics on the usage of different options for signout available
  // for users with non-managed accounts.
  if (!self.authenticationService->IsAuthenticatedIdentityManaged()) {
    UMA_HISTOGRAM_BOOLEAN("Signin.UserRequestedWipeDataOnSignout",
                          forceClearData);
  }
  if (forceClearData) {
    base::RecordAction(base::UserMetricsAction(
        "Signin_SignoutClearData_FromAccountListSettings"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Signin_Signout_FromAccountListSettings"));
  }
}

@end
