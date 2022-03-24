// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

constexpr base::TaskTraits kBackendTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

constexpr char kDefaultTrustedVaultServiceURL[] =
    "https://securitydomain-pa.googleapis.com/v1/";

GURL ExtractTrustedVaultServiceURLFromCommandLine() {
  std::string string_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTrustedVaultServiceURL);
  if (string_url.empty()) {
    // Command line switch is not specified or is not a valid ASCII string.
    return GURL(kDefaultTrustedVaultServiceURL);
  }
  return GURL(string_url);
}

class IdentityManagerObserver : public signin::IdentityManager::Observer {
 public:
  IdentityManagerObserver(
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      scoped_refptr<StandaloneTrustedVaultBackend> backend,
      const base::RepeatingClosure& notify_keys_changed_callback,
      signin::IdentityManager* identity_manager);
  IdentityManagerObserver(const IdentityManagerObserver& other) = delete;
  IdentityManagerObserver& operator=(const IdentityManagerObserver& other) =
      delete;
  ~IdentityManagerObserver() override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnAccountsCookieDeletedByUserAction() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnRefreshTokensLoaded() override;

 private:
  void UpdatePrimaryAccountIfNeeded();
  void UpdateAccountsInCookieJarInfoIfNeeded(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info);

  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  const scoped_refptr<StandaloneTrustedVaultBackend> backend_;
  const base::RepeatingClosure notify_keys_changed_callback_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  CoreAccountInfo primary_account_;
};

IdentityManagerObserver::IdentityManagerObserver(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<StandaloneTrustedVaultBackend> backend,
    const base::RepeatingClosure& notify_keys_changed_callback,
    signin::IdentityManager* identity_manager)
    : backend_task_runner_(backend_task_runner),
      backend_(backend),
      notify_keys_changed_callback_(notify_keys_changed_callback),
      identity_manager_(identity_manager) {
  DCHECK(backend_task_runner_);
  DCHECK(backend_);
  DCHECK(identity_manager_);

  identity_manager_->AddObserver(this);
  if (identity_manager_->AreRefreshTokensLoaded()) {
    OnRefreshTokensLoaded();
  }
}

IdentityManagerObserver::~IdentityManagerObserver() {
  identity_manager_->RemoveObserver(this);
}

void IdentityManagerObserver::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdatePrimaryAccountIfNeeded();
}

void IdentityManagerObserver::OnAccountsCookieDeletedByUserAction() {
  // TODO(crbug.com/1148328): remove this handler once tests can mimic
  // OnAccountInCookieUpdated() properly.
  UpdateAccountsInCookieJarInfoIfNeeded(
      signin::AccountsInCookieJarInfo(/*accounts_are_fresh_param=*/true,
                                      /*signed_in_accounts_param=*/{},
                                      /*signed_out_accounts_param=*/{}));
  notify_keys_changed_callback_.Run();
}

void IdentityManagerObserver::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateAccountsInCookieJarInfoIfNeeded(accounts_in_cookie_jar_info);
  notify_keys_changed_callback_.Run();
}

void IdentityManagerObserver::OnRefreshTokensLoaded() {
  UpdatePrimaryAccountIfNeeded();
  UpdateAccountsInCookieJarInfoIfNeeded(
      identity_manager_->GetAccountsInCookieJar());
}

void IdentityManagerObserver::UpdatePrimaryAccountIfNeeded() {
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // Defer setting primary account until refresh tokens are loaded (otherwise,
    // |has_persistent_auth_error| can't be determined correctly).
    return;
  }
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account == primary_account_) {
    return;
  }
  primary_account_ = primary_account;

  // IdentityManager returns empty CoreAccountInfo if there is no primary
  // account.
  absl::optional<CoreAccountInfo> optional_primary_account;
  bool has_persistent_auth_error = false;
  if (!primary_account_.IsEmpty()) {
    optional_primary_account = primary_account_;
    has_persistent_auth_error =
        identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account_.account_id);
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::SetPrimaryAccount,
                     backend_, optional_primary_account,
                     has_persistent_auth_error));
}

void IdentityManagerObserver::UpdateAccountsInCookieJarInfoIfNeeded(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // Defer propagation of changes into backend until refresh tokens are
    // loaded, since primary account setting is deferred too. Otherwise,
    // deferred deletion of primary account keys won't work.
    return;
  }
  if (accounts_in_cookie_jar_info.accounts_are_fresh) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &StandaloneTrustedVaultBackend::UpdateAccountsInCookieJarInfo,
            backend_, accounts_in_cookie_jar_info));
  }
}

