// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database_async_helper.h"

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

namespace {

constexpr base::TimeDelta kSyncTaskTimeout = base::Seconds(30);

}  // namespace

LoginDatabaseAsyncHelper::LoginDatabaseAsyncHelper(
    std::unique_ptr<LoginDatabase> login_db,
    std::unique_ptr<UnsyncedCredentialsDeletionNotifier> notifier,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : login_db_(std::move(login_db)),
      deletion_notifier_(std::move(notifier)),
      main_task_runner_(std::move(main_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(login_db_);
  DCHECK(main_task_runner_);
}

LoginDatabaseAsyncHelper::~LoginDatabaseAsyncHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool LoginDatabaseAsyncHelper::Initialize(
    PasswordStoreBackend::RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_forms_changes_received_callback_ =
      std::move(remote_form_changes_received);

  bool success = true;
  if (!login_db_->Init()) {
    login_db_.reset();
    // The initialization should be continued, because PasswordSyncBridge
    // has to be initialized even if database initialization failed.
    success = false;
    LOG(ERROR) << "Could not create/open login database.";
  }
  if (success) {
    login_db_->SetDeletionsHaveSyncedCallback(base::BindRepeating(
        &LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced,
        weak_ptr_factory_.GetWeakPtr()));

    // Delay the actual reporting by 30 seconds, to ensure it doesn't happen
    // during the "hot phase" of Chrome startup.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LoginDatabaseAsyncHelper::ReportMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }

  sync_bridge_ = std::make_unique<PasswordSyncBridge>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::PASSWORDS, base::DoNothing()),
      static_cast<PasswordStoreSync*>(this),
      std::move(sync_enabled_or_disabled_cb));

  return success;
}

LoginsResult LoginDatabaseAsyncHelper::GetAllLogins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrimaryKeyToFormMap key_to_form_map;

  if (!login_db_)
    return {};
  FormRetrievalResult result = login_db_->GetAllLogins(&key_to_form_map);
  if (result != FormRetrievalResult::kSuccess &&
      result != FormRetrievalResult::kEncryptionServiceFailureWithPartialData)
    return {};

  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  obtained_forms.reserve(key_to_form_map.size());
  for (auto& pair : key_to_form_map) {
    obtained_forms.push_back(std::move(pair.second));
  }
  return obtained_forms;
}

LoginsResult LoginDatabaseAsyncHelper::GetAutofillableLogins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  if (!login_db_ || !login_db_->GetAutofillableLogins(&results))
    return {};
  return results;
}

LoginsResult LoginDatabaseAsyncHelper::FillMatchingLogins(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  for (const auto& form : forms) {
    std::vector<std::unique_ptr<PasswordForm>> matched_forms;
    if (login_db_ && !login_db_->GetLogins(form, include_psl, &matched_forms))
      continue;
    results.insert(results.end(),
                   std::make_move_iterator(matched_forms.begin()),
                   std::make_move_iterator(matched_forms.end()));
  }
  return results;
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::AddLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes = AddLoginSync(form, /*error=*/nullptr);
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::UpdateLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes = UpdateLoginSync(form, /*error=*/nullptr);
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::RemoveLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form, &changes)) {
    if (sync_bridge_ && !changes.empty())
      sync_bridge_->ActOnPasswordStoreChanges(changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginsCreatedBetween(
                       delete_begin, delete_end, &changes)) {
    if (sync_bridge_ && !changes.empty())
      sync_bridge_->ActOnPasswordStoreChanges(changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::RemoveLoginsByURLAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                      &key_to_form_map)) {
    for (const auto& pair : key_to_form_map) {
      PasswordForm* form = pair.second.get();
      PasswordStoreChangeList remove_changes;
      if (url_filter.Run(form->url) &&
          login_db_->RemoveLogin(*form, &remove_changes)) {
        std::move(remove_changes.begin(), remove_changes.end(),
                  std::back_inserter(changes));
      }
    }
  }
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();

  if (sync_completion) {
    deletions_have_synced_callbacks_.push_back(std::move(sync_completion));
    // Start a timeout for sync, or restart it if it was already running.
    deletions_have_synced_timeout_.Reset(
        base::BindOnce(&LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*success=*/false));
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, deletions_have_synced_timeout_.callback(), kSyncTaskTimeout);

    // Do an immediate check for the case where there are already no unsynced
    // deletions.
    if (!GetMetadataStore()->HasUnsyncedDeletions())
      NotifyDeletionsHaveSynced(/*success=*/true);
  }
  return changes;
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->GetAutoSignInLogins(&key_to_form_map))
    return changes;

  std::set<GURL> origins_to_update;
  for (const auto& pair : key_to_form_map) {
    if (origin_filter.Run(pair.second->url))
      origins_to_update.insert(pair.second->url);
  }

  std::set<GURL> origins_updated;
  for (const GURL& origin : origins_to_update) {
    if (login_db_->DisableAutoSignInForOrigin(origin))
      origins_updated.insert(origin);
  }

  for (const auto& pair : key_to_form_map) {
    if (origins_updated.count(pair.second->url)) {
      changes.emplace_back(PasswordStoreChange::UPDATE, *pair.second,
                           FormPrimaryKey(pair.first));
    }
  }
  return changes;
}

