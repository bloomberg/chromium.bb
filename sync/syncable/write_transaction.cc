// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/write_transaction.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/syncable/transaction_observer.h"
#include "sync/syncable/write_transaction_info.h"

namespace syncable {

enum InvariantCheckLevel {
  OFF = 0,
  VERIFY_IN_MEMORY = 1,
  FULL_DB_VERIFICATION = 2
};

const InvariantCheckLevel kInvariantCheckLevel = VERIFY_IN_MEMORY;

WriteTransaction::WriteTransaction(const tracked_objects::Location& location,
                                   WriterTag writer, Directory* directory)
    : BaseTransaction(location, "WriteTransaction", writer, directory) {
  Lock();
}

void WriteTransaction::SaveOriginal(const EntryKernel* entry) {
  if (!entry) {
    return;
  }
  // Insert only if it's not already there.
  const int64 handle = entry->ref(META_HANDLE);
  EntryKernelMutationMap::iterator it = mutations_.lower_bound(handle);
  if (it == mutations_.end() || it->first != handle) {
    EntryKernelMutation mutation;
    mutation.original = *entry;
    ignore_result(mutations_.insert(it, std::make_pair(handle, mutation)));
  }
}

ImmutableEntryKernelMutationMap WriteTransaction::RecordMutations() {
  directory_->kernel_->transaction_mutex.AssertAcquired();
  for (syncable::EntryKernelMutationMap::iterator it = mutations_.begin();
       it != mutations_.end();) {
    EntryKernel* kernel = directory()->GetEntryByHandle(it->first);
    if (!kernel) {
      NOTREACHED();
      continue;
    }
    if (kernel->is_dirty()) {
      it->second.mutated = *kernel;
      ++it;
    } else {
      DCHECK(!it->second.original.is_dirty());
      // Not actually mutated, so erase from |mutations_|.
      mutations_.erase(it++);
    }
  }
  return ImmutableEntryKernelMutationMap(&mutations_);
}

void WriteTransaction::UnlockAndNotify(
    const ImmutableEntryKernelMutationMap& mutations) {
  // Work while transaction mutex is held.
  ModelTypeSet models_with_changes;
  bool has_mutations = !mutations.Get().empty();
  if (has_mutations) {
    models_with_changes = NotifyTransactionChangingAndEnding(mutations);
  }
  Unlock();

  // Work after mutex is relased.
  if (has_mutations) {
    NotifyTransactionComplete(models_with_changes);
  }
}

ModelTypeSet WriteTransaction::NotifyTransactionChangingAndEnding(
    const ImmutableEntryKernelMutationMap& mutations) {
  directory_->kernel_->transaction_mutex.AssertAcquired();
  DCHECK(!mutations.Get().empty());

  WriteTransactionInfo write_transaction_info(
      directory_->kernel_->next_write_transaction_id,
      from_here_, writer_, mutations);
  ++directory_->kernel_->next_write_transaction_id;

  ImmutableWriteTransactionInfo immutable_write_transaction_info(
      &write_transaction_info);
  DirectoryChangeDelegate* const delegate = directory_->kernel_->delegate;
  if (writer_ == syncable::SYNCAPI) {
    delegate->HandleCalculateChangesChangeEventFromSyncApi(
        immutable_write_transaction_info, this);
  } else {
    delegate->HandleCalculateChangesChangeEventFromSyncer(
        immutable_write_transaction_info, this);
  }

  ModelTypeSet models_with_changes =
      delegate->HandleTransactionEndingChangeEvent(
          immutable_write_transaction_info, this);

  directory_->kernel_->transaction_observer.Call(FROM_HERE,
      &TransactionObserver::OnTransactionWrite,
      immutable_write_transaction_info, models_with_changes);

  return models_with_changes;
}

void WriteTransaction::NotifyTransactionComplete(
    ModelTypeSet models_with_changes) {
  directory_->kernel_->delegate->HandleTransactionCompleteChangeEvent(
      models_with_changes);
}

WriteTransaction::~WriteTransaction() {
  const ImmutableEntryKernelMutationMap& mutations = RecordMutations();

  if (!unrecoverable_error_set_) {
    if (OFF != kInvariantCheckLevel) {
      const bool full_scan = (FULL_DB_VERIFICATION == kInvariantCheckLevel);
      if (full_scan)
        directory()->CheckTreeInvariants(this, full_scan);
      else
        directory()->CheckTreeInvariants(this, mutations.Get());
    }
  }

  // |CheckTreeInvariants| could have thrown an unrecoverable error.
  if (unrecoverable_error_set_) {
    HandleUnrecoverableErrorIfSet();
    Unlock();
    return;
  }

  UnlockAndNotify(mutations);
}

#define ENUM_CASE(x) case x: return #x; break

std::string WriterTagToString(WriterTag writer_tag) {
  switch (writer_tag) {
    ENUM_CASE(INVALID);
    ENUM_CASE(SYNCER);
    ENUM_CASE(AUTHWATCHER);
    ENUM_CASE(UNITTEST);
    ENUM_CASE(VACUUM_AFTER_SAVE);
    ENUM_CASE(PURGE_ENTRIES);
    ENUM_CASE(SYNCAPI);
  };
  NOTREACHED();
  return "";
}

#undef ENUM_CASE

}  // namespace syncable