// Backend delegate that dispatches delegate notifications to custom callbacks,
// used to post notifications from the backend sequence to the UI thread.
class BackendDelegate : public StandaloneTrustedVaultBackend::Delegate {
 public:
  explicit BackendDelegate(
      const base::RepeatingClosure& notify_recoverability_degraded_cb)
      : notify_recoverability_degraded_cb_(notify_recoverability_degraded_cb) {}

  ~BackendDelegate() override = default;

  // StandaloneTrustedVaultBackend::Delegate implementation.
  void NotifyRecoverabilityDegradedChanged() override {
    notify_recoverability_degraded_cb_.Run();
  }

 private:
  const base::RepeatingClosure notify_recoverability_degraded_cb_;
};

}  // namespace

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    const base::FilePath& file_path,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : backend_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBackendTaskTraits)),
      access_token_fetcher_frontend_(identity_manager) {
  std::unique_ptr<TrustedVaultConnection> connection;
  GURL trusted_vault_service_gurl =
      ExtractTrustedVaultServiceURLFromCommandLine();
  if (base::FeatureList::IsEnabled(
          switches::kSyncTrustedVaultPassphraseRecovery) &&
      trusted_vault_service_gurl.is_valid()) {
    connection = std::make_unique<TrustedVaultConnectionImpl>(
        trusted_vault_service_gurl, url_loader_factory->Clone(),
        std::make_unique<TrustedVaultAccessTokenFetcherImpl>(
            access_token_fetcher_frontend_.GetWeakPtr()));
  }

  backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      file_path,
      std::make_unique<
          BackendDelegate>(BindToCurrentSequence(base::BindRepeating(
          &StandaloneTrustedVaultClient::NotifyRecoverabilityDegradedChanged,
          weak_ptr_factory_.GetWeakPtr()))),
      std::move(connection));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     backend_));
  // Using base::Unretained() is safe here, because |identity_manager_observer_|
  // owned by |this|.
  identity_manager_observer_ = std::make_unique<IdentityManagerObserver>(
      backend_task_runner_, backend_,
      base::BindRepeating(
          &StandaloneTrustedVaultClient::NotifyTrustedVaultKeysChanged,
          base::Unretained(this)),
      identity_manager);
}

StandaloneTrustedVaultClient::~StandaloneTrustedVaultClient() {
  // |backend_| needs to be destroyed inside backend sequence, not the current
  // one. Destroy |identity_manager_observer_| that owns pointer to |backend_|
  // as well and release |backend_| in |backend_task_runner_|.
  identity_manager_observer_.reset();
  backend_task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));
}

void StandaloneTrustedVaultClient::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void StandaloneTrustedVaultClient::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void StandaloneTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::FetchKeys, backend_,
                     account_info, BindToCurrentSequence(std::move(cb))));
}

void StandaloneTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StandaloneTrustedVaultBackend::StoreKeys,
                                backend_, gaia_id, keys, last_key_version));
  NotifyTrustedVaultKeysChanged();
}

void StandaloneTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::MarkLocalKeysAsStale,
                     backend_, account_info),
      std::move(cb));
}

void StandaloneTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetIsRecoverabilityDegraded, backend_,
          account_info, BindToCurrentSequence(std::move(cb))));
}

void StandaloneTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::AddTrustedRecoveryMethod,
                     backend_, gaia_id, public_key, method_type_hint,
                     BindToCurrentSequence(std::move(cb))));
}

void StandaloneTrustedVaultClient::ClearDataForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ClearDataForAccount,
                     backend_, account_info));
}

void StandaloneTrustedVaultClient::WaitForFlushForTesting(
    base::OnceClosure cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(cb));
}

void StandaloneTrustedVaultClient::FetchBackendPrimaryAccountForTesting(
    base::OnceCallback<void(const absl::optional<CoreAccountInfo>&)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting,
          backend_),
      std::move(cb));
}

void StandaloneTrustedVaultClient::
    GetLastAddedRecoveryMethodPublicKeyForTesting(
        base::OnceCallback<void(const std::vector<uint8_t>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::
                         GetLastAddedRecoveryMethodPublicKeyForTesting,
                     backend_),
      std::move(callback));
}

void StandaloneTrustedVaultClient::NotifyTrustedVaultKeysChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void StandaloneTrustedVaultClient::NotifyRecoverabilityDegradedChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

}  // namespace syncer
