// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/authpolicy/data_pipe_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/kerberos/dbus-constants.h"

namespace chromeos {

namespace {

KerberosCredentialsManager* g_instance = nullptr;

// Default encryption with strong encryption.
constexpr char kDefaultKerberosConfigFmt[] = R"(
[libdefaults]
  default_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
  default_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
  permitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
  default_realm = %s
)";

// If |principal_name| is "UsEr@realm.com", sets |principal_name| to
// "user@REALM.COM". Returns false if the given name has no @ or one of the
// parts is empty.
bool NormalizePrincipal(std::string* principal_name) {
  std::vector<std::string> parts = base::SplitString(
      *principal_name, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2 || parts.at(0).empty() || parts.at(1).empty())
    return false;
  *principal_name =
      base::ToLowerASCII(parts[0]) + "@" + base::ToUpperASCII(parts[1]);
  return true;
}

// Tries to normalize |principal_name|. Runs |callback| with
// |ERROR_PARSE_PRINCIPAL_FAILED| if not possible.
bool NormalizePrincipalOrPostCallback(
    std::string* principal_name,
    KerberosCredentialsManager::ResultCallback* callback) {
  if (NormalizePrincipal(principal_name))
    return true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback),
                                kerberos::ERROR_PARSE_PRINCIPAL_FAILED));
  return false;
}

// If |normalized_principal| is "user@REALM.COM", returns "REALM.COM".
// DCHECKs valid format.
std::string GetRealm(const std::string& normalized_principal) {
  std::vector<std::string> parts = base::SplitString(
      normalized_principal, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(parts.size() == 2);
  DCHECK(!parts[1].empty());
  return parts[1];
}

// Logs an error if |error| is not |ERROR_NONE|.
void LogError(const char* function_name, kerberos::ErrorType error) {
  LOG_IF(ERROR, error != kerberos::ERROR_NONE)
      << function_name << " failed with error code " << error;
}

// Returns true if |error| is |ERROR_NONE|.
bool Succeeded(kerberos::ErrorType error) {
  return error == kerberos::ERROR_NONE;
}

}  // namespace

// Encapsulates the steps to add a Kerberos account. Overview of the flow:
// - Start up the daemon.
// - Call daemon's AddAccount. Handle errors duplicate errors transparently,
//   it's OK if the account is already present.
// - If the account is new, call daemon's SetConfig with a default config.
// - Call daemon's AcquireKerberosTgt.
// - Call manager's OnAddAccountRunnerDone.
// If an error happens on any step, removes account if it was added and calls
// OnAddAccountRunnerDone with the error.
class KerberosAddAccountRunner {
 public:
  // Kicks off the flow to add (or re-authenticate) a Kerberos account.
  // |manager| is a non-owned pointer to the owning manager.
  // |normalized_principal| is the normalized user principal name, e.g.
  // user@REALM.COM. |password| is the password of the account. |callback| is
  // called by OnAddAccountRunnerDone() at the end of the flow, see class
  // description.
  KerberosAddAccountRunner(KerberosCredentialsManager* manager,
                           const std::string& normalized_principal,
                           const std::string& password,
                           KerberosCredentialsManager::ResultCallback callback)
      : manager_(manager),
        normalized_principal_(normalized_principal),
        password_(password),
        callback_(std::move(callback)) {
    AddAccount();
  }

 private:
  // Adds the |normalized_principal_| account to the Kerberos daemon.
  void AddAccount() {
    kerberos::AddAccountRequest request;
    request.set_principal_name(normalized_principal_);
    KerberosClient::Get()->AddAccount(
        request, base::BindOnce(&KerberosAddAccountRunner::OnAddAccount,
                                weak_factory_.GetWeakPtr()));
  }

  // If the account was added (new account), calls SetDefaultConfig().
  // Otherwise, continues with AcquireKerberosTgt().
  void OnAddAccount(const kerberos::AddAccountResponse& response) {
    // New account, set a default config.
    if (response.error() == kerberos::ERROR_NONE) {
      was_added_ = true;
      SetDefaultConfig();
      return;
    }

    if (response.error() == kerberos::ERROR_DUPLICATE_PRINCIPAL_NAME) {
      AcquireKerberosTgt();
      return;
    }

    // In all other cases, respond with error (shouldn't happen right now).
    NOTREACHED();
    Done(response.error());
  }

