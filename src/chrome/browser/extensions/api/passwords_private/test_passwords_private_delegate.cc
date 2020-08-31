// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "ui/base/l10n/time_format.h"

namespace extensions {

namespace {

using ui::TimeFormat;

constexpr size_t kNumMocks = 3;

api::passwords_private::PasswordUiEntry CreateEntry(int id) {
  api::passwords_private::PasswordUiEntry entry;
  entry.urls.shown = "test" + base::NumberToString(id) + ".com";
  entry.urls.origin = "http://" + entry.urls.shown + "/login";
  entry.urls.link = entry.urls.origin;
  entry.username = "testName" + base::NumberToString(id);
  entry.id = id;
  return entry;
}

api::passwords_private::ExceptionEntry CreateException(int id) {
  api::passwords_private::ExceptionEntry exception;
  exception.urls.shown = "exception" + base::NumberToString(id) + ".com";
  exception.urls.origin = "http://" + exception.urls.shown + "/login";
  exception.urls.link = exception.urls.origin;
  exception.id = id;
  return exception;
}
}  // namespace

TestPasswordsPrivateDelegate::TestPasswordsPrivateDelegate()
    : profile_(nullptr) {
  // Create mock data.
  for (size_t i = 0; i < kNumMocks; i++) {
    current_entries_.push_back(CreateEntry(i));
    current_exceptions_.push_back(CreateException(i));
  }
}
TestPasswordsPrivateDelegate::~TestPasswordsPrivateDelegate() = default;

void TestPasswordsPrivateDelegate::GetSavedPasswordsList(
    UiEntriesCallback callback) {
  std::move(callback).Run(current_entries_);
}

void TestPasswordsPrivateDelegate::GetPasswordExceptionsList(
    ExceptionEntriesCallback callback) {
  std::move(callback).Run(current_exceptions_);
}

void TestPasswordsPrivateDelegate::ChangeSavedPassword(
    int id,
    base::string16 username,
    base::Optional<base::string16> password) {
  if (static_cast<size_t>(id) >= current_entries_.size())
    return;

  // PasswordUiEntry does not contain a password. Thus we are only updating
  // the username and the length of the password.
  current_entries_[id].username = base::UTF16ToUTF8(username);
  SendSavedPasswordsList();
}

void TestPasswordsPrivateDelegate::RemoveSavedPassword(int id) {
  if (current_entries_.empty())
    return;

  // Since this is just mock data, remove the first entry regardless of
  // the data contained.
  last_deleted_entry_ = std::move(current_entries_.front());
  current_entries_.erase(current_entries_.begin());
  SendSavedPasswordsList();
}

void TestPasswordsPrivateDelegate::RemovePasswordException(int id) {
  // Since this is just mock data, remove the first entry regardless of
  // the data contained.
  last_deleted_exception_ = std::move(current_exceptions_.front());
  current_exceptions_.erase(current_exceptions_.begin());
  SendPasswordExceptionsList();
}

// Simplified version of undo logic, only use for testing.
void TestPasswordsPrivateDelegate::UndoRemoveSavedPasswordOrException() {
  if (last_deleted_entry_) {
    current_entries_.insert(current_entries_.begin(),
                            std::move(*last_deleted_entry_));
    last_deleted_entry_ = base::nullopt;
    SendSavedPasswordsList();
  } else if (last_deleted_exception_) {
    current_exceptions_.insert(current_exceptions_.begin(),
                               std::move(*last_deleted_exception_));
    last_deleted_exception_ = base::nullopt;
    SendPasswordExceptionsList();
  }
}

void TestPasswordsPrivateDelegate::RequestPlaintextPassword(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    content::WebContents* web_contents) {
  // Return a mocked password value.
  std::move(callback).Run(plaintext_password_);
}

void TestPasswordsPrivateDelegate::ImportPasswords(
    content::WebContents* web_contents) {
  // The testing of password importing itself should be handled via
  // |PasswordManagerPorter|.
  import_passwords_triggered_ = true;
}

void TestPasswordsPrivateDelegate::ExportPasswords(
    base::OnceCallback<void(const std::string&)> callback,
    content::WebContents* web_contents) {
  // The testing of password exporting itself should be handled via
  // |PasswordManagerPorter|.
  export_passwords_triggered_ = true;
  std::move(callback).Run(std::string());
}

void TestPasswordsPrivateDelegate::CancelExportPasswords() {
  cancel_export_passwords_triggered_ = true;
}

api::passwords_private::ExportProgressStatus
TestPasswordsPrivateDelegate::GetExportProgressStatus() {
  // The testing of password exporting itself should be handled via
  // |PasswordManagerPorter|.
  return api::passwords_private::ExportProgressStatus::
      EXPORT_PROGRESS_STATUS_IN_PROGRESS;
}

bool TestPasswordsPrivateDelegate::IsOptedInForAccountStorage() {
  return is_opted_in_for_account_storage_;
}

void TestPasswordsPrivateDelegate::SetAccountStorageOptIn(
    bool opt_in,
    content::WebContents* web_contents) {
  is_opted_in_for_account_storage_ = opt_in;
}

std::vector<api::passwords_private::CompromisedCredential>
TestPasswordsPrivateDelegate::GetCompromisedCredentials() {
  api::passwords_private::CompromisedCredential credential;
  credential.username = "alice";
  credential.formatted_origin = "example.com";
  credential.detailed_origin = "https://example.com";
  credential.is_android_credential = false;
  credential.change_password_url =
      std::make_unique<std::string>("https://example.com/change-password");
  credential.compromise_type = api::passwords_private::COMPROMISE_TYPE_LEAKED;
  credential.compromise_time = 1583236800000;  // Mar 03 2020 12:00:00 UTC
  credential.elapsed_time_since_compromise = base::UTF16ToUTF8(
      TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG,
                         base::TimeDelta::FromDays(3)));

