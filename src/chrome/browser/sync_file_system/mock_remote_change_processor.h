// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_CHANGE_PROCESSOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class MockRemoteChangeProcessor : public RemoteChangeProcessor {
 public:
  MockRemoteChangeProcessor();
  ~MockRemoteChangeProcessor() override;

  // RemoteChangeProcessor overrides.
  MOCK_METHOD2(PrepareForProcessRemoteChange,
               void(const storage::FileSystemURL& url,
                    const PrepareChangeCallback& callback));
  MOCK_METHOD4(ApplyRemoteChange,
               void(const FileChange& change,
                    const base::FilePath& local_path,
                    const storage::FileSystemURL& url,
                    const SyncStatusCallback& callback));
  MOCK_METHOD3(FinalizeRemoteSync,
               void(const storage::FileSystemURL& url,
                    bool clear_local_changes,
                    const base::Closure& completion_callback));
  MOCK_METHOD3(RecordFakeLocalChange,
               void(const storage::FileSystemURL& url,
                    const FileChange& change,
                    const SyncStatusCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRemoteChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_CHANGE_PROCESSOR_H_
