// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_SYNCABLE_SERVICE_H_
#define SYNC_API_SYNCABLE_SERVICE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/syncable/model_type.h"

class SyncErrorFactory;

typedef std::vector<SyncData> SyncDataList;

// TODO(zea): remove SupportsWeakPtr in favor of having all SyncableService
// implementers provide a way of getting a weak pointer to themselves.
// See crbug.com/100114.
class SyncableService : public SyncChangeProcessor,
                        public base::SupportsWeakPtr<SyncableService> {
 public:
  // Informs the service to begin syncing the specified synced datatype |type|.
  // The service should then merge |initial_sync_data| into it's local data,
  // calling |sync_processor|'s ProcessSyncChanges as necessary to reconcile the
  // two. After this, the SyncableService's local data should match the server
  // data, and the service should be ready to receive and process any further
  // SyncChange's as they occur.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      scoped_ptr<SyncChangeProcessor> sync_processor,
      scoped_ptr<SyncErrorFactory> error_handler) = 0;

  // Stop syncing the specified type and reset state.
  virtual void StopSyncing(syncable::ModelType type) = 0;

  // Fills a list of SyncData from the local data. This should create an up
  // to date representation of the SyncableService's view of that datatype, and
  // should match/be a subset of the server's view of that datatype.
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const = 0;

  // SyncChangeProcessor interface.
  // Process a list of new SyncChanges and update the local data as necessary.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE = 0;

 protected:
  virtual ~SyncableService();
};

#endif  // SYNC_API_SYNCABLE_SERVICE_H_