  // Sets a default Kerberos configuration for |normalized_principal_|.
  void SetDefaultConfig() {
    const std::string realm = GetRealm(normalized_principal_);
    const std::string krb5_conf =
        base::StringPrintf(kDefaultKerberosConfigFmt, realm.c_str());

    kerberos::SetConfigRequest request;
    request.set_principal_name(normalized_principal_);
    request.set_krb5conf(krb5_conf);
    KerberosClient::Get()->SetConfig(
        request, base::BindOnce(&KerberosAddAccountRunner::OnSetConfig,
                                weak_factory_.GetWeakPtr()));
  }

  // Calls AcquireKerberosTgt() if no error occurred or Done() otherwise.
  void OnSetConfig(const kerberos::SetConfigResponse& response) {
    if (response.error() == kerberos::ERROR_NONE) {
      AcquireKerberosTgt();
      return;
    }

    Done(response.error());
  }

  // Authenticates |normalized_principal_| using |password_|.
  void AcquireKerberosTgt() {
    kerberos::AcquireKerberosTgtRequest request;
    request.set_principal_name(normalized_principal_);
    KerberosClient::Get()->AcquireKerberosTgt(
        request, data_pipe_utils::GetDataReadPipe(password_).get(),
        base::BindOnce(&KerberosAddAccountRunner::OnAcquireKerberosTgt,
                       weak_factory_.GetWeakPtr()));
  }

  // Forwards to Done().
  void OnAcquireKerberosTgt(
      const kerberos::AcquireKerberosTgtResponse& response) {
    // We're ready.
    Done(response.error());
  }

  // Calls back into |manager_|'s OnAddAccountRunnerDone().
  void Done(kerberos::ErrorType error) {
    if (was_added_ && error != kerberos::ERROR_NONE) {
      // Do a best effort cleaning up the account we added before.
      kerberos::RemoveAccountRequest request;
      request.set_principal_name(normalized_principal_);
      KerberosClient::Get()->RemoveAccount(
          request, base::BindOnce(&KerberosAddAccountRunner::OnRemoveAccount,
                                  weak_factory_.GetWeakPtr(), error));
    } else {
      // We're done. This call will delete us!
      manager_->OnAddAccountRunnerDone(this, std::move(normalized_principal_),
                                       std::move(callback_), error);
    }
  }

  // Prints out a warning if |removal_error| is an error case and forwards
  // |original_error| to Done().
  void OnRemoveAccount(kerberos::ErrorType original_error,
                       const kerberos::RemoveAccountResponse& response) {
    if (response.error() != kerberos::ERROR_NONE) {
      LOG(WARNING) << "Failed to remove Kerberos account for "
                   << normalized_principal_;
    }

    // We're done. This call will delete us! Note that we're passing the
    // |original_error| here, not the |response.error()|.
    manager_->OnAddAccountRunnerDone(this, std::move(normalized_principal_),
                                     std::move(callback_), original_error);
  }

  // Pointer to the owning manager, not owned.
  KerberosCredentialsManager* const manager_;
  std::string normalized_principal_;
  std::string password_;
  KerberosCredentialsManager::ResultCallback callback_;

  // Whether the account was newly added.
  bool was_added_ = false;

  base::WeakPtrFactory<KerberosAddAccountRunner> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(KerberosAddAccountRunner);
};

KerberosCredentialsManager::Observer::Observer() = default;

KerberosCredentialsManager::Observer::~Observer() = default;

