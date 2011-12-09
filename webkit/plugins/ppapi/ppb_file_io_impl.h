// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_

#include <queue>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_file_io_api.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

struct PP_CompletionCallback;
struct PPB_FileIO;

namespace webkit {
namespace ppapi {

class QuotaFileIO;

class PPB_FileIO_Impl : public ::ppapi::Resource,
                        public ::ppapi::thunk::PPB_FileIO_API {
 public:
  explicit PPB_FileIO_Impl(PP_Instance instance);
  virtual ~PPB_FileIO_Impl();

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_FileIO_API* AsPPB_FileIO_API() OVERRIDE;

  // PPB_FileIO_API implementation.
  virtual int32_t Open(PP_Resource file_ref,
                       int32_t open_flags,
                       PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Query(PP_FileInfo* info,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Read(int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t SetLength(int64_t length,
                            PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Flush(PP_CompletionCallback callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t GetOSFileDescriptor() OVERRIDE;
  virtual int32_t WillWrite(int64_t offset,
                            int32_t bytes_to_write,
                            PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t WillSetLength(int64_t length,
                                PP_CompletionCallback callback) OVERRIDE;

 private:
  struct CallbackEntry {
    CallbackEntry();
    CallbackEntry(const CallbackEntry& entry);
    ~CallbackEntry();

    scoped_refptr<TrackedCompletionCallback> callback;

    // Pointer back to the caller's read buffer; only used by |Read()|.
    // Not owned.
    char* read_buffer;
  };

  enum OperationType {
    // If there are pending reads, any other kind of async operation is not
    // allowed.
    OPERATION_READ,
    // If there are pending writes, any other kind of async operation is not
    // allowed.
    OPERATION_WRITE,
    // If there is a pending operation that is neither read nor write, no
    // further async operation is allowed.
    OPERATION_EXCLUSIVE,
    // There is no pending operation right now.
    OPERATION_NONE,
  };

  // Verifies:
  //  - that |callback| is valid (only nonblocking operation supported);
  //  - that the file is already open or not, depending on |should_be_open|; and
  //  - that no callback is already pending, or it is a read(write) request
  //    and currently the pending operations are reads(writes).
  // Returns |PP_OK| to indicate that everything is valid or |PP_ERROR_...| if
  // the call should be aborted and that code returned to the plugin.
  int32_t CommonCallValidation(bool should_be_open,
                               OperationType new_op,
                               PP_CompletionCallback callback);

  // Sets up a pending callback. This should only be called once it is certain
  // that |PP_OK_COMPLETIONPENDING| will be returned.
  // |read_buffer| is only used by read operations.
  void RegisterCallback(OperationType op,
                        PP_CompletionCallback callback,
                        char* read_buffer);
  void RunAndRemoveFirstPendingCallback(int32_t result);

  void StatusCallback(base::PlatformFileError error_code);
  void AsyncOpenFileCallback(base::PlatformFileError error_code,
                             base::PassPlatformFile file);
  void QueryInfoCallback(base::PlatformFileError error_code,
                         const base::PlatformFileInfo& file_info);
  void ReadCallback(base::PlatformFileError error_code,
                    const char* data, int bytes_read);
  void WriteCallback(base::PlatformFileError error_code, int bytes_written);
  void WillWriteCallback(base::PlatformFileError error_code, int bytes_written);

  base::PlatformFile file_;
  PP_FileSystemType file_system_type_;

  // Valid only for PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}.
  GURL file_system_url_;

  std::queue<CallbackEntry> callbacks_;
  OperationType pending_op_;

  // Output buffer pointer for |Query()|; only non-null when a callback is
  // pending for it.
  PP_FileInfo* info_;

  // Pointer to a QuotaFileIO instance, which is valid only while a file
  // of type PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY} is opened.
  scoped_ptr<QuotaFileIO> quota_file_io_;

  base::WeakPtrFactory<PPB_FileIO_Impl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileIO_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_
