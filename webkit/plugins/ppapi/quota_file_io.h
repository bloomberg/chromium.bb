// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_QUOTA_FILE_IO_H_
#define WEBKIT_PLUGINS_PPAPI_QUOTA_FILE_IO_H_

#include <deque>

#include "base/file_util_proxy.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "webkit/plugins/webkit_plugins_export.h"
#include "webkit/quota/quota_types.h"

namespace webkit {
namespace ppapi {

class PluginDelegate;

// This class is created per PPB_FileIO_Impl instance and provides
// write operations for quota-managed files (i.e. files of
// PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}).
class QuotaFileIO {
 public:
  typedef base::FileUtilProxy::WriteCallback WriteCallback;
  typedef base::FileUtilProxy::StatusCallback StatusCallback;

  WEBKIT_PLUGINS_EXPORT QuotaFileIO(PP_Instance instance,
                                    base::PlatformFile file,
                                    const GURL& path_url,
                                    PP_FileSystemType type);
  WEBKIT_PLUGINS_EXPORT ~QuotaFileIO();

  // Performs write or setlength operation with quota checks.
  // Returns true when the operation is successfully dispatched.
  // |bytes_to_write| must be geater than zero.
  // |callback| will be dispatched when the operation completes.
  // Otherwise it returns false and |callback| will not be dispatched.
  // |callback| will not be dispatched either when this instance is
  // destroyed before the operation completes.
  // SetLength/WillSetLength cannot be called while there're any in-flight
  // operations.  For Write/WillWrite it is guaranteed that |callback| are
  // always dispatched in the same order as Write being called.
  WEBKIT_PLUGINS_EXPORT bool Write(int64_t offset,
                                   const char* buffer,
                                   int32_t bytes_to_write,
                                   const WriteCallback& callback);
  WEBKIT_PLUGINS_EXPORT bool WillWrite(int64_t offset,
                                       int32_t bytes_to_write,
                                       const WriteCallback& callback);

  WEBKIT_PLUGINS_EXPORT bool SetLength(int64_t length,
                                       const StatusCallback& callback);
  WEBKIT_PLUGINS_EXPORT bool WillSetLength(int64_t length,
                                           const StatusCallback& callback);

  // Returns the plugin delegate or NULL if the resource has outlived the
  // instance.
  PluginDelegate* GetPluginDelegate() const;

 private:
  class PendingOperationBase;
  class WriteOperation;
  class SetLengthOperation;

  bool CheckIfExceedsQuota(int64_t new_file_size) const;
  void WillUpdate();
  void DidWrite(WriteOperation* op, int64_t written_offset_end);
  void DidSetLength(base::PlatformFileError error, int64_t new_file_size);

  bool RegisterOperationForQuotaChecks(PendingOperationBase* op);
  void DidQueryInfoForQuota(base::PlatformFileError error_code,
                            const base::PlatformFileInfo& file_info);
  void DidQueryAvailableSpace(int64_t avail_space);
  void DidQueryForQuotaCheck();

  // The plugin instance that owns this (via PPB_FileIO_Impl).
  PP_Instance pp_instance_;

  // The file information associated to this instance.
  base::PlatformFile file_;
  GURL file_url_;
  quota::StorageType storage_type_;

  // Pending operations that are waiting quota checks and pending
  // callbacks that are to be fired after the operation;
  // we use two separate queues for them so that we can safely dequeue the
  // pending callbacks while enqueueing new operations. (This could
  // happen when callbacks are dispatched synchronously due to error etc.)
  std::deque<PendingOperationBase*> pending_operations_;
  std::deque<PendingOperationBase*> pending_callbacks_;

  // Valid only while there're pending quota checks.
  int64_t cached_file_size_;
  int64_t cached_available_space_;

  // Quota-related queries and errors occurred during in-flight quota checks.
  int outstanding_quota_queries_;
  int outstanding_errors_;

  // For parallel writes bookkeeping.
  int64_t max_written_offset_;
  int inflight_operations_;

  base::WeakPtrFactory<QuotaFileIO> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaFileIO);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_QUOTA_FILE_IO_H_
