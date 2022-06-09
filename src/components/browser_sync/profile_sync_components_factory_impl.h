// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/version_info/version_info.h"

namespace syncer {
class ModelTypeController;
class ModelTypeControllerDelegate;
class SyncInvalidationsService;
class SyncService;
}  // namespace syncer

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStoreInterface;
}

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace browser_sync {

class BrowserSyncClient;

class ProfileSyncComponentsFactoryImpl
    : public syncer::SyncApiComponentFactory {
 public:
  ProfileSyncComponentsFactoryImpl(
      BrowserSyncClient* sync_client,
      version_info::Channel channel,
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<base::SequencedTaskRunner>& db_thread,
      const scoped_refptr<autofill::AutofillWebDataService>&
          web_data_service_on_disk,
      const scoped_refptr<autofill::AutofillWebDataService>&
          web_data_service_in_memory,
      const scoped_refptr<password_manager::PasswordStoreInterface>&
          profile_password_store,
      const scoped_refptr<password_manager::PasswordStoreInterface>&
          account_password_store,
      sync_bookmarks::BookmarkSyncService* bookmark_sync_service);
  ProfileSyncComponentsFactoryImpl(const ProfileSyncComponentsFactoryImpl&) =
      delete;
  ProfileSyncComponentsFactoryImpl& operator=(
      const ProfileSyncComponentsFactoryImpl&) = delete;
  ~ProfileSyncComponentsFactoryImpl() override;

  // Creates and returns enabled datatypes and their controllers.
  // |disabled_types| allows callers to prevent certain types from being
  // created (e.g. to honor command-line flags).
  syncer::DataTypeController::TypeVector CreateCommonDataTypeControllers(
      syncer::ModelTypeSet disabled_types,
      syncer::SyncService* sync_service);

  // SyncApiComponentFactory implementation:
  std::unique_ptr<syncer::DataTypeManager> CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const syncer::DataTypeController::TypeMap* controllers,
      const syncer::DataTypeEncryptionHandler* encryption_handler,
      syncer::ModelTypeConfigurer* configurer,
      syncer::DataTypeManagerObserver* observer) override;
  std::unique_ptr<syncer::SyncEngine> CreateSyncEngine(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      syncer::SyncInvalidationsService* sync_invalidation_service) override;
  void ClearAllTransportData() override;

 private:
  // Factory function for ModelTypeController instances for models living on
  // |ui_thread_|.
  std::unique_ptr<syncer::ModelTypeController>
  CreateModelTypeControllerForModelRunningOnUIThread(syncer::ModelType type);

  // Factory function for ModelTypeControllerDelegate instances for models
  // living in |ui_thread_| that have their delegate accessible via SyncClient.
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  CreateForwardingControllerDelegate(syncer::ModelType type);

  // Factory function for ModelTypeController instances for wallet-related
  // datatypes, which live in |db_thread_| and have a delegate accessible via
  // AutofillWebDataService.
  std::unique_ptr<syncer::ModelTypeController> CreateWalletModelTypeController(
      syncer::ModelType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::ModelTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data,
      syncer::SyncService* sync_service);
  // Same as above, but supporting STORAGE_IN_MEMORY implemented as an
  // independent AutofillWebDataService, namely |web_data_service_in_memory_|.
  std::unique_ptr<syncer::ModelTypeController>
  CreateWalletModelTypeControllerWithInMemorySupport(
      syncer::ModelType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::ModelTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data,
      syncer::SyncService* sync_service);

  // Client/platform specific members.
  const raw_ptr<BrowserSyncClient> sync_client_;
  const version_info::Channel channel_;
  const scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  const scoped_refptr<base::SequencedTaskRunner> db_thread_;
  const scoped_refptr<base::SequencedTaskRunner>
      engines_and_directory_deletion_thread_;
  const scoped_refptr<autofill::AutofillWebDataService>
      web_data_service_on_disk_;
  const scoped_refptr<autofill::AutofillWebDataService>
      web_data_service_in_memory_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;
  const raw_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
