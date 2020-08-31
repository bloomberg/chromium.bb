// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_CHECK_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_CHECK_DELEGATE_H_

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class Profile;

namespace password_manager {
class PasswordStore;
}

namespace extensions {

extern const char kPasswordCheckDataKey[];

class PasswordCheckProgress;

// This class handles the part of the passwordsPrivate extension API that deals
// with the bulk password check feature.
class PasswordCheckDelegate
    : public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::CompromisedCredentialsProvider::Observer,
      public password_manager::BulkLeakCheckService::Observer {
 public:
  using StartPasswordCheckCallback =
      PasswordsPrivateDelegate::StartPasswordCheckCallback;

  using CredentialPasswordsMap =
      std::map<password_manager::CredentialWithPassword,
               std::vector<autofill::PasswordForm>,
               password_manager::PasswordCredentialLess>;

  explicit PasswordCheckDelegate(Profile* profile);
  PasswordCheckDelegate(const PasswordCheckDelegate&) = delete;
  PasswordCheckDelegate& operator=(const PasswordCheckDelegate&) = delete;
  ~PasswordCheckDelegate() override;

  // Obtains information about compromised credentials. This includes the last
  // time a check was run, as well as all compromised credentials that are
  // present in the password store.
  std::vector<api::passwords_private::CompromisedCredential>
  GetCompromisedCredentials();

  // Requests the plaintext password for |credential|. If successful, this
  // returns |credential| with its |password| member set. This can fail if no
  // matching compromised credential can be found in the password store.
  base::Optional<api::passwords_private::CompromisedCredential>
  GetPlaintextCompromisedPassword(
      api::passwords_private::CompromisedCredential credential) const;

  // Attempts to change the stored password of |credential| to |new_password|.
  // Returns whether the change succeeded.
  bool ChangeCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential,
      base::StringPiece new_password);

  // Attempts to remove |credential| from the password store. Returns whether
  // the remove succeeded.
  bool RemoveCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential);

  // Requests to start a check for compromised passwords. Invokes |callback|
  // once a check is running or the request was stopped via StopPasswordCheck().
  void StartPasswordCheck(
      StartPasswordCheckCallback callback = base::DoNothing());
  // Stops checking for compromised passwords.
  void StopPasswordCheck();

  // Returns the current status of the password check.
  api::passwords_private::PasswordCheckStatus GetPasswordCheckStatus() const;

 private:
  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords)
      override;

  // password_manager::CompromisedCredentialsProvider::Observer:
  // Invokes PasswordsPrivateEventRouter::OnCompromisedCredentialsChanged if
  // a valid pointer can be obtained.
  void OnCompromisedCredentialsChanged(
      password_manager::CompromisedCredentialsProvider::CredentialsView
          credentials) override;

  // password_manager::BulkLeakCheckService::Observer:
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  // Tries to find the matching CredentialWithPassword for |credential|. It
  // performs a look-up in |compromised_credential_id_generator_| using
  // |credential.id|. If a matching value exists it also verifies that signon
  // realm, username and when possible password match.
  // Returns a pointer to the matching CredentialWithPassword on success or
  // nullptr otherwise.
  const password_manager::CredentialWithPassword*
  FindMatchingCompromisedCredential(
      const api::passwords_private::CompromisedCredential& credential) const;

  // Tries to notify the PasswordsPrivateEventRouter that the password check
  // status has changed. Invoked after OnSavedPasswordsChanged and
  // OnStateChanged.
  void NotifyPasswordCheckStatusChanged();

  // Raw pointer to the underlying profile. Needs to outlive this instance.
  Profile* profile_ = nullptr;

  // Handle to the password store, powering both |saved_passwords_presenter_|
  // and |compromised_credentials_provider_|.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // Used by |compromised_credentials_provider_| to obtain the list of saved
  // passwords.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Used to obtain the list of compromised credentials.
  password_manager::CompromisedCredentialsProvider
      compromised_credentials_provider_;

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_;

  // Boolean that remembers whether the delegate is initialized. This is done
  // when the delegate obtains the list of saved passwords for the first time.
  bool is_initialized_ = false;

  // List of callbacks that were passed to StartPasswordCheck() prior to the
  // delegate being initialized. These will be run when either initialization
  // finishes, or StopPasswordCheck() gets invoked before hand.
  std::vector<StartPasswordCheckCallback> start_check_callbacks_;

  // Remembers the progress of the ongoing check. Null if no check is currently
  // running.
  base::WeakPtr<PasswordCheckProgress> password_check_progress_;

  // Remembers whether a password check is running right now.
  bool is_check_running_ = false;

  // A scoped observer for |saved_passwords_presenter_|.
  ScopedObserver<password_manager::SavedPasswordsPresenter,
                 password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for |compromised_credentials_provider_|.
  ScopedObserver<password_manager::CompromisedCredentialsProvider,
                 password_manager::CompromisedCredentialsProvider::Observer>
      observed_compromised_credentials_provider_{this};

  // A scoped observer for the BulkLeakCheckService.
  ScopedObserver<password_manager::BulkLeakCheckService,
                 password_manager::BulkLeakCheckService::Observer>
      observed_bulk_leak_check_service_{this};

  // A map that matches CredentialWithPasswords to corresponding PasswordForms.
  // This is required to inject affiliation information into Android
  // credentials, as well as being able to reflect edits and removals of
  // compromised credentials in the underlying password store.
  CredentialPasswordsMap credentials_to_forms_;

  // An id generator for compromised credentials. Required to match
  // api::passwords_private::CompromisedCredential instances passed to the UI
  // with the underlying CredentialWithPassword they are based on.
  IdGenerator<password_manager::CredentialWithPassword,
              int,
              password_manager::PasswordCredentialLess>
      compromised_credential_id_generator_;

  base::WeakPtrFactory<PasswordCheckDelegate> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_CHECK_DELEGATE_H_
