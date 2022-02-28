// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

class IOSChromePasswordCheckManager;
namespace {
class IOSChromePasswordCheckManagerProxy;
}

// Enum which represents possible states of Password Check on UI.
// It's created based on BulkLeakCheckService::State.
enum class PasswordCheckState {
  kCanceled,
  kIdle,
  kNoPasswords,
  kOffline,
  kOther,
  kQuotaLimit,
  kRunning,
  kSignedOut,
};

// This class handles the bulk password check feature.
class IOSChromePasswordCheckManager
    : public base::SupportsWeakPtr<IOSChromePasswordCheckManager>,
      public base::RefCounted<IOSChromePasswordCheckManager>,
      public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::InsecureCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void PasswordCheckStatusChanged(PasswordCheckState state) {}
    virtual void CompromisedCredentialsChanged(
        password_manager::InsecureCredentialsManager::CredentialsView
            credentials) {}
  };

  // Requests to start a check for compromised passwords.
  void StartPasswordCheck();

  // Stops checking for compromised passwords.
  void StopPasswordCheck();

  // Returns the current state of the password check.
  PasswordCheckState GetPasswordCheckState() const;

  // The elapsed time since the last full password check was performed.
  base::Time GetLastPasswordCheckTime() const;

  // Obtains all compromised credentials that are present in the password store.
  std::vector<password_manager::CredentialWithPassword>
  GetCompromisedCredentials() const;

  password_manager::SavedPasswordsPresenter::SavedPasswordsView
  GetAllCredentials() const;

  password_manager::SavedPasswordsPresenter::SavedPasswordsView
  GetSavedPasswordsFor(
      const password_manager::CredentialWithPassword& credential) const;

  // Edits |username| and |password| for |form| and its duplicates.
  bool EditPasswordForm(const password_manager::PasswordForm& form,
                        const std::u16string& new_username,
                        const std::u16string& new_password);

  // Adds new password credentials |form| to the store.
  bool AddPasswordForm(const password_manager::PasswordForm& form);

  // Edits password form using |insecure_credentials_manager_|.
  void EditCompromisedPasswordForm(const password_manager::PasswordForm& form,
                                   base::StringPiece password);

  // Deletes |form| and its duplicates.
  void DeletePasswordForm(const password_manager::PasswordForm& form);

  // Deletes compromised credentials which are related to |form|.
  void DeleteCompromisedPasswordForm(
      const password_manager::PasswordForm& form);

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  password_manager::SavedPasswordsPresenter* GetSavedPasswordsPresenter() {
    return &saved_passwords_presenter_;
  }

 private:
  friend class base::RefCounted<IOSChromePasswordCheckManager>;
  friend class IOSChromePasswordCheckManagerProxy;

  explicit IOSChromePasswordCheckManager(ChromeBrowserState* browser_state);
  ~IOSChromePasswordCheckManager() override;

  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords)
      override;

  // password_manager::InsecureCredentialsManager::Observer:
  void OnInsecureCredentialsChanged(
      password_manager::InsecureCredentialsManager::CredentialsView credentials)
      override;

  // password_manager::BulkLeakCheckServiceInterface::Observer:
  void OnStateChanged(
      password_manager::BulkLeakCheckServiceInterface::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  void NotifyPasswordCheckStatusChanged();

  // Remembers whether a password check is running right now.
  bool is_check_running_ = false;

  ChromeBrowserState* browser_state_ = nullptr;

  // Handle to the password store, powering both |saved_passwords_presenter_|
  // and |insecure_credentials_manager_|.
  scoped_refptr<password_manager::PasswordStoreInterface> password_store_;

  // Used by |insecure_credentials_manager_| to obtain the list of saved
  // passwords.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Used to obtain the list of compromised credentials.
  password_manager::InsecureCredentialsManager insecure_credentials_manager_;

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_;

  // Boolean that remembers whether the delegate is initialized. This is done
  // when the delegate obtains the list of saved passwords for the first time.
  bool is_initialized_ = false;

  // Boolean that indicate whether Password Check should be started right after
  // delegate is initialized.
  bool start_check_on_init_ = false;

  // Time when password check was started. Used to calculate delay in case
  // when password check run less than 3 seconds.
  base::Time start_time_;

  // A scoped observer for |saved_passwords_presenter_|.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for |insecure_credentials_manager_|.
  base::ScopedObservation<
      password_manager::InsecureCredentialsManager,
      password_manager::InsecureCredentialsManager::Observer>
      observed_insecure_credentials_manager_{this};

  // A scoped observer for the BulkLeakCheckService.
  base::ScopedObservation<
      password_manager::BulkLeakCheckServiceInterface,
      password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_bulk_leak_check_service_{this};

  // Observers to listen to password check changes.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<IOSChromePasswordCheckManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_
