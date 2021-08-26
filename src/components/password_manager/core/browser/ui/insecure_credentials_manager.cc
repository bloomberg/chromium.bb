// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/password_manager/core/browser/ui/weak_check_utility.h"
#endif

namespace password_manager {

// Extra information about InsecureCredential which is required by UI.
struct CredentialMetadata {
  std::vector<PasswordForm> forms;
  InsecureCredentialTypeFlags type = InsecureCredentialTypeFlags::kSecure;
  base::Time latest_time;
};

namespace {

using CredentialPasswordsMap =
    std::map<CredentialView, CredentialMetadata, PasswordCredentialLess>;

// Transparent comparator that can compare InsecureCredential and
// PasswordForm.
struct CredentialWithoutPasswordLess {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return CredentialOriginAndUsernameAndStore(lhs) <
           CredentialOriginAndUsernameAndStore(rhs);
  }

  using is_transparent = void;

 private:
  static auto CredentialOriginAndUsernameAndStore(const PasswordForm& form) {
    return std::tie(form.signon_realm, form.username_value, form.in_store);
  }

  static auto CredentialOriginAndUsernameAndStore(const InsecureCredential& c) {
    return std::tie(c.signon_realm, c.username, c.in_store);
  }
};

InsecureCredentialTypeFlags ConvertInsecureType(InsecureType type) {
  switch (type) {
    case InsecureType::kLeaked:
      return InsecureCredentialTypeFlags::kCredentialLeaked;
    case InsecureType::kPhished:
      return InsecureCredentialTypeFlags::kCredentialPhished;
    case InsecureType::kWeak:
      return InsecureCredentialTypeFlags::kWeakCredential;
    case InsecureType::kReused:
      return InsecureCredentialTypeFlags::kReusedCredential;
  }
  NOTREACHED();
}

bool IsPasswordFormLeaked(const PasswordForm& form) {
  return form.password_issues.find(InsecureType::kLeaked) !=
         form.password_issues.end();
}

bool IsPasswordFormPhished(const PasswordForm& form) {
  return form.password_issues.find(InsecureType::kPhished) !=
         form.password_issues.end();
}

// This function takes two lists: weak passwords and saved passwords and joins
// them, producing a map that contains CredentialWithPassword as keys and
// vector<PasswordForm> as values.
CredentialPasswordsMap GetInsecureCredentialsFromPasswords(
    const base::flat_set<std::u16string>& weak_passwords,
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  CredentialPasswordsMap credentials_to_forms;

  bool mark_all_credentials_leaked_for_testing =
      base::GetFieldTrialParamByFeatureAsBool(
          password_manager::features::kPasswordChangeInSettings,
          password_manager::features::
              kPasswordChangeInSettingsWithForcedWarningForEverySite,
          false);
  if (mark_all_credentials_leaked_for_testing) {
    for (const auto& form : saved_passwords) {
      CredentialView insecure_credential(form);
      auto& credential_to_form = credentials_to_forms[insecure_credential];
      credential_to_form.type = InsecureCredentialTypeFlags::kCredentialLeaked;
      credential_to_form.forms.push_back(form);
      credential_to_form.latest_time = form.date_created;
    }
    return credentials_to_forms;
  }

  for (const auto& form : saved_passwords) {
    if (IsPasswordFormLeaked(form) || IsPasswordFormPhished(form)) {
      CredentialView insecure_credential(form);
      auto& credential_to_form = credentials_to_forms[insecure_credential];
      for (const auto& pair : form.password_issues) {
        credential_to_form.type |= ConvertInsecureType(pair.first);
        credential_to_form.latest_time =
            std::max(credential_to_form.latest_time, pair.second.create_time);
      }
      // Populate the map. The values are vectors, because it is
      // possible that multiple saved passwords match to the same
      // insecure credential.
      credential_to_form.forms.push_back(form);
    }
    if (weak_passwords.contains(form.password_value)) {
      CredentialView weak_credential(form);
      auto& credential_to_form = credentials_to_forms[weak_credential];
      credential_to_form.type |= InsecureCredentialTypeFlags::kWeakCredential;

      // This helps not to create a copy of the |form| in case the credential
      // has also been insecure. This is important because we don't want to
      // delete the form twice in the RemoveCredential.
      if (!IsInsecure(credential_to_form.type)) {
        credential_to_form.forms.push_back(form);
      }
    }
  }

  return credentials_to_forms;
}

std::vector<CredentialWithPassword> ExtractInsecureCredentials(
    const CredentialPasswordsMap& credentials_to_forms,
    bool (*condition)(const InsecureCredentialTypeFlags&)) {
  std::vector<CredentialWithPassword> credentials;
  for (const auto& credential_to_forms : credentials_to_forms) {
    if (condition(credential_to_forms.second.type)) {
      CredentialWithPassword credential(credential_to_forms.first);
      credential.insecure_type = credential_to_forms.second.type;
      credential.create_time = credential_to_forms.second.latest_time;
      credentials.push_back(std::move(credential));
    }
  }
  return credentials;
}

// The function is only used by the weak check.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
base::flat_set<std::u16string> ExtractPasswords(
    SavedPasswordsPresenter::SavedPasswordsView password_forms) {
  std::vector<std::u16string> passwords;
  passwords.reserve(password_forms.size());
  for (const auto& form : password_forms) {
    passwords.push_back(form.password_value);
  }
  return base::flat_set<std::u16string>(std::move(passwords));
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace

CredentialView::CredentialView(std::string signon_realm,
                               GURL url,
                               std::u16string username,
                               std::u16string password)
    : signon_realm(std::move(signon_realm)),
      url(std::move(url)),
      username(std::move(username)),
      password(std::move(password)) {}

CredentialView::CredentialView(const PasswordForm& form)
    : signon_realm(form.signon_realm),
      url(form.url),
      username(form.username_value),
      password(form.password_value) {}

CredentialView::CredentialView(const CredentialView& credential) = default;
CredentialView::CredentialView(CredentialView&& credential) = default;
CredentialView& CredentialView::operator=(const CredentialView& credential) =
    default;
CredentialView& CredentialView::operator=(CredentialView&& credential) =
    default;
CredentialView::~CredentialView() = default;

CredentialWithPassword::CredentialWithPassword(const CredentialView& credential)
    : CredentialView(credential) {}
CredentialWithPassword::~CredentialWithPassword() = default;
CredentialWithPassword::CredentialWithPassword(
    const CredentialWithPassword& other) = default;

CredentialWithPassword::CredentialWithPassword(CredentialWithPassword&& other) =
    default;
CredentialWithPassword::CredentialWithPassword(
    const InsecureCredential& credential)
    : CredentialView(credential.signon_realm,
                     GURL(credential.signon_realm),
                     credential.username,
                     /*password=*/{}),
      create_time(credential.create_time),
      insecure_type(ConvertInsecureType(credential.insecure_type)) {}

CredentialWithPassword& CredentialWithPassword::operator=(
    const CredentialWithPassword& other) = default;
CredentialWithPassword& CredentialWithPassword::operator=(
    CredentialWithPassword&& other) = default;

InsecureCredentialsManager::InsecureCredentialsManager(
    SavedPasswordsPresenter* presenter,
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : presenter_(presenter),
      profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {
  observed_saved_password_presenter_.Observe(presenter_);
}

InsecureCredentialsManager::~InsecureCredentialsManager() = default;

void InsecureCredentialsManager::Init() {}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void InsecureCredentialsManager::StartWeakCheck(
    base::OnceClosure on_check_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&BulkWeakCheck,
                     ExtractPasswords(presenter_->GetSavedPasswords())),
      base::BindOnce(&InsecureCredentialsManager::OnWeakCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer())
          .Then(std::move(on_check_done)));
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

void InsecureCredentialsManager::SaveInsecureCredential(
    const LeakCheckCredential& credential) {
  // Iterate over all currently saved credentials and mark those as insecure
  // that have the same canonicalized username and password.
  const std::u16string canonicalized_username =
      CanonicalizeUsername(credential.username());
  for (const PasswordForm& saved_password : presenter_->GetSavedPasswords()) {
    if (saved_password.password_value == credential.password() &&
        CanonicalizeUsername(saved_password.username_value) ==
            canonicalized_username) {
      PasswordForm form_to_update = saved_password;
      form_to_update.password_issues.insert_or_assign(
          InsecureType::kLeaked,
          InsecurityMetadata(base::Time::Now(), IsMuted(false)));
      GetStoreFor(saved_password).UpdateLogin(form_to_update);
    }
  }
}

bool InsecureCredentialsManager::UpdateCredential(
    const CredentialView& credential,
    const base::StringPiece password) {
  auto it = credentials_to_forms_.find(credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Make sure there are matching password forms. Also erase duplicates if there
  // are any.
  const auto& forms = it->second.forms;
  if (forms.empty())
    return false;

  for (size_t i = 1; i < forms.size(); ++i)
    GetStoreFor(forms[i]).RemoveLogin(forms[i]);

  // Note: We Invoke EditPassword on the presenter rather than UpdateLogin() on
  // the store, so that observers of the presenter get notified of this event.
  return presenter_->EditPassword(forms[0], base::UTF8ToUTF16(password));
}

bool InsecureCredentialsManager::RemoveCredential(
    const CredentialView& credential) {
  auto it = credentials_to_forms_.find(credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Erase all matching credentials from the store. Return whether any
  // credentials were deleted.
  const auto& saved_passwords = it->second.forms;
  for (const PasswordForm& saved_password : saved_passwords)
    GetStoreFor(saved_password).RemoveLogin(saved_password);

  return !saved_passwords.empty();
}

std::vector<CredentialWithPassword>
InsecureCredentialsManager::GetInsecureCredentials() const {
  return ExtractInsecureCredentials(credentials_to_forms_, &IsInsecure);
}

std::vector<CredentialWithPassword>
InsecureCredentialsManager::GetWeakCredentials() const {
  std::vector<CredentialWithPassword> weak_credentials =
      ExtractInsecureCredentials(credentials_to_forms_, &IsWeak);

  auto get_sort_key = [this](const CredentialWithPassword& credential) {
    return CreateSortKey(GetSavedPasswordsFor(credential)[0],
                         IgnoreStore(true));
  };
  base::ranges::sort(weak_credentials, {}, get_sort_key);
  return weak_credentials;
}

SavedPasswordsPresenter::SavedPasswordsView
InsecureCredentialsManager::GetSavedPasswordsFor(
    const CredentialView& credential) const {
  auto it = credentials_to_forms_.find(credential);
  return it != credentials_to_forms_.end()
             ? it->second.forms
             : SavedPasswordsPresenter::SavedPasswordsView();
}

void InsecureCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InsecureCredentialsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InsecureCredentialsManager::UpdateInsecureCredentials() {
  credentials_to_forms_ = GetInsecureCredentialsFromPasswords(
      weak_passwords_, presenter_->GetSavedPasswords());
}

void InsecureCredentialsManager::OnWeakCheckDone(
    base::ElapsedTimer timer_since_weak_check_start,
    base::flat_set<std::u16string> weak_passwords) {
  base::UmaHistogramTimes("PasswordManager.WeakCheck.Time",
                          timer_since_weak_check_start.Elapsed());
  weak_passwords_ = std::move(weak_passwords);
  UpdateInsecureCredentials();
  NotifyWeakCredentialsChanged();
}

void InsecureCredentialsManager::OnEdited(const PasswordForm& form) {
  // The WeakCheck is a Desktop only feature for now. Disable on Mobile to avoid
  // pulling in a big dependency on zxcvbn.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  const std::u16string& password = form.password_value;
  if (weak_passwords_.contains(password) || !IsWeak(password)) {
    // Either the password is already known to be weak, or it is not weak at
    // all. In both cases there is nothing to do.
    return;
  }

  weak_passwords_.insert(password);
  UpdateInsecureCredentials();
  NotifyWeakCredentialsChanged();
#endif
}

// Re-computes the list of insecure credentials with passwords after obtaining a
// new list of saved passwords.
void InsecureCredentialsManager::OnSavedPasswordsChanged(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  credentials_to_forms_ =
      GetInsecureCredentialsFromPasswords(weak_passwords_, saved_passwords);
  NotifyInsecureCredentialsChanged();
  NotifyWeakCredentialsChanged();
}

void InsecureCredentialsManager::NotifyInsecureCredentialsChanged() {
  std::vector<CredentialWithPassword> insecure_credentials =
      ExtractInsecureCredentials(credentials_to_forms_, &IsInsecure);
  for (auto& observer : observers_) {
    observer.OnInsecureCredentialsChanged(insecure_credentials);
  }
}

void InsecureCredentialsManager::NotifyWeakCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.OnWeakCredentialsChanged();
  }
}

PasswordStoreInterface& InsecureCredentialsManager::GetStoreFor(
    const PasswordForm& form) {
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
