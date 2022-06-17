// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/consent_auditor.h"
#include "components/sync/driver/sync_service.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninSyncMediator () <ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

// Logger used to record sign in metrics.
@property(nonatomic, strong) UserSigninLogger* logger;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// Authentication service for sign in.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Manager for user's Google identities.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// Auditor for user consent.
@property(nonatomic, assign) consent_auditor::ConsentAuditor* consentAuditor;
// Service to configure sync.
@property(nonatomic, assign) SyncSetupService* syncSetupService;
// Manager for user consent.
@property(nonatomic, assign)
    unified_consent::UnifiedConsentService* unifiedConsentService;
// Manager for the authentication flow.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// Whether the setting screen was presented.
@property(nonatomic, assign) BOOL settingsScreenShown;

@end

@implementation SigninSyncMediator

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
            accountManagerService:
                (ChromeAccountManagerService*)accountManagerService
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
                 syncSetupService:(SyncSetupService*)syncSetupService
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService
                      syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    DCHECK(accountManagerService);
    DCHECK(authenticationService);

    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
    _consentAuditor = consentAuditor;
    _syncSetupService = syncSetupService;
    _unifiedConsentService = unifiedConsentService;
    _syncService = syncService;

    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);

    _logger = [[FirstRunSigninLogger alloc]
          initWithPromoAction:signin_metrics::PromoAction::
                                  PROMO_ACTION_NO_SIGNIN_PROMO
        accountManagerService:accountManagerService];

    [_logger logSigninStarted];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)disconnect {
  [self.logger disconnect];
  self.accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

- (void)cancelSyncAndRestoreSigninState:(IdentitySigninState)signinStateOnStart
                  signinIdentityOnStart:(ChromeIdentity*)signinIdentityOnStart {
  [self.consumer setUIEnabled:NO];
  [self.authenticationFlow cancelAndDismissAnimated:NO];

  self.syncService->GetUserSettings()->SetSyncRequested(false);
  switch (signinStateOnStart) {
    case IdentitySigninStateSignedOut: {
      __weak __typeof(self) weakSelf = self;
      self.authenticationService->SignOut(
          signin_metrics::ABORT_SIGNIN,
          /*force_clear_browsing_data=*/false, ^{
            [weakSelf onSigninStateRestorationCompleted];
          });
      break;
    }
    case IdentitySigninStateSignedInWithSyncDisabled: {
      DCHECK(!self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSync));
      if ([self.authenticationService->GetPrimaryIdentity(
              signin::ConsentLevel::kSignin) isEqual:signinIdentityOnStart]) {
        // Can't be synced in this option because sync has to be disabled.
        _syncService->StopAndClear();
        [self onSigninStateRestorationCompleted];
      } else {
        __weak __typeof(self) weakSelf = self;
        self.authenticationService->SignOut(
            signin_metrics::ABORT_SIGNIN,
            /*force_clear_browsing_data=*/false, ^() {
              AuthenticationService* authenticationService =
                  weakSelf.authenticationService;
              ChromeIdentity* identity = signinIdentityOnStart;
              ChromeAccountManagerService* accountManagerService =
                  weakSelf.accountManagerService;
              if (authenticationService && identity &&
                  accountManagerService->IsValidIdentity(identity)) {
                // Sign back in with a valid identity.
                authenticationService->SignIn(identity, nil);
              }
              [weakSelf onSigninStateRestorationCompleted];
            });
      }
      break;
    }
    case IdentitySigninStateSignedInWithSyncEnabled: {
      // This view wouldn't be shown if sync is enabled, so this option
      // shouldn't be reached.
      NOTREACHED();
      break;
    }
  }
}

- (void)startSyncWithConfirmationID:(const int)confirmationID
                         consentIDs:(NSArray<NSNumber*>*)consentIDs
                 authenticationFlow:(AuthenticationFlow*)authenticationFlow {
  DCHECK(!self.authenticationFlow);

  [self.consumer setUIEnabled:NO];

  // Local copy to be captured to make sure that the updates don't propagate to
  // the authentication flow when it is started.
  NSArray<NSNumber*>* consentIDsCopy = [consentIDs copy];

  self.authenticationFlow = authenticationFlow;
  __weak __typeof(self) weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signinCompletedWithSuccess:success
                          confirmationID:confirmationID
                              consentIDs:consentIDsCopy];
  }];
}

- (void)prepareAdvancedSettingsWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow {
  DCHECK(!self.authenticationFlow);

  self.settingsScreenShown = YES;

  [self.consumer setUIEnabled:NO];

  self.authenticationFlow = authenticationFlow;
  __weak __typeof(self) weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signinForAdvancedSettingsCompletedWithSuccess:success];
  }];
}

#pragma mark - Properties

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if ([self.selectedIdentity isEqual:selectedIdentity])
    return;
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !self.accountManagerService->HasIdentities());
  _selectedIdentity = selectedIdentity;

  [self updateConsumerIdentity];
}

- (void)setConsumer:(id<SigninSyncConsumer>)consumer {
  if (consumer == _consumer)
    return;
  _consumer = consumer;

  [self updateConsumerIdentity];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (!self.accountManagerService) {
    return;
  }

  if (!self.selectedIdentity ||
      !self.accountManagerService->IsValidIdentity(self.selectedIdentity)) {
    self.selectedIdentity = self.accountManagerService->GetDefaultIdentity();
  }
}

- (void)identityChanged:(ChromeIdentity*)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateConsumerIdentity];
  }
}

#pragma mark - Private

// Updates the identity displayed by the consumer.
- (void)updateConsumerIdentity {
  if (!self.selectedIdentity) {
    [self.consumer noIdentityAvailable];
  } else {
    UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
        self.selectedIdentity, IdentityAvatarSize::DefaultLarge);
    [self.consumer
        setSelectedIdentityUserName:self.selectedIdentity.userFullName
                              email:self.selectedIdentity.userEmail
                          givenName:self.selectedIdentity.userGivenName
                             avatar:avatar];
  }
}

// Callback used when the sign-in flow is complete, with/without `success`.
- (void)signinCompletedWithSuccess:(BOOL)success
                    confirmationID:(const int)confirmationID
                        consentIDs:(NSArray<NSNumber*>*)consentIDs {
  self.authenticationFlow = nil;
  [self.consumer setActionToDone];

  if (!success) {
    return;
  }

  // TODO(crbug.com/1254359): Dedupe duplicated code, here and in
  // user_signin_mediator.

  [self.logger logSigninCompletedWithResult:SigninCoordinatorResultSuccess
                               addedAccount:self.addedAccount
                      advancedSettingsShown:self.settingsScreenShown];

  // Set sync consent.
  sync_pb::UserConsentTypes::SyncConsent syncConsent;
  syncConsent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                             UserConsentTypes_ConsentStatus_GIVEN);
  syncConsent.set_confirmation_grd_id(confirmationID);
  for (NSNumber* consentID in consentIDs) {
    syncConsent.add_description_grd_ids([consentID intValue]);
  }

  // Set the account to enable sync for.
  DCHECK(self.selectedIdentity);
  CoreAccountId coreAccountId = self.identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8([self.selectedIdentity gaiaID]),
      base::SysNSStringToUTF8([self.selectedIdentity userEmail]));

  self.consentAuditor->RecordSyncConsent(coreAccountId, syncConsent);
  self.authenticationService->GrantSyncConsent(self.selectedIdentity);

  self.unifiedConsentService->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  // Turn on FirstSetupComplete flag after the authentication service has
  // granted user consent to start Sync.
  self.syncSetupService->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  self.syncSetupService->CommitSyncChanges();

  [self.delegate signinSyncMediatorDidSuccessfulyFinishSignin:self];
}

// Callback used when the sign-in flow used for advanced settings is complete,
// with/without `success`.
- (void)signinForAdvancedSettingsCompletedWithSuccess:(BOOL)success {
  self.authenticationFlow = nil;
  [self.consumer setActionToDone];

  if (!success) {
    return;
  }

  // Sync has to be set as requested in order to display the preferences
  // correctly and differentiate the special state where the user is signed
  // in, but the sync feature can't start yet.
  _syncService->GetUserSettings()->SetSyncRequested(true);

  [self.delegate
      signinSyncMediatorDidSuccessfulyFinishSigninForAdvancedSettings:self];
}

- (void)onSigninStateRestorationCompleted {
  // Stop the loading overlay and call back to the coordinator.
  [self.consumer setActionToDone];
  [self.delegate signinSyncMediatorDidSuccessfulyFinishSignout:self];
}

@end