KerberosCredentialsManager::KerberosCredentialsManager(PrefService* local_state)
    : local_state_(local_state),
      kerberos_files_handler_(
          base::BindRepeating(&KerberosCredentialsManager::GetKerberosFiles,
                              base::Unretained(this))) {
  DCHECK(!g_instance);
  g_instance = this;

  // Connect to a signal that indicates when Kerberos files change.
  // TODO(https://crbug.com/963824): Make sure no code inside this constructor
  // causes the daemon to start.
  KerberosClient::Get()->ConnectToKerberosFileChangedSignal(
      base::BindRepeating(&KerberosCredentialsManager::OnKerberosFilesChanged,
                          weak_factory_.GetWeakPtr()));

  // Listen to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state);
  pref_change_registrar_->Add(
      prefs::kKerberosEnabled,
      base::BindRepeating(&KerberosCredentialsManager::UpdateEnabledFromPref,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kKerberosRememberPasswordEnabled,
      base::BindRepeating(
          &KerberosCredentialsManager::UpdateRememberPasswordEnabledFromPref,
          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kKerberosAddAccountsAllowed,
      base::BindRepeating(
          &KerberosCredentialsManager::UpdateAddAccountsAllowedFromPref,
          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kKerberosAccounts,
      base::BindRepeating(&KerberosCredentialsManager::UpdateAccountsFromPref,
                          weak_factory_.GetWeakPtr()));
}

KerberosCredentialsManager::~KerberosCredentialsManager() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

// static
KerberosCredentialsManager& KerberosCredentialsManager::Get() {
  DCHECK(g_instance);
  return *g_instance;
}

// static
void KerberosCredentialsManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKerberosEnabled, false);
  registry->RegisterBooleanPref(prefs::kKerberosRememberPasswordEnabled, true);
  registry->RegisterBooleanPref(prefs::kKerberosAddAccountsAllowed, true);
  registry->RegisterDictionaryPref(prefs::kKerberosAccounts);
}

void KerberosCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void KerberosCredentialsManager::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

void KerberosCredentialsManager::AddAccountAndAuthenticate(
    std::string principal_name,
    const std::string& password,
    ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  // If there's a call in progress, respond with ERROR_IN_PROGRESS.
  if (add_account_runner_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), kerberos::ERROR_IN_PROGRESS));
    return;
  }

  add_account_runner_ = std::make_unique<KerberosAddAccountRunner>(
      this, principal_name, password, std::move(callback));
  // The runner starts automatically and calls OnAddAccountRunnerDone when it's
  // done.
}

void KerberosCredentialsManager::OnAddAccountRunnerDone(
    KerberosAddAccountRunner* runner,
    std::string normalized_principal,
    ResultCallback callback,
    kerberos::ErrorType error) {
  // Reset the instance.
  DCHECK(runner == add_account_runner_.get());
  add_account_runner_.reset();

  LogError("AddAccountAndAuthenticate", error);
  if (Succeeded(error)) {
    // Set active account.
    // TODO(https://crbug.com/948121): Wait until the files have been saved.
    // This is important when this code is triggered directly through a page
    // that requires Kerberos auth.
    active_principal_name_ = normalized_principal;
    GetKerberosFiles();

    // Bring the merry news to the observers.
    NotifyAccountsChanged();
  }

  std::move(callback).Run(error);
}

void KerberosCredentialsManager::RemoveAccount(std::string principal_name,
                                               ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  kerberos::RemoveAccountRequest request;
  request.set_principal_name(principal_name);
  KerberosClient::Get()->RemoveAccount(
      request, base::BindOnce(&KerberosCredentialsManager::OnRemoveAccount,
                              weak_factory_.GetWeakPtr(), principal_name,
                              std::move(callback)));
}

