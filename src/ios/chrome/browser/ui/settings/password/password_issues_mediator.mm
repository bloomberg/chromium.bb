// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_issue_with_form.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesMediator () <PasswordCheckObserver> {
  IOSChromePasswordCheckManager* _manager;

  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  std::vector<password_manager::CredentialWithPassword> _compromisedCredentials;
}

// Object storing the time of the previous successful re-authentication.
// This is meant to be used by the |ReauthenticationModule| for keeping
// re-authentications valid for a certain time interval within the scope
// of the Password Issues Screen.
@property(nonatomic, strong, readonly) NSDate* successfulReauthTime;

@end

@implementation PasswordIssuesMediator

- (instancetype)initWithPasswordCheckManager:
    (IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _manager = manager;
    _passwordCheckObserver.reset(
        new PasswordCheckObserverBridge(self, manager));
  }
  return self;
}

- (void)setConsumer:(id<PasswordIssuesConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self fetchPasswordIssues];
}

- (void)deletePassword:(const password_manager::PasswordForm&)password {
  for (const auto& credential : _compromisedCredentials) {
    if (std::tie(credential.signon_realm, credential.username,
                 credential.password) == std::tie(password.signon_realm,
                                                  password.username_value,
                                                  password.password_value)) {
      _manager->DeleteCompromisedPasswordForm(password);
      return;
    }
  }
  _manager->DeletePasswordForm(password);
  // TODO:(crbug.com/1075494) - Update list of compromised passwords without
  // awaiting compromisedCredentialsDidChange.
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op.
}

- (void)compromisedCredentialsDidChange:
    (password_manager::InsecureCredentialsManager::CredentialsView)credentials {
  [self fetchPasswordIssues];
}

#pragma mark - Private Methods

- (void)fetchPasswordIssues {
  DCHECK(self.consumer);
  _compromisedCredentials = _manager->GetCompromisedCredentials();
  NSMutableArray* passwords = [[NSMutableArray alloc] init];
  for (auto credential : _compromisedCredentials) {
    const password_manager::PasswordForm form =
        _manager->GetSavedPasswordsFor(credential)[0];
    [passwords
        addObject:[[PasswordIssueWithForm alloc] initWithPasswordForm:form]];
  }

  NSSortDescriptor* origin = [[NSSortDescriptor alloc] initWithKey:@"website"
                                                         ascending:YES];
  NSSortDescriptor* username = [[NSSortDescriptor alloc] initWithKey:@"username"
                                                           ascending:YES];

  [self.consumer
      setPasswordIssues:[passwords
                            sortedArrayUsingDescriptors:@[ origin, username ]]];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  _successfulReauthTime = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return [self successfulReauthTime];
}

@end
