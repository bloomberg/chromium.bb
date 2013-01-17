// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/ppb_file_io_shared.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace webkit {
namespace ppapi {

class QuotaFileIO;

class PPB_FileIO_Impl : public ::ppapi::PPB_FileIO_Shared {
 public:
  explicit PPB_FileIO_Impl(PP_Instance instance);
  virtual ~PPB_FileIO_Impl();

  // PPB_FileIO_API implementation (most of the operations are implemented
  // as the "Validated" versions below).
  virtual void Close() OVERRIDE;
  virtual int32_t GetOSFileDescriptor() OVERRIDE;
  virtual int32_t WillWrite(
      int64_t offset,
      int32_t bytes_to_write,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t WillSetLength(
      int64_t length,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;

 private:
  // FileIOImpl overrides.
  virtual int32_t OpenValidated(
      PP_Resource file_ref_resource,
      ::ppapi::thunk::PPB_FileRef_API* file_ref_api,
      int32_t open_flags,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t QueryValidated(
      PP_FileInfo* info,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t TouchValidated(
      PP_Time last_access_time,
      PP_Time last_modified_time,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t ReadValidated(
      int64_t offset,
      const PP_ArrayOutput& output_array_buffer,
      int32_t max_read_length,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t WriteValidated(
      int64_t offset,
      const char* buffer,
      int32_t bytes_to_write,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t SetLengthValidated(
      int64_t length,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t FlushValidated(
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;

  // Returns the plugin delegate for this resource if it exists, or NULL if it
  // doesn't. Calling code should always check for NULL.
  PluginDelegate* GetPluginDelegate();

  // Callback handlers. These mostly convert the PlatformFileError to the
  // PP_Error code and call the shared (non-"Platform") version.
  void ExecutePlatformGeneralCallback(base::PlatformFileError error_code);
  void ExecutePlatformOpenFileCallback(base::PlatformFileError error_code,
                                       base::PassPlatformFile file);
  void ExecutePlatformOpenFileSystemURLCallback(
      base::PlatformFileError error_code,
      base::PassPlatformFile file,
      const PluginDelegate::NotifyCloseFileCallback& callback);
  void ExecutePlatformQueryCallback(base::PlatformFileError error_code,
                                    const base::PlatformFileInfo& file_info);
  void ExecutePlatformReadCallback(base::PlatformFileError error_code,
                                   const char* data, int bytes_read);
  void ExecutePlatformWriteCallback(base::PlatformFileError error_code,
                                    int bytes_written);
  void ExecutePlatformWillWriteCallback(base::PlatformFileError error_code,
                                        int bytes_written);

  base::PlatformFile file_;

  // Valid only for PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}.
  GURL file_system_url_;

  // Callback function for notifying when the file handle is closed.
  PluginDelegate::NotifyCloseFileCallback notify_close_file_callback_;

  // Pointer to a QuotaFileIO instance, which is valid only while a file
  // of type PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY} is opened.
  scoped_ptr<QuotaFileIO> quota_file_io_;

  base::WeakPtrFactory<PPB_FileIO_Impl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileIO_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_
