// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_io_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/logging.h"
#include "base/time.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/quota_file_io.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::PPTimeToTime;
using ppapi::TimeToPPTime;
using ppapi::TrackedCallback;
using ppapi::thunk::PPB_FileRef_API;

namespace webkit {
namespace ppapi {

namespace {

typedef base::Callback<void (base::PlatformFileError)> PlatformGeneralCallback;

class PlatformGeneralCallbackTranslator
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  PlatformGeneralCallbackTranslator(
      const PlatformGeneralCallback& callback)
    : callback_(callback) {}

  virtual ~PlatformGeneralCallbackTranslator() {}

  virtual void DidSucceed() OVERRIDE {
    callback_.Run(base::PLATFORM_FILE_OK);
  }

  virtual void DidReadMetadata(
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    callback_.Run(error_code);
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFile(base::PlatformFile file) OVERRIDE {
    NOTREACHED();
  }

 private:
  PlatformGeneralCallback callback_;
};

}  // namespace

PPB_FileIO_Impl::PPB_FileIO_Impl(PP_Instance instance)
    : ::ppapi::PPB_FileIO_Shared(instance),
      file_(base::kInvalidPlatformFileValue),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

PPB_FileIO_Impl::~PPB_FileIO_Impl() {
  Close();
}

int32_t PPB_FileIO_Impl::OpenValidated(
    PP_Resource file_ref_resource,
    PPB_FileRef_API* file_ref_api,
    int32_t open_flags,
    scoped_refptr<TrackedCallback> callback) {
  PPB_FileRef_Impl* file_ref = static_cast<PPB_FileRef_Impl*>(file_ref_api);

  int flags = 0;
  if (!::ppapi::PepperFileOpenFlagsToPlatformFileFlags(open_flags, &flags))
    return PP_ERROR_BADARGUMENT;

  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_BADARGUMENT;

  if (file_ref->HasValidFileSystem()) {
    file_system_url_ = file_ref->GetFileSystemURL();
    if (!plugin_delegate->AsyncOpenFileSystemURL(
            file_system_url_, flags,
            base::Bind(
                &PPB_FileIO_Impl::ExecutePlatformOpenFileSystemURLCallback,
                weak_factory_.GetWeakPtr())))
      return PP_ERROR_FAILED;
  } else {
    if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL)
      return PP_ERROR_FAILED;
    if (!plugin_delegate->AsyncOpenFile(
            file_ref->GetSystemPath(), flags,
            base::Bind(&PPB_FileIO_Impl::ExecutePlatformOpenFileCallback,
                       weak_factory_.GetWeakPtr())))
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::QueryValidated(
    PP_FileInfo* info,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
          plugin_delegate->GetFileThreadMessageLoopProxy(), file_,
          base::Bind(&PPB_FileIO_Impl::ExecutePlatformQueryCallback,
                     weak_factory_.GetWeakPtr())))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, info);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::TouchValidated(
    PP_Time last_access_time,
    PP_Time last_modified_time,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    if (!plugin_delegate->Touch(
            file_system_url_,
            PPTimeToTime(last_access_time),
            PPTimeToTime(last_modified_time),
            new PlatformGeneralCallbackTranslator(
                base::Bind(&PPB_FileIO_Impl::ExecutePlatformGeneralCallback,
                           weak_factory_.GetWeakPtr()))))
      return PP_ERROR_FAILED;
  } else {
    // TODO(nhiroki): fix a failure of FileIO.Touch for an external filesystem
    // on Mac and Linux due to sandbox restrictions (http://crbug.com/101128).
    if (!base::FileUtilProxy::Touch(
            plugin_delegate->GetFileThreadMessageLoopProxy(),
            file_, PPTimeToTime(last_access_time),
            PPTimeToTime(last_modified_time),
            base::Bind(&PPB_FileIO_Impl::ExecutePlatformGeneralCallback,
                       weak_factory_.GetWeakPtr())))
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::ReadValidated(
    int64_t offset,
    const PP_ArrayOutput& output_array_buffer,
    int32_t max_read_length,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Read(
          plugin_delegate->GetFileThreadMessageLoopProxy(), file_, offset,
          max_read_length,
          base::Bind(&PPB_FileIO_Impl::ExecutePlatformReadCallback,
                     weak_factory_.GetWeakPtr())))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_READ, callback, &output_array_buffer, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::WriteValidated(
    int64_t offset,
    const char* buffer,
    int32_t bytes_to_write,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (quota_file_io_.get()) {
    if (!quota_file_io_->Write(
            offset, buffer, bytes_to_write,
            base::Bind(&PPB_FileIO_Impl::ExecutePlatformWriteCallback,
                       weak_factory_.GetWeakPtr())))
      return PP_ERROR_FAILED;
  } else {
    if (!base::FileUtilProxy::Write(
            plugin_delegate->GetFileThreadMessageLoopProxy(), file_, offset,
            buffer, bytes_to_write,
            base::Bind(&PPB_FileIO_Impl::ExecutePlatformWriteCallback,
                       weak_factory_.GetWeakPtr())))
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_WRITE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::SetLengthValidated(
    int64_t length,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    if (!plugin_delegate->SetLength(
            file_system_url_, length,
            new PlatformGeneralCallbackTranslator(
                base::Bind(&PPB_FileIO_Impl::ExecutePlatformGeneralCallback,
                           weak_factory_.GetWeakPtr()))))
      return PP_ERROR_FAILED;
  } else {
    // TODO(nhiroki): fix a failure of FileIO.SetLength for an external
    // filesystem on Mac due to sandbox restrictions (http://crbug.com/156077).
    if (!base::FileUtilProxy::Truncate(
            plugin_delegate->GetFileThreadMessageLoopProxy(), file_, length,
            base::Bind(&PPB_FileIO_Impl::ExecutePlatformGeneralCallback,
                       weak_factory_.GetWeakPtr())))
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::FlushValidated(
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Flush(
          plugin_delegate->GetFileThreadMessageLoopProxy(), file_,
          base::Bind(&PPB_FileIO_Impl::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr())))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_FileIO_Impl::Close() {
  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (file_ != base::kInvalidPlatformFileValue && plugin_delegate) {
    base::FileUtilProxy::Close(
        plugin_delegate->GetFileThreadMessageLoopProxy(),
        file_,
        base::ResetAndReturn(&notify_close_file_callback_));
    file_ = base::kInvalidPlatformFileValue;
    quota_file_io_.reset();
  }
  // TODO(viettrungluu): Check what happens to the callback (probably the
  // wrong thing). May need to post abort here. crbug.com/69457
}

int32_t PPB_FileIO_Impl::GetOSFileDescriptor() {
#if defined(OS_POSIX)
  return file_;
#elif defined(OS_WIN)
  return reinterpret_cast<uintptr_t>(file_);
#else
#error "Platform not supported."
#endif
}

int32_t PPB_FileIO_Impl::WillWrite(int64_t offset,
                                   int32_t bytes_to_write,
                                   scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;

  if (!quota_file_io_.get())
    return PP_OK;

  if (!quota_file_io_->WillWrite(
          offset, bytes_to_write,
          base::Bind(&PPB_FileIO_Impl::ExecutePlatformWillWriteCallback,
                     weak_factory_.GetWeakPtr())))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::WillSetLength(
    int64_t length,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;

  if (!quota_file_io_.get())
    return PP_OK;

  if (!quota_file_io_->WillSetLength(
          length,
          base::Bind(&PPB_FileIO_Impl::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr())))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

PluginDelegate* PPB_FileIO_Impl::GetPluginDelegate() {
  return ResourceHelper::GetPluginDelegate(this);
}

void PPB_FileIO_Impl::ExecutePlatformGeneralCallback(
    base::PlatformFileError error_code) {
  ExecuteGeneralCallback(::ppapi::PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::ExecutePlatformOpenFileCallback(
    base::PlatformFileError error_code,
    base::PassPlatformFile file) {
  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file.ReleaseValue();

  DCHECK(!quota_file_io_.get());
  if (file_ != base::kInvalidPlatformFileValue &&
      (file_system_type_ == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
       file_system_type_ == PP_FILESYSTEMTYPE_LOCALPERSISTENT)) {
    quota_file_io_.reset(new QuotaFileIO(
        pp_instance(), file_, file_system_url_, file_system_type_));
  }

  ExecuteOpenFileCallback(::ppapi::PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::ExecutePlatformOpenFileSystemURLCallback(
    base::PlatformFileError error_code,
    base::PassPlatformFile file,
    const PluginDelegate::NotifyCloseFileCallback& callback) {
  if (error_code == base::PLATFORM_FILE_OK)
    notify_close_file_callback_ = callback;
  ExecutePlatformOpenFileCallback(error_code, file);
}

void PPB_FileIO_Impl::ExecutePlatformQueryCallback(
    base::PlatformFileError error_code,
    const base::PlatformFileInfo& file_info) {
  PP_FileInfo pp_info;
  pp_info.size = file_info.size;
  pp_info.creation_time = TimeToPPTime(file_info.creation_time);
  pp_info.last_access_time = TimeToPPTime(file_info.last_accessed);
  pp_info.last_modified_time = TimeToPPTime(file_info.last_modified);
  pp_info.system_type = file_system_type_;
  if (file_info.is_directory)
    pp_info.type = PP_FILETYPE_DIRECTORY;
  else
    pp_info.type = PP_FILETYPE_REGULAR;

  ExecuteQueryCallback(::ppapi::PlatformFileErrorToPepperError(error_code),
                       pp_info);
}

void PPB_FileIO_Impl::ExecutePlatformReadCallback(
    base::PlatformFileError error_code,
    const char* data, int bytes_read) {
  // Map the error code, OK getting mapped to the # of bytes read.
  int32_t pp_result = ::ppapi::PlatformFileErrorToPepperError(error_code);
  pp_result = pp_result == PP_OK ? bytes_read : pp_result;
  ExecuteReadCallback(pp_result, data);
}

void PPB_FileIO_Impl::ExecutePlatformWriteCallback(
    base::PlatformFileError error_code,
    int bytes_written) {
  int32_t pp_result = ::ppapi::PlatformFileErrorToPepperError(error_code);
  ExecuteGeneralCallback(pp_result == PP_OK ? bytes_written : pp_result);
}

void PPB_FileIO_Impl::ExecutePlatformWillWriteCallback(
    base::PlatformFileError error_code,
    int bytes_written) {
  if (pending_op_ != OPERATION_EXCLUSIVE || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  if (error_code != base::PLATFORM_FILE_OK) {
    RunAndRemoveFirstPendingCallback(
        ::ppapi::PlatformFileErrorToPepperError(error_code));
  } else {
    RunAndRemoveFirstPendingCallback(bytes_written);
  }
}

}  // namespace ppapi
}  // namespace webkit
