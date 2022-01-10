// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_migration_decorator.h"

#include "base/bind.h"
#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_store_proxy_backend.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

// Time in seconds by which the passwords migration from the built-in backend to
// the Android backend is delayed.
constexpr int kMigrationToAndroidBackendDelay = 30;

}  // namespace

PasswordStoreBackendMigrationDecorator::PasswordStoreBackendMigrationDecorator(
    std::unique_ptr<PasswordStoreBackend> built_in_backend,
    std::unique_ptr<PasswordStoreBackend> android_backend,
    PrefService* prefs,
    base::RepeatingCallback<bool()> is_syncing_passwords_callback)
    : built_in_backend_(std::move(built_in_backend)),
      android_backend_(std::move(android_backend)),
      prefs_(prefs),
      is_syncing_passwords_callback_(std::move(is_syncing_passwords_callback)) {
  DCHECK(built_in_backend_);
  DCHECK(android_backend_);
  active_backend_ = std::make_unique<PasswordStoreProxyBackend>(
      built_in_backend_.get(), android_backend_.get(),
      is_syncing_passwords_callback_);
}

PasswordStoreBackendMigrationDecorator::
    ~PasswordStoreBackendMigrationDecorator() = default;

base::WeakPtr<PasswordStoreBackend>
PasswordStoreBackendMigrationDecorator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreBackendMigrationDecorator::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  active_backend_->InitBackend(std::move(remote_form_changes_received),
                               std::move(sync_enabled_or_disabled_cb),
                               std::move(completion));
  if (base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerMigration)) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PasswordStoreBackendMigrationDecorator::StartMigration,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(kMigrationToAndroidBackendDelay));
  }
}

void PasswordStoreBackendMigrationDecorator::Shutdown(
    base::OnceClosure shutdown_completed) {
  // Calling Shutdown() on active_backend_ will take care of calling
  // Shutdown() on relevant backends.
  active_backend_->Shutdown(
      base::BindOnce(
          [](std::unique_ptr<PasswordStoreBackend> built_in_backend,
             std::unique_ptr<PasswordStoreBackend> android_backend,
             std::unique_ptr<PasswordStoreBackend> combined_backend) {
            // All the backends must be destroyed only after |active_backend_|
            // signals that Shutdown is over. It can be done asynchronously and
            // after PasswordStoreBackendMigrationDecorator destruction.
            built_in_backend.reset();
            android_backend.reset();
            combined_backend.reset();
          },
          std::move(built_in_backend_), std::move(android_backend_),
          std::move(active_backend_))
          .Then(std::move(shutdown_completed)));
}

void PasswordStoreBackendMigrationDecorator::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend_->GetAllLoginsAsync(std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend_->GetAutofillableLoginsAsync(std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  active_backend_->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                           forms);
}

void PasswordStoreBackendMigrationDecorator::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  active_backend_->AddLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  active_backend_->UpdateLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  active_backend_->RemoveLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  active_backend_->RemoveLoginsByURLAndTimeAsync(
      url_filter, std::move(delete_begin), std::move(delete_end),
      std::move(sync_completion), std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  active_backend_->RemoveLoginsCreatedBetweenAsync(
      std::move(delete_begin), std::move(delete_end), std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  active_backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                                    std::move(completion));
}

SmartBubbleStatsStore*
PasswordStoreBackendMigrationDecorator::GetSmartBubbleStatsStore() {
  return active_backend_->GetSmartBubbleStatsStore();
}

FieldInfoStore* PasswordStoreBackendMigrationDecorator::GetFieldInfoStore() {
  return active_backend_->GetFieldInfoStore();
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreBackendMigrationDecorator::CreateSyncControllerDelegate() {
  if (base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerSyncUsingAndroidBackendOnly)) {
    // The android backend (PasswordStoreAndroidBackend) creates a controller
    // delegate that prevents sync from actually communicating with the sync
    // server using the built in SyncEngine.
    return android_backend_->CreateSyncControllerDelegate();
  }

  return built_in_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreBackendMigrationDecorator::StartMigration() {
  migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
      built_in_backend_.get(), android_backend_.get(), prefs_,
      is_syncing_passwords_callback_);
  migrator_->StartMigrationIfNecessary();
}

}  // namespace password_manager
