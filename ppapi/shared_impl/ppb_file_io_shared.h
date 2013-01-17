// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_FILE_IO_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_FILE_IO_SHARED_H_

#include <deque>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_file_io_api.h"

namespace ppapi {

namespace thunk {
class PPB_FileRef_API;
}

class PPAPI_SHARED_EXPORT PPB_FileIO_Shared : public Resource,
                                              public thunk::PPB_FileIO_API {
 public:
  PPB_FileIO_Shared(PP_Instance instance);
  PPB_FileIO_Shared(const HostResource& host_resource);
  ~PPB_FileIO_Shared();

  // Resource overrides.
  virtual thunk::PPB_FileIO_API* AsPPB_FileIO_API() OVERRIDE;

  // PPB_FileIO_API implementation.
  virtual int32_t Open(PP_Resource file_ref,
                       int32_t open_flags,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Query(PP_FileInfo* info,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Read(int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t ReadToArray(int64_t offset,
                              int32_t max_read_length,
                              PP_ArrayOutput* output_array_buffer,
                              scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t SetLength(int64_t length,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // Callback handler for different types of operations.
  void ExecuteGeneralCallback(int32_t pp_error);
  void ExecuteOpenFileCallback(int32_t pp_error);
  void ExecuteQueryCallback(int32_t pp_error, const PP_FileInfo& info);
  void ExecuteReadCallback(int32_t pp_error_or_bytes, const char* data);

 protected:
  struct CallbackEntry {
    CallbackEntry();
    CallbackEntry(const CallbackEntry& entry);
    ~CallbackEntry();

    scoped_refptr<TrackedCallback> callback;

    // Output buffer used only by |Read()|.
    PP_ArrayOutput read_buffer;

    // Pointer back to the caller's PP_FileInfo structure for Query operations.
    // NULL for non-query operations. Not owned.
    PP_FileInfo* info;
  };

  enum OperationType {
    // There is no pending operation right now.
    OPERATION_NONE,

    // If there are pending reads, any other kind of async operation is not
    // allowed.
    OPERATION_READ,

    // If there are pending writes, any other kind of async operation is not
    // allowed.
    OPERATION_WRITE,

    // If there is a pending operation that is neither read nor write, no
    // further async operation is allowed.
    OPERATION_EXCLUSIVE
  };

  // Validated versions of the FileIO API. Subclasses in the proxy and impl
  // implement these so the common error checking stays here.
  virtual int32_t OpenValidated(PP_Resource file_ref_resource,
                                thunk::PPB_FileRef_API* file_ref_api,
                                int32_t open_flags,
                                scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t QueryValidated(PP_FileInfo* info,
                                 scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t TouchValidated(PP_Time last_access_time,
                                 PP_Time last_modified_time,
                                 scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t ReadValidated(int64_t offset,
                                const PP_ArrayOutput& buffer,
                                int32_t max_read_length,
                                scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t WriteValidated(int64_t offset,
                                 const char* buffer,
                                 int32_t bytes_to_write,
                                 scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t SetLengthValidated(
      int64_t length,
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t FlushValidated(scoped_refptr<TrackedCallback> callback) = 0;

  // Called for every "Validated" function.
  //
  // This verifies that the callback is valid and that no callback is already
  // pending, or it is a read(write) request and currently the pending
  // operations are reads(writes).
  //
  // Returns |PP_OK| to indicate that everything is valid or |PP_ERROR_...| if
  // the call should be aborted and that code returned to the plugin.
  int32_t CommonCallValidation(bool should_be_open, OperationType new_op);

  // Sets up a pending callback. This should only be called once it is certain
  // that |PP_OK_COMPLETIONPENDING| will be returned.
  //
  // |read_buffer| is only used by read operations, |info| is used only by
  // query operations.
  void RegisterCallback(OperationType op,
                        scoped_refptr<TrackedCallback> callback,
                        const PP_ArrayOutput* read_buffer,
                        PP_FileInfo* info);

  // Pops the oldest callback from the queue and runs it.
  void RunAndRemoveFirstPendingCallback(int32_t result);

  // The file system type specified in the Open() call. This will be
  // PP_FILESYSTEMTYPE_INVALID before open was called. This value does not
  // indicate that the open command actually succeeded.
  PP_FileSystemType file_system_type_;

  // Set to true when the file has been successfully opened.
  bool file_open_;

  std::deque<CallbackEntry> callbacks_;
  OperationType pending_op_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileIO_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_FILE_IO_SHARED_H_
