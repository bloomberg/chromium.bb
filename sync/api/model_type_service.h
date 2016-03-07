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
#include "sync/api/sync_error.h"
#include "sync/base/sync_export.h"

namespace syncer_v2 {

class DataBatch;
class MetadataChangeList;

// Interface implemented by model types to receive updates from sync via the
// SharedModelTypeProcessor. Provides a way for sync to update the data and
// metadata for entities, as well as the model type state.
class SYNC_EXPORT ModelTypeService {
 public:
  typedef base::Callback<void(syncer::SyncError, scoped_ptr<DataBatch>)>
      DataCallback;
  typedef std::vector<std::string> ClientTagList;

  ModelTypeService();

  virtual ~ModelTypeService();

  // Creates an object used to communicate changes in the sync metadata to the
  // model type store.
  virtual scoped_ptr<MetadataChangeList> CreateMetadataChangeList() = 0;

  // Perform the initial merge between local and sync data. This should only be
  // called when a data type is first enabled to start syncing, and there is no
  // sync metadata. Best effort should be made to match local and sync data. The
  // keys in the |entity_data_map| will have been created via GetClientTag(...),
  // and if a local and sync data should match/merge but disagree on tags, the
  // service should use the sync data's tag. Any local pieces of data that are
  // not present in sync should immediately be Put(...) to the processor before
  // returning. The same MetadataChangeList that was passed into this function
  // can be passed to Put(...) calls. Delete(...) can also be called but should
  // not be needed for most model types. Durable storage writes, if not able to
  // combine all change atomically, should save the metadata after the data
  // changes, so that this merge will be re-driven by sync if is not completely
  // saved during the current run.
  virtual syncer::SyncError MergeSyncData(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityDataMap entity_data_map) = 0;

  // Apply changes from the sync server locally.
  // Please note that |entity_changes| might have fewer entries than
  // |metadata_change_list| in case when some of the data changes are filtered
  // out, or even be empty in case when a commit confirmation is processed and
  // only the metadata needs to persisted.
  virtual syncer::SyncError ApplySyncChanges(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) = 0;

  // Asynchronously retrieve the corresponding sync data for |client_tags|.
  virtual void GetData(ClientTagList client_tags, DataCallback callback) = 0;

  // Asynchronously retrieve all of the local sync data.
  virtual void GetAllData(DataCallback callback) = 0;

  // Get or generate a client tag for |entity_data|.
  virtual std::string GetClientTag(const EntityData& entity_data) = 0;

  // Overridable notification for when the processor is set. This is typically
  // when the service should start loading metadata and then subsequently giving
  // it to the processor.
  virtual void OnChangeProcessorSet() = 0;

  // TODO(skym): See crbug/547087, do we need all these accessors?
  ModelTypeChangeProcessor* change_processor() const;

  void set_change_processor(
      scoped_ptr<ModelTypeChangeProcessor> change_processor);

  void clear_change_processor();

 private:
  // Recieves ownership in set_change_processor(...).
  scoped_ptr<ModelTypeChangeProcessor> change_processor_;
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_SERVICE_H_
