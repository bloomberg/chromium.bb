// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_SYNC_CHANGE_PROCESSOR_H_
#define SYNC_API_SYNC_CHANGE_PROCESSOR_H_

#include <vector>

#include "sync/api/sync_error.h"
#include "sync/base/sync_export.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

class SyncChange;

typedef std::vector<SyncChange> SyncChangeList;

// An interface for services that handle receiving SyncChanges.
class SYNC_EXPORT SyncChangeProcessor {
 public:
  SyncChangeProcessor();
  virtual ~SyncChangeProcessor();

  // Process a list of SyncChanges.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  // Inputs:
  //   |from_here|: allows tracking of where sync changes originate.
  //   |change_list|: is the list of sync changes in need of processing.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) = 0;
};

}  // namespace syncer

#endif  // SYNC_API_SYNC_CHANGE_PROCESSOR_H_
