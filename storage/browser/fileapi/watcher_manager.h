// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_WATCHER_MANAGER_H_
#define STORAGE_BROWSER_FILEAPI_WATCHER_MANAGER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file.h"

namespace base {
class Time;
}

namespace storage {

class FileSystemOperationContext;
class FileSystemURL;

// An interface for providing entry observing capability for file system
// backends.
//
// It is NOT valid to give null callback to this class, and implementors
// can assume that they don't get any null callbacks.
class WatcherManager {
 public:
  enum Action { CHANGED, REMOVED };

  typedef base::Callback<void(base::File::Error result)> StatusCallback;
  typedef base::Callback<void(Action action)> NotificationCallback;

  virtual ~WatcherManager() {}

  // Observes a directory entry. If the |recursive| mode is not supported then
  // FILE_ERROR_INVALID_OPERATION must be returned as an error. If the |url| is
  // already watched, or setting up the watcher fails, then |callback|
  // must be called with a specific error code. Otherwise |callback| must be
  // called with the FILE_OK error code. |notification_callback| is called for
  // every change related to the watched directory.
  virtual void WatchDirectory(
      const FileSystemURL& url,
      bool recursive,
      const StatusCallback& callback,
      const NotificationCallback& notification_callback) = 0;

  // Stops observing an entry represented by |url|.
  virtual void UnwatchEntry(const FileSystemURL& url,
                            const StatusCallback& callback) = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_WATCHER_MANAGER_H_
