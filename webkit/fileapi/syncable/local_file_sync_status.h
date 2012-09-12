// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_STATUS_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_STATUS_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "webkit/fileapi/file_system_url.h"

namespace fileapi {

// Represents local file sync status.
// This class is thread-safe and fields of this class are protected by a lock.
// Owned by FileSystemContext.
//
// This class manages two important synchronization flags: writing (counter)
// and syncing (flag).  Writing counter keeps track of which URL is in
// writing and syncing flag indicates which URL is in syncing.
//
// An invariant of this class is: no FileSystem objects should be both
// in syncing_ and writing_ status, i.e. trying to increment writing
// while the target url is in syncing must fail and vice versa.
class FILEAPI_EXPORT LocalFileSyncStatus {
 public:
  LocalFileSyncStatus();
  ~LocalFileSyncStatus();

  // Tries to increment writing counter for |url|.
  // This fails if the target |url| is in syncing.
  bool TryIncrementWriting(const FileSystemURL& url);

  // Decrement writing counter for |url|.
  void DecrementWriting(const FileSystemURL& url);

  // Tries to mark syncing flag for |url| to enable writing.
  // This fails if the target |url| is in writing.
  bool TryDisableWriting(const FileSystemURL& url);

  // Clears the syncing flag for |url| to disable writing.
  void EnableWriting(const FileSystemURL& url);

  // Returns true if the |url| or its parent or child is in writing.
  bool IsWriting(const FileSystemURL& url) const;

  // Returns true if the |url| is enabled for writing (i.e. not in syncing).
  bool IsWritable(const FileSystemURL& url) const;

 private:
  typedef std::map<FileSystemURL, int64, FileSystemURL::Comparator> URLCountMap;
  typedef std::set<FileSystemURL, FileSystemURL::Comparator> URLSet;

  // These private methods must be called with the lock_ held.
  bool IsChildOrParentWriting(const FileSystemURL& url) const;
  bool IsChildOrParentSyncing(const FileSystemURL& url) const;

  mutable base::Lock lock_;

  // If this count is non-zero positive there're ongoing write operations.
  URLCountMap writing_;

  // If this flag is set sync process is running on the file.
  URLSet syncing_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSyncStatus);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_STATUS_H_
