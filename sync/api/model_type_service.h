// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_SERVICE_H_
#define SYNC_API_MODEL_TYPE_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "sync/api/entity_change.h"
#include "sync/api/entity_data.h"
#include "sync/api/model_type_change_processor.h"
#include "sync/base/sync_export.h"

namespace syncer {
class SyncError;
}  // namespace syncer

namespace syncer_v2 {

class DataBatch;
class MetadataBatch;
class MetadataChangeList;

// Interface implemented by model types to receive updates from sync via the
// SharedModelTypeProcessor. Provides a way for sync to update the data and
// metadata for entities, as well as the model type state.
class SYNC_EXPORT ModelTypeService {
 public:
  typedef base::Callback<void(syncer::SyncError, scoped_ptr<DataBatch>)>
      DataCallback;
  typedef base::Callback<void(syncer::SyncError, scoped_ptr<MetadataBatch>)>
      MetadataCallback;
  typedef std::vector<std::string> ClientKeyList;

  ModelTypeService();

  virtual ~ModelTypeService();

  // Creates an object used to communicate changes in the sync metadata to the
  // model type store.
  virtual scoped_ptr<MetadataChangeList> CreateMetadataChangeList() = 0;

  // Perform the initial merge of data from the sync server. Should only need
  // to be called when sync is first turned on, not on every restart.
  virtual syncer::SyncError MergeSyncData(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityDataList entity_data_list) = 0;

  // Apply changes from the sync server locally.
  virtual syncer::SyncError ApplySyncChanges(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) = 0;

  // Asynchronously retrieve the sync metadata.
  virtual void LoadMetadata(MetadataCallback callback) = 0;

  // Asynchronously retrieve the corresponding sync data for |client_keys|.
  virtual void GetData(ClientKeyList client_keys, DataCallback callback) = 0;

  // Asynchronously retrieve all of the local sync data.
  virtual void GetAllData(DataCallback callback) = 0;

  // Get or generate a client tag for |entity_data|.
  virtual std::string GetClientTag(const EntityData& entity_data) = 0;

  // TODO(skym): See crbug/547087, do we need all these accessors?
  syncer_v2::ModelTypeChangeProcessor* change_processor();

  void set_change_processor(
      scoped_ptr<syncer_v2::ModelTypeChangeProcessor> change_processor);

  void clear_change_processor();

 private:
  // Recieves ownership in set_change_processor(...).
  scoped_ptr<syncer_v2::ModelTypeChangeProcessor> change_processor_;
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_SERVICE_H_
