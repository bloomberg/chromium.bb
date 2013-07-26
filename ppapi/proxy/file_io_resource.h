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
  int32_t ReadValidated(int64_t offset,
                        int32_t bytes_to_read,
                        const PP_ArrayOutput& array_output,
                        scoped_refptr<TrackedCallback> callback);
  void CloseFileHandle();

  // Reply message handlers for operations that are done in the host.
  void OnPluginMsgGeneralComplete(scoped_refptr<TrackedCallback> callback,
                                  const ResourceMessageReplyParams& params);
  void OnPluginMsgOpenFileComplete(scoped_refptr<TrackedCallback> callback,
                                   const ResourceMessageReplyParams& params);
  void OnPluginMsgRequestOSFileHandleComplete(
      scoped_refptr<TrackedCallback> callback,
      PP_FileHandle* output_handle,
      const ResourceMessageReplyParams& params);

  // Reply message handlers for operations that are done in the plugin.
  void OnQueryComplete(scoped_refptr<TrackedCallback> callback,
                       PP_FileInfo* output_info,
                       base::PlatformFileError error_code,
                       const base::PlatformFileInfo& file_info);
  void OnReadComplete(scoped_refptr<TrackedCallback> callback,
                      PP_ArrayOutput array_output,
                      base::PlatformFileError error_code,
                      const char* data, int bytes_read);

  PP_FileHandle file_handle_;
  PP_FileSystemType file_system_type_;
  FileIOStateManager state_manager_;

  DISALLOW_COPY_AND_ASSIGN(FileIOResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_IO_RESOURCE_H_
