// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#include "ios/chrome/browser/passwords/password_store_observer_bridge.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Amount of time after which timestamp is shown instead of "just now".
constexpr base::TimeDelta kJustCheckedTimeThresholdInMinutes =
    base::TimeDelta::FromMinutes(1);
}  // namespace

@interface PasswordsMediator () <PasswordCheckObserver,
                                 PasswordStoreObserver,
                                 SavePasswordsConsumerDelegate> {
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> _passwordStore;

  // Service used to check if user is signed in.
  AuthenticationService* _authService;

  // Service to check if passwords are synced.
  SyncSetupService* _syncService;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordsTableViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> _savedPasswordsConsumer;

  // A helper object which listens to the password store changes.
  std::unique_ptr<PasswordStoreObserverBridge> _passwordStoreObserver;

  // Current state of password check.
  PasswordCheckState _currentState;
}

// Object storing the time of the previous successful re-authentication.
// This is meant to be used by the |ReauthenticationModule| for keeping
// re-authentications valid for a certain time interval within the scope
// of the Passwords Screen.
@property(nonatomic, strong, readonly) NSDate* successfulReauthTime;

@end

@implementation PasswordsMediator

- (instancetype)
    initWithPasswordStore:
        (scoped_refptr<password_manager::PasswordStore>)passwordStore
     passwordCheckManager:
         (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
              authService:(AuthenticationService*)authService
              syncService:(SyncSetupService*)syncService {
  self = [super init];
  if (self) {
    _passwordStore = passwordStore;
    _authService = authService;
    _syncService = syncService;
    _savedPasswordsConsumer =
        std::make_unique<ios::SavePasswordsConsumer>(self);
    _passwordStoreObserver =
        std::make_unique<PasswordStoreObserverBridge>(self);
    _passwordStore->AddObserver(_passwordStoreObserver.get());
    _passwordCheckManager = passwordCheckManager;
    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());
  }
  return self;
}

- (void)dealloc {
  if (_passwordStoreObserver) {
    _passwordStore->RemoveObserver(_passwordStoreObserver.get());
  }
  if (_passwordCheckObserver) {
    _passwordCheckManager->RemoveObserver(_passwordCheckObserver.get());
  }
}

- (void)setConsumer:(id<PasswordsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self loginsDidChange];

  _currentState = _passwordCheckManager->GetPasswordCheckState();
  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWith:_currentState]
               compromisedPasswordsCount:_passwordCheckManager
                                             ->GetCompromisedCredentials()
                                             .size()];
}

#pragma mark - PasswordsTableViewControllerDelegate

- (void)startPasswordCheck {
  _passwordCheckManager->StartPasswordCheck();
}

- (NSString*)formatElapsedTimeSinceLastCheck {
  base::Time lastCompletedCheck =
      _passwordCheckManager->GetLastPasswordCheckTime();

  // lastCompletedCheck is 0.0 in case the check never completely ran before.
  if (lastCompletedCheck == base::Time())
    return l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN);

  base::TimeDelta elapsedTime = base::Time::Now() - lastCompletedCheck;

  NSString* timestamp;
  // If check finished in less than |kJustCheckedTimeThresholdInMinutes| show
  // "just now" instead of timestamp.
  if (elapsedTime < kJustCheckedTimeThresholdInMinutes)
    timestamp = l10n_util::GetNSString(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  else
    timestamp = base::SysUTF8ToNSString(
        base::UTF16ToUTF8(ui::TimeFormat::SimpleWithMonthAndYear(
            ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
            elapsedTime, true)));

  return l10n_util::GetNSStringF(IDS_IOS_LAST_COMPLETED_CHECK,
                                 base::SysNSStringToUTF16(timestamp));
}

