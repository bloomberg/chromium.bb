// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/syncable_model_neutral_write_transaction.h"

#include "sync/syncable/directory.h"

namespace syncer {
namespace syncable {

ModelNeutralWriteTransaction::ModelNeutralWriteTransaction(
    const tracked_objects::Location& location,
    WriterTag writer, Directory* directory)
    : BaseWriteTransaction(location,
                           "ModelNeutralWriteTransaction",
                           writer,
                           directory) {
  Lock();
}

ModelNeutralWriteTransaction::~ModelNeutralWriteTransaction() {
  directory()->CheckInvariantsOnTransactionClose(this, modified_handles_);
  HandleUnrecoverableErrorIfSet();
  Unlock();
}

void ModelNeutralWriteTransaction::TrackChangesTo(const EntryKernel* entry) {
  modified_handles_.insert(entry->ref(META_HANDLE));
}

}  // namespace syncer
}  // namespace syncable