  std::vector<api::passwords_private::CompromisedCredential> credentials;
  credentials.push_back(std::move(credential));
  return credentials;
}

void TestPasswordsPrivateDelegate::GetPlaintextCompromisedPassword(
    api::passwords_private::CompromisedCredential credential,
    api::passwords_private::PlaintextReason reason,
    content::WebContents* web_contents,
    PlaintextCompromisedPasswordCallback callback) {
  // Return a mocked password value.
  if (!plaintext_password_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  credential.password =
      std::make_unique<std::string>(base::UTF16ToUTF8(*plaintext_password_));
  std::move(callback).Run(std::move(credential));
}

// Fake implementation of ChangeCompromisedCredential. This succeeds if the
// delegate knows of a compromised credential with the same id.
bool TestPasswordsPrivateDelegate::ChangeCompromisedCredential(
    const api::passwords_private::CompromisedCredential& credential,
    base::StringPiece new_password) {
  return std::any_of(compromised_credentials_.begin(),
                     compromised_credentials_.end(),
                     [&credential](const auto& compromised_credential) {
                       return compromised_credential.id == credential.id;
                     });
}

// Fake implementation of RemoveCompromisedCredential. This succeeds if the
// delegate knows of a compromised credential with the same id.
bool TestPasswordsPrivateDelegate::RemoveCompromisedCredential(
    const api::passwords_private::CompromisedCredential& credential) {
  return base::EraseIf(compromised_credentials_,
                       [&credential](const auto& compromised_credential) {
                         return compromised_credential.id == credential.id;
                       }) != 0;
}

void TestPasswordsPrivateDelegate::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  start_password_check_triggered_ = true;
  std::move(callback).Run(start_password_check_state_);
}

void TestPasswordsPrivateDelegate::StopPasswordCheck() {
  stop_password_check_triggered_ = true;
}

api::passwords_private::PasswordCheckStatus
TestPasswordsPrivateDelegate::GetPasswordCheckStatus() {
  api::passwords_private::PasswordCheckStatus status;
  status.state = api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;
  status.already_processed = std::make_unique<int>(5);
  status.remaining_in_queue = std::make_unique<int>(10);
  status.elapsed_time_since_last_check =
      std::make_unique<std::string>(base::UTF16ToUTF8(TimeFormat::Simple(
          TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT,
          base::TimeDelta::FromMinutes(5))));
  return status;
}

void TestPasswordsPrivateDelegate::SetProfile(Profile* profile) {
  profile_ = profile;
}

void TestPasswordsPrivateDelegate::SetOptedInForAccountStorage(bool opted_in) {
  is_opted_in_for_account_storage_ = opted_in;
}

void TestPasswordsPrivateDelegate::AddCompromisedCredential(int id) {
  api::passwords_private::CompromisedCredential cred;
  cred.id = id;
  compromised_credentials_.push_back(std::move(cred));
}

void TestPasswordsPrivateDelegate::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

void TestPasswordsPrivateDelegate::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

}  // namespace extensions
