// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_MOCK_H_
#define SYNC_SYNCABLE_SYNCABLE_MOCK_H_

#include <string>

#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/null_directory_change_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {

class MockDirectory : public Directory {
 public:
  explicit MockDirectory(UnrecoverableErrorHandler* handler);
  virtual ~MockDirectory();

  MOCK_METHOD1(GetEntryByHandle, syncable::EntryKernel*(int64));

  MOCK_METHOD2(set_last_downloadstamp, void(ModelType, int64));

  MOCK_METHOD1(GetEntryByClientTag,
               syncable::EntryKernel*(const std::string&));

  MOCK_METHOD2(PurgeEntriesWithTypeIn, bool(ModelTypeSet, ModelTypeSet));

 private:
  syncable::NullDirectoryChangeDelegate delegate_;
};

class MockSyncableWriteTransaction : public syncable::WriteTransaction {
 public:
  MockSyncableWriteTransaction(
      const tracked_objects::Location& from_here, Directory *directory);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_MOCK_H_
