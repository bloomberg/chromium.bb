// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_SYNCABLE_SERVICE_H_
#define SYNC_API_SYNCABLE_SERVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_merge_result.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class SyncErrorFactory;

// TODO(zea): remove SupportsWeakPtr in favor of having all SyncableService
// implementers provide a way of getting a weak pointer to themselves.
// See crbug.com/100114.
class SYNC_EXPORT SyncableService
    : public SyncChangeProcessor,
      public base::SupportsWeakPtr<SyncableService> {
 public:
  // A StartSyncFlare is useful when your SyncableService has a need for sync
  // to start ASAP, typically because a local change event has occurred but
  // MergeDataAndStartSyncing hasn't been called yet, meaning you don't have a
  // SyncChangeProcessor. The sync subsystem will respond soon after invoking
  // Run() on your flare by calling MergeDataAndStartSyncing. The ModelType
  // parameter is included so that the recieving end can track usage and timing
  // statistics, make optimizations or tradeoffs by type, etc.
  typedef base::Callback<void(ModelType)> StartSyncFlare;

  // Informs the service to begin syncing the specified synced datatype |type|.
  // The service should then merge |initial_sync_data| into it's local data,
  // calling |sync_processor|'s ProcessSyncChanges as necessary to reconcile the
  // two. After this, the SyncableService's local data should match the server
  // data, and the service should be ready to receive and process any further
  // SyncChange's as they occur.
  // Returns: a SyncMergeResult whose error field reflects whether an error
  //          was encountered while merging the two models. The merge result
  //          may also contain optional merge statistics.
  virtual SyncMergeResult MergeDataAndStartSyncing(
      ModelType type,
      const SyncDataList& initial_sync_data,
      scoped_ptr<SyncChangeProcessor> sync_processor,
      scoped_ptr<SyncErrorFactory> error_handler) = 0;

  // Stop syncing the specified type and reset state.
  virtual void StopSyncing(ModelType type) = 0;

  // SyncChangeProcessor interface.
  // Process a list of new SyncChanges and update the local data as necessary.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE = 0;

  // Returns AttachmentStore used by datatype. Attachment store is used by sync
  // when uploading or downloading attachments.
  // GetAttachmentStore is called right before MergeDataAndStartSyncing. If at
  // that time GetAttachmentStore returns NULL then datatype is considered not
  // using attachments and all attempts to upload/download attachments will
  // fail. Default implementation returns NULL. Datatype that uses sync
  // attachemnts should create attachment store and implement GetAttachmentStore
  // to return pointer to it.
  virtual scoped_refptr<AttachmentStore> GetAttachmentStore();

 protected:
  virtual ~SyncableService();
};

}  // namespace syncer

#endif  // SYNC_API_SYNCABLE_SERVICE_H_
