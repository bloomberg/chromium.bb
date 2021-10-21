// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using password_manager::CredentialWithPassword;
using password_manager::LeakCheckCredential;
using InsecureCredentialsView =
    password_manager::InsecureCredentialsManager::CredentialsView;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;
using State = password_manager::BulkLeakCheckServiceInterface::State;

// Key used to attach UserData to a LeakCheckCredential.
constexpr char kPasswordCheckDataKey[] = "password-check-manager-data-key";
// Minimum time the check should be running.
constexpr base::TimeDelta kDelay = base::Seconds(3);

// Class which ensures that IOSChromePasswordCheckManager will stay alive
// until password check is completed even if class what initially created
// IOSChromePasswordCheckManager was destroyed.
class IOSChromePasswordCheckManagerHolder : public LeakCheckCredential::Data {
 public:
  explicit IOSChromePasswordCheckManagerHolder(
      scoped_refptr<IOSChromePasswordCheckManager> manager)
      : manager_(std::move(manager)) {}
  ~IOSChromePasswordCheckManagerHolder() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<IOSChromePasswordCheckManagerHolder>(manager_);
  }

 private:
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
};

PasswordCheckState ConvertBulkCheckState(State state) {
  switch (state) {
    case State::kIdle:
      return PasswordCheckState::kIdle;
    case State::kRunning:
      return PasswordCheckState::kRunning;
    case State::kSignedOut:
      return PasswordCheckState::kSignedOut;
    case State::kNetworkError:
      return PasswordCheckState::kOffline;
    case State::kQuotaLimit:
      return PasswordCheckState::kQuotaLimit;
    case State::kCanceled:
      return PasswordCheckState::kCanceled;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return PasswordCheckState::kOther;
  }
  NOTREACHED();
  return PasswordCheckState::kIdle;
}
}  // namespace

IOSChromePasswordCheckManager::IOSChromePasswordCheckManager(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      password_store_(IOSChromePasswordStoreFactory::GetForBrowserState(
          browser_state,
          ServiceAccessType::EXPLICIT_ACCESS)),
      saved_passwords_presenter_(password_store_),
      insecure_credentials_manager_(&saved_passwords_presenter_,
                                    password_store_),
      bulk_leak_check_service_adapter_(
          &saved_passwords_presenter_,
          IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
              browser_state),
          browser_state->GetPrefs()) {
  observed_saved_passwords_presenter_.Observe(&saved_passwords_presenter_);
  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);
  observed_bulk_leak_check_service_.Observe(
      IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(browser_state));

  // Instructs the presenter and manager to initialize and build their caches.
  saved_passwords_presenter_.Init();
  insecure_credentials_manager_.Init();
}

IOSChromePasswordCheckManager::~IOSChromePasswordCheckManager() {
  DCHECK(observers_.empty());
}

void IOSChromePasswordCheckManager::StartPasswordCheck() {
  if (is_initialized_) {
    IOSChromePasswordCheckManagerHolder data(
        scoped_refptr<IOSChromePasswordCheckManager>(this));
    bulk_leak_check_service_adapter_.StartBulkLeakCheck(kPasswordCheckDataKey,
                                                        &data);
    is_check_running_ = true;
    start_time_ = base::Time::Now();
  } else {
    start_check_on_init_ = true;
  }
}

void IOSChromePasswordCheckManager::StopPasswordCheck() {
  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
  is_check_running_ = false;
}

PasswordCheckState IOSChromePasswordCheckManager::GetPasswordCheckState()
    const {
  if (saved_passwords_presenter_.GetSavedPasswords().empty()) {
    return PasswordCheckState::kNoPasswords;
  }
  return ConvertBulkCheckState(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

base::Time IOSChromePasswordCheckManager::GetLastPasswordCheckTime() const {
  return base::Time::FromDoubleT(browser_state_->GetPrefs()->GetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted));
}

std::vector<CredentialWithPassword>
IOSChromePasswordCheckManager::GetCompromisedCredentials() const {
  return insecure_credentials_manager_.GetInsecureCredentials();
}

password_manager::SavedPasswordsPresenter::SavedPasswordsView
IOSChromePasswordCheckManager::GetAllCredentials() const {
  return saved_passwords_presenter_.GetSavedPasswords();
}

password_manager::SavedPasswordsPresenter::SavedPasswordsView
IOSChromePasswordCheckManager::GetSavedPasswordsFor(
    const CredentialWithPassword& credential) const {
  return insecure_credentials_manager_.GetSavedPasswordsFor(credential);
}

bool IOSChromePasswordCheckManager::EditPasswordForm(
    const password_manager::PasswordForm& form,
    const std::u16string& new_username,
    const std::u16string& new_password) {
  return saved_passwords_presenter_.EditSavedPasswords(form, new_username,
                                                       new_password);
}

bool IOSChromePasswordCheckManager::AddPasswordForm(
    const password_manager::PasswordForm& form) {
  return saved_passwords_presenter_.AddPassword(form);
}

void IOSChromePasswordCheckManager::EditCompromisedPasswordForm(
    const password_manager::PasswordForm& form,
    base::StringPiece password) {
  insecure_credentials_manager_.UpdateCredential(
      password_manager::CredentialView(form), password);
}

void IOSChromePasswordCheckManager::DeletePasswordForm(
    const password_manager::PasswordForm& form) {
  saved_passwords_presenter_.RemovePassword(form);
}

void IOSChromePasswordCheckManager::DeleteCompromisedPasswordForm(
    const password_manager::PasswordForm& form) {
  insecure_credentials_manager_.RemoveCredential(
      password_manager::CredentialView(form));
}

void IOSChromePasswordCheckManager::OnSavedPasswordsChanged(
    SavedPasswordsView) {
  // Observing saved passwords to update possible kNoPasswords state.
  NotifyPasswordCheckStatusChanged();
  if (!std::exchange(is_initialized_, true) && start_check_on_init_) {
    StartPasswordCheck();
  }
}

void IOSChromePasswordCheckManager::OnInsecureCredentialsChanged(
    InsecureCredentialsView credentials) {
  for (auto& observer : observers_) {
    observer.CompromisedCredentialsChanged(credentials);
  }
}

void IOSChromePasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Saving time of last successful password check
    browser_state_->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().ToDoubleT());
    browser_state_->GetPrefs()->SetTime(
        password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
        base::Time::Now());
  }
  if (state != State::kRunning) {
    // If check was running
    if (is_check_running_) {
      const base::TimeDelta elapsed = base::Time::Now() - start_time_;
      if (elapsed < kDelay) {
        base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&IOSChromePasswordCheckManager::
                               NotifyPasswordCheckStatusChanged,
                           weak_ptr_factory_.GetWeakPtr()),
            kDelay - elapsed);
        is_check_running_ = false;
        return;
      }
    }
    is_check_running_ = false;
  }
  NotifyPasswordCheckStatusChanged();
}

void IOSChromePasswordCheckManager::OnCredentialDone(
    const LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (is_leaked) {
    insecure_credentials_manager_.SaveInsecureCredential(credential);
  }
}

void IOSChromePasswordCheckManager::NotifyPasswordCheckStatusChanged() {
  for (auto& observer : observers_) {
    observer.PasswordCheckStatusChanged(GetPasswordCheckState());
  }
}
