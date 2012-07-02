// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_WRITE_TRANSACTION_H_
#define SYNC_SYNCABLE_WRITE_TRANSACTION_H_
#pragma once

#include "sync/syncable/base_transaction.h"
#include "sync/syncable/entry_kernel.h"

namespace syncer {
namespace syncable {

// Locks db in constructor, unlocks in destructor.
class WriteTransaction : public BaseTransaction {
 public:
  WriteTransaction(const tracked_objects::Location& from_here,
                   WriterTag writer, Directory* directory);

  virtual ~WriteTransaction();

  void SaveOriginal(const EntryKernel* entry);

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

  // Only the original fields are filled in until |RecordMutations()|.
  // We use a mutation map instead of a kernel set to avoid copying.
  EntryKernelMutationMap mutations_;

  DISALLOW_COPY_AND_ASSIGN(WriteTransaction);
};

}  // namespace syncable
}  // namespace syncer

#endif //  SYNC_SYNCABLE_WRITE_TRANSACTION_H_
