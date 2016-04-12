// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "sync/api/entity_data.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/activation_context.h"

namespace syncer {
class SyncError;
}  // namespace syncer

namespace syncer_v2 {

class MetadataBatch;
class MetadataChangeList;

// Interface used by the ModelTypeService to inform sync of local
// changes.
class SYNC_EXPORT ModelTypeChangeProcessor {
 public:
  typedef base::Callback<void(syncer::SyncError,
                              std::unique_ptr<ActivationContext>)>
      StartCallback;

  ModelTypeChangeProcessor();
  virtual ~ModelTypeChangeProcessor();

  // Inform the processor of a new or updated entity. The |entity_data| param
  // does not need to be fully set, but it should at least have specifics and
  // non-unique name. The processor will fill in the rest if the service does
  // not have a reason to care.
  virtual void Put(const std::string& client_tag,
                   std::unique_ptr<EntityData> entity_data,
                   MetadataChangeList* metadata_change_list) = 0;

  // Inform the processor of a deleted entity.
  virtual void Delete(const std::string& client_tag,
                      MetadataChangeList* metadata_change_list) = 0;

  // Accept the initial sync metadata loaded by the service. This should be
  // called as soon as the metadata is available to the service.
  virtual void OnMetadataLoaded(std::unique_ptr<MetadataBatch> batch) = 0;

  // Called by the DataTypeController to gather additional information needed
  // before a CommitQueue object can be created for this model type. Once the
  // metadata has been loaded, the info is collected and given to |callback|.
  // Once called, this can only be called again if sync is disconnected.
  virtual void OnSyncStarting(const StartCallback& callback) = 0;

  // Indicates that sync is being disabled permanently for this data type. All
  // metadata should be erased from storage.
  virtual void DisableSync() = 0;
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