void KerberosCredentialsManager::OnRemoveAccount(
    const std::string& principal_name,
    ResultCallback callback,
    const kerberos::RemoveAccountResponse& response) {
  LogError("RemoveAccount", response.error());
  if (Succeeded(response.error())) {
    // Clear out active credentials.
    if (active_principal_name_ == principal_name) {
      kerberos_files_handler_.DeleteFiles();
      active_principal_name_.clear();
    }

    // Express our condolence to the observers.
    NotifyAccountsChanged();
  }

  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::ClearAccounts(ResultCallback callback) {
  kerberos::ClearAccountsRequest request;
  KerberosClient::Get()->ClearAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnClearAccounts,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnClearAccounts(
    ResultCallback callback,
    const kerberos::ClearAccountsResponse& response) {
  LogError("ClearAccounts", response.error());
  if (Succeeded(response.error())) {
    // Clear out active credentials.
    kerberos_files_handler_.DeleteFiles();
    active_principal_name_.clear();

    // Tattle on the lost accounts to the observers.
    NotifyAccountsChanged();
  }

  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::ListAccounts(ListAccountsCallback callback) {
  kerberos::ListAccountsRequest request;
  KerberosClient::Get()->ListAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnListAccounts,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnListAccounts(
    ListAccountsCallback callback,
    const kerberos::ListAccountsResponse& response) {
  LogError("ListAccounts", response.error());
  std::move(callback).Run(response);
}

kerberos::ErrorType KerberosCredentialsManager::SetActiveAccount(
    std::string principal_name) {
  if (!NormalizePrincipal(&principal_name))
    return kerberos::ERROR_PARSE_PRINCIPAL_FAILED;

  // Don't early out if names are equal, this might be required to bootstrap
  // Kerberos credentials.
  active_principal_name_ = principal_name;
  GetKerberosFiles();
  return kerberos::ERROR_NONE;
}

void KerberosCredentialsManager::SetConfig(std::string principal_name,
                                           const std::string& krb5_conf,
                                           ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  kerberos::SetConfigRequest request;
  request.set_principal_name(principal_name);
  request.set_krb5conf(krb5_conf);
  KerberosClient::Get()->SetConfig(
      request, base::BindOnce(&KerberosCredentialsManager::OnSetConfig,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnSetConfig(
    ResultCallback callback,
    const kerberos::SetConfigResponse& response) {
  LogError("SetConfig", response.error());
  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::AcquireKerberosTgt(std::string principal_name,
                                                    const std::string& password,
                                                    ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  kerberos::AcquireKerberosTgtRequest request;
  request.set_principal_name(principal_name);
  KerberosClient::Get()->AcquireKerberosTgt(
      request, data_pipe_utils::GetDataReadPipe(password).get(),
      base::BindOnce(&KerberosCredentialsManager::OnAcquireKerberosTgt,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnAcquireKerberosTgt(
    ResultCallback callback,
    const kerberos::AcquireKerberosTgtResponse& response) {
  LogError("AcquireKerberosTgt", response.error());
  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::GetKerberosFiles() {
  if (active_principal_name_.empty())
    return;

  kerberos::GetKerberosFilesRequest request;
  request.set_principal_name(active_principal_name_);
  KerberosClient::Get()->GetKerberosFiles(
      request,
      base::BindOnce(&KerberosCredentialsManager::OnGetKerberosFiles,
                     weak_factory_.GetWeakPtr(), request.principal_name()));
}

void KerberosCredentialsManager::OnGetKerberosFiles(
    const std::string& principal_name,
    const kerberos::GetKerberosFilesResponse& response) {
  LogError("GetKerberosFiles", response.error());
  if (!Succeeded(response.error()))
    return;

  // Ignore if the principal changed in the meantime.
  if (active_principal_name_ != principal_name) {
    VLOG(1) << "Ignoring Kerberos files. Active principal changed from "
            << principal_name << " to " << active_principal_name_;
    return;
  }

  auto nullstr = base::Optional<std::string>();
  kerberos_files_handler_.SetFiles(
      response.files().has_krb5cc() ? response.files().krb5cc() : nullstr,
      response.files().has_krb5conf() ? response.files().krb5conf() : nullstr);
}

void KerberosCredentialsManager::OnKerberosFilesChanged(
    const std::string& principal_name) {
  // Only listen to the active account.
  if (principal_name == active_principal_name_)
    GetKerberosFiles();
}

void KerberosCredentialsManager::NotifyAccountsChanged() {
  for (auto& observer : observers_)
    observer.OnAccountsChanged();
}

void KerberosCredentialsManager::UpdateEnabledFromPref() {
  const bool enabled = local_state_->GetBoolean(prefs::kKerberosEnabled);
  if (!enabled) {
    // Note that ClearAccounts logs an error if the operation fails.
    ClearAccounts(base::BindOnce([](kerberos::ErrorType) {}));
  }
}

void KerberosCredentialsManager::UpdateRememberPasswordEnabledFromPref() {
  // TODO(https://crbug.com/952239): Implement
}

void KerberosCredentialsManager::UpdateAddAccountsAllowedFromPref() {
  // TODO(https://crbug.com/952239): Implement
}

void KerberosCredentialsManager::UpdateAccountsFromPref() {
  // TODO(https://crbug.com/952239): Implement
}

}  // namespace chromeos
