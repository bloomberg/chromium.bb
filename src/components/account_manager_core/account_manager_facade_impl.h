// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace account_manager {

class AccountManager;

// ChromeOS-specific implementation of |AccountManagerFacade| that talks to
// |account_manager::AccountManager| over Mojo. Used by both Lacros and Ash.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacadeImpl
    : public AccountManagerFacade,
      public crosapi::mojom::AccountManagerObserver {
 public:
  // Constructs `AccountManagerFacadeImpl`.
  // `account_manager_remote` is a Mojo `Remote` to Account Manager in Ash -
  // either in-process or out-of-process.
  // `remote_version` is the Mojo API version of the remote.
  // `init_finished` is called after `this` has been fully initialized.
  AccountManagerFacadeImpl(
      mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
      uint32_t remote_version,
      AccountManager* account_manager_for_tests,
      base::OnceClosure init_finished = base::DoNothing());
  AccountManagerFacadeImpl(const AccountManagerFacadeImpl&) = delete;
  AccountManagerFacadeImpl& operator=(const AccountManagerFacadeImpl&) = delete;
  ~AccountManagerFacadeImpl() override;

  // AccountManagerFacade overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetAccounts(
      base::OnceCallback<void(const std::vector<Account>&)> callback) override;
  void GetPersistentErrorForAccount(
      const AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback)
      override;
  void ShowAddAccountDialog(AccountAdditionSource source) override;
  void ShowAddAccountDialog(
      AccountAdditionSource source,
      base::OnceCallback<void(const account_manager::AccountAdditionResult&
                                  result)> callback) override;
  void ShowReauthAccountDialog(AccountAdditionSource source,
                               const std::string& email) override;
  void ShowManageAccountsSettings() override;
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const AccountKey& account,
      const std::string& oauth_consumer_name,
      OAuth2AccessTokenConsumer* consumer) override;
  void UpsertAccountForTesting(const Account& account,
                               const std::string& token_value) override;
  void RemoveAccountForTesting(const AccountKey& account) override;

  // crosapi::mojom::AccountManagerObserver overrides:
  void OnTokenUpserted(crosapi::mojom::AccountPtr account) override;
  void OnAccountRemoved(crosapi::mojom::AccountPtr account) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowAddAccountDialogCallsMojo);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowAddAccountDialogUMA);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowReauthAccountDialogCallsMojo);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      ShowAddAccountDialogSetsCorrectOptionsForAdditionFromAsh);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      ShowAddAccountDialogSetsCorrectOptionsForAdditionFromLacros);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      ShowAddAccountDialogSetsCorrectOptionsForAdditionFromArc);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowReauthAccountDialogUMA);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowManageAccountsSettingsCallsMojo);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           InitializationStatusIsCorrectlySet);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      AccessTokenFetcherCanBeCreatedBeforeAccountManagerFacadeInitialization);
  static std::string GetAccountAdditionResultStatusHistogramNameForTesting();

  // A utility class to fetch access tokens over Mojo.
  class AccessTokenFetcher;

  void OnReceiverReceived(
      mojo::PendingReceiver<AccountManagerObserver> receiver);
  // Callback for `crosapi::mojom::AccountManager::ShowAddAccountDialog`.
  void OnShowAddAccountDialogFinished(
      base::OnceCallback<
          void(const account_manager::AccountAdditionResult& result)> callback,
      crosapi::mojom::AccountAdditionResultPtr mojo_result);
  void FinishAddAccount(
      base::OnceCallback<
          void(const account_manager::AccountAdditionResult& result)> callback,
      const account_manager::AccountAdditionResult& result);

  void GetAccountsInternal(
      base::OnceCallback<void(const std::vector<Account>&)> callback);

  void GetPersistentErrorInternal(
      const AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback);

  // Proxy method to call `CreateAccessTokenFetcher` on
  // `account_manager_remote_`. Returns `true` if `account_manager_remote_` is
  // bound and the call was queued successfully.
  bool CreateAccessTokenFetcher(
      crosapi::mojom::AccountKeyPtr account_key,
      const std::string& oauth_consumer_name,
      crosapi::mojom::AccountManager::CreateAccessTokenFetcherCallback
          callback);

  // The initialization sequence for `AccountManagerFacadeImpl` consists of
  // adding an observer to the remote.
  //
  // Remote-querying methods like `GetAccounts` won't actually produce a remote
  // call until the initialization sequence is finished (instead, they will be
  // queued in `initialization_callbacks_`).
  //
  // `FinishInitSequenceIfNotAlreadyFinished` invokes callbacks from
  // `initialization_callbacks_` and marks the initialization as finished.
  void FinishInitSequenceIfNotAlreadyFinished();
  // `closure` will be invoked after the initialization sequence is finished.
  // See `FinishInitSequenceIfNotAlreadyFinished` for details.
  void RunAfterInitializationSequence(base::OnceClosure closure);

  // Runs `closure` if/when `account_manager_remote_` gets disconnected.
  void RunOnMojoDisconnection(base::OnceClosure closure);
  // Mojo disconnect handler.
  void OnMojoError();

  bool IsInitialized();

  void FlushMojoForTesting();

  // Mojo API version on the remote (Ash) side.
  const uint32_t remote_version_;

  bool is_initialized_ = false;
  std::vector<base::OnceClosure> initialization_callbacks_;
  std::vector<base::OnceClosure> mojo_disconnection_handlers_;

  mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote_;
  std::unique_ptr<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>
      receiver_;

  base::ObserverList<Observer> observer_list_;

  AccountManager* account_manager_for_tests_ = nullptr;

  base::WeakPtrFactory<AccountManagerFacadeImpl> weak_factory_{this};
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
