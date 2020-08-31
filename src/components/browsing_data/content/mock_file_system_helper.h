// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_FILE_SYSTEM_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_FILE_SYSTEM_HELPER_H_

#include <list>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/browsing_data/content/file_system_helper.h"

namespace content {
class BrowserContext;
}

namespace browsing_data {

// Mock for FileSystemHelper.
// Use AddFileSystemSamples() or add directly to response_ list, then call
// Notify().
class MockFileSystemHelper : public FileSystemHelper {
 public:
  explicit MockFileSystemHelper(content::BrowserContext* browser_context);

  // FileSystemHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteFileSystemOrigin(const url::Origin& origin) override;

  // Adds a specific filesystem.
  void AddFileSystem(const url::Origin& origin,
                     bool has_persistent,
                     bool has_temporary,
                     bool has_syncable,
                     int64_t size_persistent,
                     int64_t size_temporary,
                     int64_t size_syncable);

  // Adds some FilesystemInfo samples.
  void AddFileSystemSamples();

  // Notifies the callback.
  void Notify();

  // Marks all filesystems as existing.
  void Reset();

  // Returns true if all filesystemss since the last Reset() invocation were
  // deleted.
  bool AllDeleted();

  url::Origin last_deleted_origin_;

 private:
  ~MockFileSystemHelper() override;

  FetchCallback callback_;

  // Stores which filesystems exist.
  std::map<const std::string, bool> file_systems_;

  std::list<FileSystemInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockFileSystemHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_FILE_SYSTEM_HELPER_H_
