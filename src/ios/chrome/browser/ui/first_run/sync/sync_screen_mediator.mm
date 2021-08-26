// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenMediator () <ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

// Manager for user's Google identities.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// Auditor for user consent.
@property(nonatomic, assign) consent_auditor::ConsentAuditor* consentAuditor;
// Chrome interface to the iOS shared authentication library.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Service that allows for configuring sync.
@property(nonatomic, assign) SyncSetupService* syncSetupService;
// Manager for user consent.
@property(nonatomic, assign)
    unified_consent::UnifiedConsentService* unifiedConsentService;
// Manager for the authentication flow.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;

@end

@implementation SyncScreenMediator

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
            accountManagerService:
                (ChromeAccountManagerService*)accountManagerService
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
                 syncSetupService:(SyncSetupService*)syncSetupService
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService {
  self = [super init];
  if (self) {
    DCHECK(unifiedConsentService);

    _identityManager = identityManager;
    _consentAuditor = consentAuditor;
    _authenticationService = authenticationService;
    _syncSetupService = syncSetupService;
    _unifiedConsentService = unifiedConsentService;
    _accountManagerServiceObserver.reset(
        new ChromeAccountManagerServiceObserverBridge(self,
                                                      accountManagerService));
  }
  return self;
}

- (void)disconnect {
  _accountManagerServiceObserver = nullptr;
}

- (void)startSyncWithConfirmationID:(const int)confirmationID
                           consentIDs:(NSArray<NSNumber*>*)consentIDs
                   authenticationFlow:(AuthenticationFlow*)authenticationFlow
    advancedSyncSettingsLinkWasTapped:(BOOL)advancedSyncSettingsLinkWasTapped {
  DCHECK(!self.authenticationFlow);

  [self.consumer setUIEnabled:NO];

  // Local copy to be captured.
  NSArray<NSNumber*>* consentIDsCopy = [consentIDs copy];

  self.authenticationFlow = authenticationFlow;
  __weak __typeof(self) weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signinCompletedWithSuccess:success
                          confirmationID:confirmationID
                              consentIDs:consentIDsCopy
               advancedSettingsRequested:advancedSyncSettingsLinkWasTapped];
  }];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  ChromeIdentity* identity = self.authenticationService->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin);
  if (!identity) {
    [self.delegate userRemoved];
  }
}

#pragma mark - Private

// Callback used when the sign in flow is complete, with |success|.
- (void)signinCompletedWithSuccess:(BOOL)success
                    confirmationID:(const int)confirmationID
                        consentIDs:(NSArray<NSNumber*>*)consentIDs
         advancedSettingsRequested:(BOOL)advancedSettingsRequested {
  self.authenticationFlow = nil;
  [self.consumer setUIEnabled:YES];

  if (!success)
    return;

  ChromeIdentity* identity = self.authenticationService->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin);
  DCHECK(identity);

  sync_pb::UserConsentTypes::SyncConsent syncConsent;
  syncConsent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                             UserConsentTypes_ConsentStatus_GIVEN);

  syncConsent.set_confirmation_grd_id(confirmationID);
  for (NSNumber* consentID in consentIDs) {
    syncConsent.add_description_grd_ids([consentID intValue]);
  }

  CoreAccountId coreAccountId = self.identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8([identity gaiaID]),
      base::SysNSStringToUTF8([identity userEmail]));
  self.consentAuditor->RecordSyncConsent(coreAccountId, syncConsent);
  self.authenticationService->GrantSyncConsent(identity);

  // The consent has to be given as soon as the user is signed in. Even when
  // they open the settings through the link.
  self.unifiedConsentService->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  if (!advancedSettingsRequested) {
    // Turn on FirstSetupComplete flag after the authentication service has
    // granted user consent to start Sync.
    self.syncSetupService->SetFirstSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

    self.syncSetupService->CommitSyncChanges();
  }

  [self.delegate syncScreenMediator:self
          didFinishSigninWithResult:SigninCoordinatorResultSuccess];
}

@end
