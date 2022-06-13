// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyDefaultAccountMediator () <
    ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation ConsistencyDefaultAccountMediator

- (instancetype)initWithAccountManagerService:
    (ChromeAccountManagerService*)accountManagerService {
  if (self = [super init]) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)disconnect {
  self.accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<ConsistencyDefaultAccountConsumer>)consumer {
  _consumer = consumer;
  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(ChromeIdentity*)identity {
  DCHECK(identity);
  if (_selectedIdentity == identity) {
    return;
  }
  _selectedIdentity = identity;
  [self updateSelectedIdentityUI];
}

#pragma mark - Private

// Updates the default identity.
- (void)selectSelectedIdentity {
  if (!self.accountManagerService) {
    return;
  }

  ChromeIdentity* identity = self.accountManagerService->GetDefaultIdentity();
  if (!identity) {
    [self.delegate consistencyDefaultAccountMediatorNoIdentities:self];
    return;
  }

  if ([identity isEqual:self.selectedIdentity]) {
    return;
  }

  self.selectedIdentity = identity;
}

// Updates the view controller using the default identity.
- (void)updateSelectedIdentityUI {
  [self.consumer updateWithFullName:self.selectedIdentity.userFullName
                          givenName:self.selectedIdentity.userGivenName
                              email:self.selectedIdentity.userEmail];
  UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
      self.selectedIdentity, IdentityAvatarSize::TableViewIcon);
  [self.consumer updateUserAvatar:avatar];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self selectSelectedIdentity];
}

- (void)identityChanged:(ChromeIdentity*)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

@end
