// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_IO_RESOURCE_H_
#define PPAPI_PROXY_FILE_IO_RESOURCE_H_

#include <string>

#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/file_io_state_manager.h"
#include "ppapi/thunk/ppb_file_io_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT FileIOResource
    : public PluginResource,
      public thunk::PPB_FileIO_API {
 public:
  FileIOResource(Connection connection, PP_Instance instance);
  virtual ~FileIOResource();

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
                              PP_ArrayOutput* array_output,
                              scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t SetLength(int64_t length,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t GetOSFileDescriptor() OVERRIDE;
  virtual int32_t RequestOSFileHandle(
      PP_FileHandle* handle,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t WillWrite(int64_t offset,
                            int32_t bytes_to_write,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t WillSetLength(
      int64_t length,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  // Class to perform file query operations across multiple threads.
  class QueryOp : public base::RefCountedThreadSafe<QueryOp> {
   public:
    explicit QueryOp(PP_FileHandle file_handle);

    // Queries the file. Called on the file thread (non-blocking) or the plugin
    // thread (blocking). This should not be called when we hold the proxy lock.
    int32_t DoWork();

    const base::PlatformFileInfo& file_info() const { return file_info_; }

   private:
    friend class base::RefCountedThreadSafe<QueryOp>;
    ~QueryOp();

    PP_FileHandle file_handle_;
    base::PlatformFileInfo file_info_;
  };

  // Class to perform file read operations across multiple threads.
  class ReadOp : public base::RefCountedThreadSafe<ReadOp> {
   public:
    ReadOp(PP_FileHandle file_handle, int64_t offset, int32_t bytes_to_read);

    // Reads the file. Called on the file thread (non-blocking) or the plugin
    // thread (blocking). This should not be called when we hold the proxy lock.
    int32_t DoWork();

    char* buffer() const { return buffer_.get(); }

   private:
    friend class base::RefCountedThreadSafe<ReadOp>;
    ~ReadOp();

    PP_FileHandle file_handle_;
    int64_t offset_;
    int32_t bytes_to_read_;
    scoped_ptr<char[]> buffer_;
  };

  int32_t ReadValidated(int64_t offset,
                        int32_t bytes_to_read,
                        const PP_ArrayOutput& array_output,
                        scoped_refptr<TrackedCallback> callback);

  void CloseFileHandle();


  // Completion tasks for file operations that are done in the plugin.
  int32_t OnQueryComplete(scoped_refptr<QueryOp> query_op,
                          PP_FileInfo* info,
                          int32_t result);
  int32_t OnReadComplete(scoped_refptr<ReadOp> read_op,
                         PP_ArrayOutput array_output,
                         int32_t result);

  // Reply message handlers for operations that are done in the host.
  void OnPluginMsgGeneralComplete(scoped_refptr<TrackedCallback> callback,
                                  const ResourceMessageReplyParams& params);
  void OnPluginMsgOpenFileComplete(scoped_refptr<TrackedCallback> callback,
                                   const ResourceMessageReplyParams& params);
  void OnPluginMsgRequestOSFileHandleComplete(
      scoped_refptr<TrackedCallback> callback,
      PP_FileHandle* output_handle,
      const ResourceMessageReplyParams& params);

  PP_FileHandle file_handle_;
  PP_FileSystemType file_system_type_;
  FileIOStateManager state_manager_;

  DISALLOW_COPY_AND_ASSIGN(FileIOResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_IO_RESOURCE_H_
