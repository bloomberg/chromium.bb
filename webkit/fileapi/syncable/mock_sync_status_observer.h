// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_MOCK_SYNC_STATUS_OBSERVER_H_
#define WEBKIT_FILEAPI_SYNCABLE_MOCK_SYNC_STATUS_OBSERVER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"

namespace fileapi {

class MockSyncStatusObserver : public LocalFileSyncStatus::Observer {
 public:
  MockSyncStatusObserver();
  virtual ~MockSyncStatusObserver();

  // LocalFileSyncStatus::Observer overrides.
  MOCK_METHOD1(OnSyncEnabled, void(const FileSystemURL& url));
  MOCK_METHOD1(OnWriteEnabled, void(const FileSystemURL& url));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncStatusObserver);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_MOCK_SYNC_STATUS_OBSERVER_H_
