// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "sync/api/entity_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/activation_context.h"

namespace syncer {
class DataTypeErrorHandler;
class SyncError;
}  // namespace syncer

namespace syncer_v2 {

class MetadataBatch;
class MetadataChangeList;

// Interface used by the ModelTypeService to inform sync of local
// changes.
class SYNC_EXPORT ModelTypeChangeProcessor : public syncer::SyncErrorFactory {
 public:
  typedef base::Callback<void(syncer::SyncError,
                              std::unique_ptr<ActivationContext>)>
      StartCallback;

  ModelTypeChangeProcessor();
  ~ModelTypeChangeProcessor() override;

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
  virtual void OnMetadataLoaded(syncer::SyncError error,
                                std::unique_ptr<MetadataBatch> batch) = 0;

  // Indicates that sync wants to connect a sync worker to this processor. Once
  // the processor has metadata from the service, it will pass the info needed
  // for the worker into |callback|. |error_handler| is how the processor will
  // inform sync of any unrecoverable errors after calling |callback|, and it is
  // guaranteed to outlive the processor. StartCallback takes a SyncError and an
  // ActivationContext; the context should be nullptr iff the error is set.
  virtual void OnSyncStarting(syncer::DataTypeErrorHandler* error_handler,
                              const StartCallback& callback) = 0;

  // Indicates that sync is being disabled permanently for this data type. All
  // metadata should be erased from storage.
  virtual void DisableSync() = 0;
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_CHANGE_PROCESSOR_H_
