// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_TRACKER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_TRACKER_H_

#include <deque>
#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "webkit/fileapi/file_system_types.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

// Owned by the SandboxedFileSystemContext, which is a per-profile
// instance, and has the same lifetime as the SandboxedFileSystemContext.
// It's going to be created and destroyed on the IO thread in chrome.
// (The destruction on the same thread where it is created was guaranteed
// by its owner, SandboxedFileSystemContext.)
class FileSystemUsageTracker {
 public:
  FileSystemUsageTracker(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const FilePath& profile_path,
      bool is_incognito);
  ~FileSystemUsageTracker();

  // Get the amount of data stored in the filesystem specified by
  // |origin_url| and |type|.
  typedef Callback1<int64 /* usage */>::Type GetUsageCallback;
  void GetOriginUsage(const GURL& origin_url,
                      fileapi::FileSystemType type,
                      GetUsageCallback* callback);

 private:
  class GetUsageTask;

  void RegisterUsageTask(GetUsageTask* task);
  void UnregisterUsageTask(GetUsageTask* task);

  void DidGetOriginUsage(const std::string& fs_name, int64 usage);

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  FilePath base_path_;
  bool is_incognito_;

  typedef std::deque<GetUsageTask*> UsageTaskQueue;
  UsageTaskQueue running_usage_tasks_;

  typedef std::list<GetUsageCallback*> PendingCallbackList;
  typedef std::map<std::string, PendingCallbackList> PendingUsageCallbackMap;
  PendingUsageCallbackMap pending_usage_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemUsageTracker);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_TRACKER_H_
