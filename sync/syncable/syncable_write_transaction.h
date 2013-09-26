// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_WRITE_TRANSACTION_H_
#define SYNC_SYNCABLE_SYNCABLE_WRITE_TRANSACTION_H_

#include "sync/base/sync_export.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/syncable_base_write_transaction.h"

namespace syncer {
namespace syncable {

SYNC_EXPORT extern const int64 kInvalidTransactionVersion;

// Locks db in constructor, unlocks in destructor.
class SYNC_EXPORT WriteTransaction : public BaseWriteTransaction {
 public:
  WriteTransaction(const tracked_objects::Location& from_here,
                   WriterTag writer, Directory* directory);

  // Constructor used for getting back transaction version after making sync
  // API changes to one model. If model is changed by the transaction,
  // the new transaction version of the model and modified nodes will be saved
  // in |transaction_version| upon destruction of the transaction. If model is
  // not changed,  |transaction_version| will be kInvalidTransactionVersion.
  WriteTransaction(const tracked_objects::Location& from_here,
                   Directory* directory, int64* transaction_version);

  virtual ~WriteTransaction();

  virtual void TrackChangesTo(const EntryKernel* entry) OVERRIDE;

 protected:
  // Overridden by tests.
  virtual void NotifyTransactionComplete(ModelTypeSet models_with_changes);

 private:
  friend class MutableEntry;

  // Clears |mutations_|.
  ImmutableEntryKernelMutationMap RecordMutations();

  void UnlockAndNotify(const ImmutableEntryKernelMutationMap& mutations);

  ModelTypeSet NotifyTransactionChangingAndEnding(
      const ImmutableEntryKernelMutationMap& mutations);

  // Increment versions of the models whose entries are modified and set the
  // version on the changed entries.
  void UpdateTransactionVersion(const std::vector<int64>& entry_changed);

  // Only the original fields are filled in until |RecordMutations()|.
  // We use a mutation map instead of a kernel set to avoid copying.
  EntryKernelMutationMap mutations_;

  // Stores new transaction version of changed model and nodes if model is
  // indeed changed. kInvalidTransactionVersion otherwise. Not owned.
  int64* transaction_version_;

  DISALLOW_COPY_AND_ASSIGN(WriteTransaction);
};

}  // namespace syncable
}  // namespace syncer

#endif //  SYNC_SYNCABLE_SYNCABLE_WRITE_TRANSACTION_H_
