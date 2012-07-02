// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/syncable_mock.h"

#include "base/location.h"
#include "sync/syncable/in_memory_directory_backing_store.h"

#include "sync/test/null_transaction_observer.h"

namespace syncer {
namespace syncable {

MockDirectory::MockDirectory(syncer::UnrecoverableErrorHandler* handler)
    : Directory(&encryptor_, handler, NULL,
                new syncable::InMemoryDirectoryBackingStore("store")) {
  InitKernelForTest("myk", &delegate_, syncable::NullTransactionObserver());
}

MockDirectory::~MockDirectory() {}

MockSyncableWriteTransaction::MockSyncableWriteTransaction(
    const tracked_objects::Location& from_here, Directory *directory)
    : WriteTransaction(from_here, syncable::UNITTEST, directory) {
}

}  // namespace syncable
}  // namespace syncer