// Synchronous implementation for manipulating with statistics.
void LoginDatabaseAsyncHelper::AddSiteStats(const InteractionsStats& stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void LoginDatabaseAsyncHelper::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<InteractionsStats> LoginDatabaseAsyncHelper::GetSiteStats(
    const GURL& origin_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<InteractionsStats>();
}

void LoginDatabaseAsyncHelper::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    login_db_->stats_table().RemoveStatsByOriginAndTime(
        origin_filter, delete_begin, delete_end);
  }
}

// Synchronous implementation for manipulating with field info.
void LoginDatabaseAsyncHelper::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->field_info_table().AddRow(field_info);
}

std::vector<FieldInfo> LoginDatabaseAsyncHelper::GetAllFieldInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ ? login_db_->field_info_table().GetAllRows()
                   : std::vector<FieldInfo>();
}

void LoginDatabaseAsyncHelper::RemoveFieldInfoByTime(base::Time remove_begin,
                                                     base::Time remove_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->field_info_table().RemoveRowsByTime(remove_begin, remove_end);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
LoginDatabaseAsyncHelper::GetSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_bridge_->change_processor()->GetControllerDelegate();
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::AddLoginSync(
    const PasswordForm& form,
    AddLoginError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    if (error) {
      *error = AddLoginError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->AddLogin(form, error);
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::UpdateLoginSync(
    const PasswordForm& form,
    UpdateLoginError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    if (error) {
      *error = UpdateLoginError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->UpdateLogin(form, error);
}

void LoginDatabaseAsyncHelper::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_forms_changes_received_callback_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(remote_forms_changes_received_callback_, changes));
}

void LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Either all deletions have been committed to the Sync server, or Sync is
  // telling us that it won't commit them (because Sync was turned off
  // permanently). In either case, run the corresponding callbacks now (on the
  // main task runner).
  DCHECK(!success || !GetMetadataStore()->HasUnsyncedDeletions());
  for (auto& callback : deletions_have_synced_callbacks_) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), success));
  }
  deletions_have_synced_timeout_.Cancel();
  deletions_have_synced_callbacks_.clear();
}

void LoginDatabaseAsyncHelper::NotifyUnsyncedCredentialsWillBeDeleted(
    std::vector<PasswordForm> unsynced_credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAccountStore());
  // |deletion_notifier_| only gets set for desktop.
  if (deletion_notifier_) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UnsyncedCredentialsDeletionNotifier::Notify,
                                  deletion_notifier_->GetWeakPtr(),
                                  std::move(unsynced_credentials)));
  }
}

bool LoginDatabaseAsyncHelper::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    return login_db_->BeginTransaction();
  return false;
}

void LoginDatabaseAsyncHelper::RollbackTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->RollbackTransaction();
}

bool LoginDatabaseAsyncHelper::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    return login_db_->CommitTransaction();
  return false;
}

FormRetrievalResult LoginDatabaseAsyncHelper::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_)
    return FormRetrievalResult::kDbError;
  return login_db_->GetAllLogins(key_to_form_map);
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::RemoveLoginByPrimaryKeySync(
    FormPrimaryKey primary_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginByPrimaryKey(primary_key, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* LoginDatabaseAsyncHelper::GetMetadataStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_.get();
}

bool LoginDatabaseAsyncHelper::IsAccountStore() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ && login_db_->is_account_store();
}

bool LoginDatabaseAsyncHelper::DeleteAndRecreateDatabaseFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ && login_db_->DeleteAndRecreateDatabaseFile();
}

DatabaseCleanupResult LoginDatabaseAsyncHelper::DeleteUndecryptableLogins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_)
    return DatabaseCleanupResult::kDatabaseUnavailable;
  return login_db_->DeleteUndecryptableLogins();
}

// Reports password store metrics that aren't reported by the
// StoreMetricsReporter. Namely, metrics related to inaccessible passwords,
// and bubble statistics.
void LoginDatabaseAsyncHelper::ReportMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_)
    return;
  login_db_->ReportMetrics();
}

}  // namespace password_manager