- (NSAttributedString*)passwordCheckErrorInfo {
  if (!_passwordCheckManager->GetCompromisedCredentials().empty())
    return nil;

  NSString* message;
  GURL linkURL;

  switch (_currentState) {
    case PasswordCheckState::kRunning:
    case PasswordCheckState::kNoPasswords:
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return nil;
    case PasswordCheckState::kSignedOut:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_SIGNED_OUT);
      break;
    case PasswordCheckState::kOffline:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_OFFLINE);
      break;
    case PasswordCheckState::kQuotaLimit:
      if ([self canUseAccountPasswordCheckup]) {
        message = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_CHECK_ERROR_QUOTA_LIMIT_VISIT_GOOGLE);
        linkURL = password_manager::GetPasswordCheckupURL(
            password_manager::PasswordCheckupReferrer::kPasswordCheck);
      } else {
        message =
            l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_QUOTA_LIMIT);
      }
      break;
    case PasswordCheckState::kOther:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_OTHER);
      break;
  }
  return [self configureTextWithLink:message link:linkURL];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == _currentState)
    return;

  DCHECK(self.consumer);
  [self.consumer
        setPasswordCheckUIState:[self computePasswordCheckUIStateWith:state]
      compromisedPasswordsCount:_passwordCheckManager
                                    ->GetCompromisedCredentials()
                                    .size()];
}

- (void)compromisedCredentialsDidChange:
    (password_manager::InsecureCredentialsManager::CredentialsView)credentials {
  // Compromised passwords changes has no effect on UI while check is running.
  if (_passwordCheckManager->GetPasswordCheckState() ==
      PasswordCheckState::kRunning)
    return;

  DCHECK(self.consumer);

  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWith:_currentState]
               compromisedPasswordsCount:credentials.size()];
}

#pragma mark - Private Methods

// Returns PasswordCheckUIState based on PasswordCheckState.
- (PasswordCheckUIState)computePasswordCheckUIStateWith:
    (PasswordCheckState)newState {
  BOOL wasRunning = _currentState == PasswordCheckState::kRunning;
  _currentState = newState;

  switch (_currentState) {
    case PasswordCheckState::kRunning:
      return PasswordCheckStateRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordCheckStateDisabled;
    case PasswordCheckState::kSignedOut:
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return _passwordCheckManager->GetCompromisedCredentials().empty()
                 ? PasswordCheckStateError
                 : PasswordCheckStateUnSafe;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle: {
      if (!_passwordCheckManager->GetCompromisedCredentials().empty()) {
        return PasswordCheckStateUnSafe;
      } else if (_currentState == PasswordCheckState::kIdle) {
        // Safe state is only possible after the state transitioned from
        // kRunning to kIdle.
        return wasRunning ? PasswordCheckStateSafe : PasswordCheckStateDefault;
      }
      return PasswordCheckStateDefault;
    }
  }
}

// Compute whether user is capable to run password check in Google Account.
- (BOOL)canUseAccountPasswordCheckup {
  return _authService->IsAuthenticated() && _syncService->IsSyncEnabled() &&
         !_syncService->IsEncryptEverythingEnabled();
}

// Configures text for Error Info Popover.
- (NSAttributedString*)configureTextWithLink:(NSString*)text link:(GURL)link {
  NSRange range;

  NSString* strippedText = ParseStringWithLink(text, &range);

  NSRange fullRange = NSMakeRange(0, strippedText.length);
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:strippedText];
  [attributedText addAttribute:NSForegroundColorAttributeName
                         value:[UIColor colorNamed:kTextSecondaryColor]
                         range:fullRange];

  [attributedText
      addAttribute:NSFontAttributeName
             value:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
             range:fullRange];

  if (range.location != NSNotFound && range.length != 0) {
    NSURL* URL = net::NSURLWithGURL(link);
    id linkValue = URL ? URL : @"";
    [attributedText addAttribute:NSLinkAttributeName
                           value:linkValue
                           range:range];
  }

  return attributedText;
}

#pragma mark - PasswordStoreObserver

- (void)loginsDidChange {
  // Cancel ongoing requests to the password store and issue a new request.
  _savedPasswordsConsumer->cancelable_task_tracker()->TryCancelAll();
  _passwordStore->GetAllLogins(_savedPasswordsConsumer.get());
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<password_manager::PasswordForm>>)results {
  DCHECK(self.consumer);
  [self.consumer setPasswordsForms:std::move(results)];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  _successfulReauthTime = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return [self successfulReauthTime];
}

@end
