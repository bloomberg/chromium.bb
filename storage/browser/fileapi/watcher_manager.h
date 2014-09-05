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
  typedef base::Callback<void(base::File::Error result)> StatusCallback;

  // Observes watched entries.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // Notifies about an entry represented by |url| being changed.
    virtual void OnEntryChanged(const FileSystemURL& url) = 0;

    // Notifies about an entry represented by |url| being removed.
    virtual void OnEntryRemoved(const FileSystemURL& url) = 0;
  };

  virtual ~WatcherManager() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) const = 0;

  // Observes a directory entry. If the |recursive| mode is not supported then
  // FILE_ERROR_INVALID_OPERATION must be returned as an error. If the |url| is
  // already watched, or setting up the watcher fails, then |callback|
  // must be called with a specific error code. Otherwise |callback| must be
  // called with the FILE_OK error code.
  virtual void WatchDirectory(const FileSystemURL& url,
                              bool recursive,
                              const StatusCallback& callback) = 0;

  // Stops observing an entry represented by |url|.
  virtual void UnwatchEntry(const FileSystemURL& url,
                            const StatusCallback& callback) = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_WATCHER_MANAGER_H_
