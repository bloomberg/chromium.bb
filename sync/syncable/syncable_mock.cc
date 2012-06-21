// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/syncable_mock.h"

#include "base/location.h"
#include "sync/test/null_transaction_observer.h"

MockDirectory::MockDirectory(csync::UnrecoverableErrorHandler* handler)
    : Directory(&encryptor_, handler, NULL) {
  InitKernelForTest("myk", &delegate_, syncable::NullTransactionObserver());
}

MockDirectory::~MockDirectory() {}

MockSyncableWriteTransaction::MockSyncableWriteTransaction(
    const tracked_objects::Location& from_here, Directory *directory)
    : WriteTransaction(from_here, syncable::UNITTEST, directory) {
}
