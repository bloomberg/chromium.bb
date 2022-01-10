// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"

class PrefService;

namespace syncer {
class ProxyModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

using IsAccountStore = base::StrongAlias<class IsAccountStoreTag, bool>;

using metrics_util::GaiaPasswordHashChange;

class PasswordStoreConsumer;

// Partial, cross-platform implementation for storing form passwords.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// PasswordStoreSync is a hidden base class because only PasswordSyncBridge
// needs to access these methods.
class PasswordStore : public PasswordStoreInterface {
 public:
  // Used to notify that unsynced credentials are about to be deleted.
  class UnsyncedCredentialsDeletionNotifier {
   public:
    // Should be called from the UI thread.
    virtual void Notify(std::vector<PasswordForm>) = 0;
    virtual ~UnsyncedCredentialsDeletionNotifier() = default;
    virtual base::WeakPtr<UnsyncedCredentialsDeletionNotifier> GetWeakPtr() = 0;
  };

  explicit PasswordStore(std::unique_ptr<PasswordStoreBackend> backend);

  PasswordStore(const PasswordStore&) = delete;
  PasswordStore& operator=(const PasswordStore&) = delete;

  // Always call this too on the UI thread.
  // TODO(crbug.bom/1218413): Move initialization into the core interface, too.
  bool Init(
      PrefService* prefs,
      std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper,
      base::RepeatingClosure sync_enabled_or_disabled_cb = base::DoNothing());

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // PasswordStoreInterface:
  bool IsAbleToSavePasswords() const override;
  void AddLogin(const PasswordForm& form) override;
  void UpdateLogin(const PasswordForm& form) override;
  void UpdateLoginWithPrimaryKey(const PasswordForm& new_form,
                                 const PasswordForm& old_primary_key) override;
  void RemoveLogin(const PasswordForm& form) override;
  void RemoveLoginsByURLAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion,
      base::OnceCallback<void(bool)> sync_completion =
          base::NullCallback()) override;
  void RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> completion) override;
  void DisableAutoSignInForOrigins(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  void Unblocklist(const PasswordFormDigest& form_digest,
                   base::OnceClosure completion) override;
  void GetLogins(const PasswordFormDigest& form,
                 base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void GetAutofillableLogins(
      base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void GetAllLogins(base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void GetAllLoginsWithAffiliationAndBrandingInformation(
      base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  PasswordStoreBackend* GetBackendForTesting() override;

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;

  // Status of PasswordStore::Init().
  enum class InitStatus {
    // Initialization status is still not determined (init hasn't started or
    // finished yet).
    kUnknown,
    // Initialization is successfully finished.
    kSuccess,
    // There was an error during initialization and PasswordStore is not ready
    // to save or get passwords.
    // Removing passwords may still work.
    kFailure,
  };

  ~PasswordStore() override;

  // This member is called to perform the actual interaction with the storage.
  // TODO(crbug.com/1217071): Make private std::unique_ptr as soon as the
  // backend is passed into the store instead of it being the store(_impl).
  raw_ptr<PasswordStoreBackend> backend_ = nullptr;

 private:
  using InsecureCredentialsTask =
      base::OnceCallback<std::vector<InsecureCredential>()>;

  // Called on the main thread after initialization is completed.
  // |success| is true if initialization was successful. Sets the
  // |init_status_|.
  void OnInitCompleted(bool success);

  // Notifies observers that password store data may have been changed.
  void NotifyLoginsChangedOnMainSequence(PasswordStoreChangeList changes);

  // The following methods notify observers that the password store may have
  // been modified via NotifyLoginsChangedOnMainSequence(). Note that there is
  // no guarantee that the called method will actually modify the password store
  // data.
  void UnblocklistInternal(base::OnceClosure completion,
                           std::vector<std::unique_ptr<PasswordForm>> forms);

  // Retrieves and fills in affiliation and branding information for Android
  // credentials in |forms| and invokes |callback| with the result. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(LoginsReply callback,
                                               LoginsResult forms);

  // The local backend is a ref-counted type in tests because it still inherits
  // from PasswordStore and this would be a self reference. So, if `this` is an
  // instance of PasswordStoreImpl, this member is not used.
  //
  // The backend is injected via the public constructor, this member owns the
  // instance and deletes it by calling PasswordStoreBackend::Shutdown on it.
  std::unique_ptr<PasswordStoreBackend> backend_deleter_ = nullptr;

  // TaskRunner for tasks that run on the main sequence (usually the UI thread).
  // TODO(crbug.com/1217071): Move into backend_.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // The observers.
  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper_;

  raw_ptr<PrefService> prefs_ = nullptr;

  InitStatus init_status_ = InitStatus::kUnknown;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
