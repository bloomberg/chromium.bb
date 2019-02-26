// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_SYNCABLE_SERVICE_BASED_BRIDGE_H_
#define COMPONENTS_SYNC_MODEL_IMPL_SYNCABLE_SERVICE_BASED_BRIDGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_change_processor.h"

namespace sync_pb {
class EntitySpecifics;
class PersistedEntityData;
}  // namespace sync_pb

namespace syncer {

class MetadataBatch;
class ModelTypeChangeProcessor;
class SyncableService;

// Implementation of ModelTypeSyncBridge that allows integrating legacy
// datatypes that implement SyncableService. Internally, it uses a database to
// persist and mimic the legacy directory's behavior, but as opposed to the
// legacy directory, it's not exposed anywhere outside this bridge, and is
// considered an implementation detail.
class SyncableServiceBasedBridge : public ModelTypeSyncBridge {
 public:
  using InMemoryStore = std::map<std::string, sync_pb::PersistedEntityData>;

  // Used for passwords only.
  // TODO(crbug.com/856941): Remove when PASSWORDS are migrated to USS, which
  // will likely make this API unnecessary.
  class ModelCryptographer
      : public base::RefCountedThreadSafe<ModelCryptographer> {
   public:
    ModelCryptographer();

    virtual base::Optional<ModelError> Decrypt(
        sync_pb::EntitySpecifics* specifics) = 0;
    virtual base::Optional<ModelError> Encrypt(
        sync_pb::EntitySpecifics* specifics) = 0;

   protected:
    virtual ~ModelCryptographer();

   private:
    friend class base::RefCountedThreadSafe<ModelCryptographer>;

    DISALLOW_COPY_AND_ASSIGN(ModelCryptographer);
  };

  // Pointers must not be null and |syncable_service| must outlive this object.
  SyncableServiceBasedBridge(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<ModelTypeChangeProcessor> change_processor,
      SyncableService* syncable_service,
      scoped_refptr<ModelCryptographer> cryptographer = nullptr);
  ~SyncableServiceBasedBridge() override;

  // ModelTypeSyncBridge implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request) override;
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list) override;
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  ConflictResolution ResolveConflict(
      const EntityData& local_data,
      const EntityData& remote_data) const override;
  StopSyncResponse ApplyStopSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;
  size_t EstimateSyncOverheadMemoryUsage() const override;
  base::Optional<ModelError> ApplySyncChangesWithNewEncryptionRequirements(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;

  // For testing.
  static std::unique_ptr<SyncChangeProcessor>
  CreateLocalChangeProcessorForTesting(ModelType type,
                                       ModelTypeStore* store,
                                       InMemoryStore* in_memory_store,
                                       ModelTypeChangeProcessor* other);

 private:
  void OnStoreCreated(const base::Optional<ModelError>& error,
                      std::unique_ptr<ModelTypeStore> store);
  void OnReadAllDataForInit(std::unique_ptr<InMemoryStore> in_memory_store,
                            const base::Optional<ModelError>& error);
  void OnReadAllMetadataForInit(const base::Optional<ModelError>& error,
                                std::unique_ptr<MetadataBatch> metadata_batch);
  base::Optional<ModelError> MaybeStartSyncableService() WARN_UNUSED_RESULT;
  base::Optional<ModelError> StoreAndConvertRemoteChanges(
      std::unique_ptr<ModelTypeStore::WriteBatch> batch,
      EntityChangeList input_entity_change_list,
      SyncChangeList* output_sync_change_list) WARN_UNUSED_RESULT;
  void ReportErrorIfSet(const base::Optional<ModelError>& error);
  base::Optional<ModelError> ReencryptEverything(
      ModelTypeStore::WriteBatch* batch);
  base::Optional<ModelError> ApplySyncChangesWithBatch(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list,
      std::unique_ptr<ModelTypeStore::WriteBatch> batch);
  void RecordAssociationTime(base::TimeDelta time) const;

  const ModelType type_;
  SyncableService* const syncable_service_;
  const scoped_refptr<ModelCryptographer> cryptographer_;
  OnceModelTypeStoreFactory store_factory_;

  std::unique_ptr<ModelTypeStore> store_;
  bool syncable_service_started_;

  // In-memory copy of |store_|, needed for remote deletions, because we need to
  // provide specifics of the deleted entity to the SyncableService.
  InMemoryStore in_memory_store_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncableServiceBasedBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncableServiceBasedBridge);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_SYNCABLE_SERVICE_BASED_BRIDGE_H_
