// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_MODEL_NEUTRAL_WRITE_TRANSACTION_H_
#define SYNC_SYNCABLE_SYNCABLE_MODEL_NEUTRAL_WRITE_TRANSACTION_H_

#include "sync/base/sync_export.h"
#include "sync/syncable/metahandle_set.h"
#include "sync/syncable/syncable_base_write_transaction.h"

namespace syncer {
namespace syncable {

// A transaction used to instantiate Entries or ModelNeutralMutableEntries.
//
// This allows it to be used when making changes to sync entity properties that
// do not need to be kept in sync with the associated native model.
//
// This class differs internally from WriteTransactions in that it does a less
// good job of tracking and reporting on changes to the entries modified within
// its scope.  This is because its changes do not need to be reported to the
// DirectoryChangeDelegate.
class SYNC_EXPORT_PRIVATE ModelNeutralWriteTransaction
    : public BaseWriteTransaction {
 public:
  ModelNeutralWriteTransaction(
      const tracked_objects::Location& location,
      WriterTag writer,
      Directory* directory);
  virtual ~ModelNeutralWriteTransaction();

  virtual void TrackChangesTo(const EntryKernel* entry) OVERRIDE;

 private:
  MetahandleSet modified_handles_;

  DISALLOW_COPY_AND_ASSIGN(ModelNeutralWriteTransaction);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_MODEL_NEUTRAL_WRITE_TRANSACTION_H_
