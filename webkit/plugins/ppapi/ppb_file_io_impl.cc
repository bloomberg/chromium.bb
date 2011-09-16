// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_io_impl.h"

#include "base/callback.h"
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
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_type_conversions.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/quota_file_io.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ppapi::PPTimeToTime;
using ppapi::TimeToPPTime;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileRef_API;

namespace webkit {
namespace ppapi {

PPB_FileIO_Impl::CallbackEntry::CallbackEntry()
    : read_buffer(NULL) {
}

PPB_FileIO_Impl::CallbackEntry::CallbackEntry(const CallbackEntry& entry)
    : callback(entry.callback),
      read_buffer(entry.read_buffer) {
}

PPB_FileIO_Impl::CallbackEntry::~CallbackEntry() {
}

PPB_FileIO_Impl::PPB_FileIO_Impl(PP_Instance instance)
    : Resource(instance),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
      file_(base::kInvalidPlatformFileValue),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      pending_op_(OPERATION_NONE),
      info_(NULL) {
}

PPB_FileIO_Impl::~PPB_FileIO_Impl() {
  Close();
}

PPB_FileIO_API* PPB_FileIO_Impl::AsPPB_FileIO_API() {
  return this;
}

int32_t PPB_FileIO_Impl::Open(PP_Resource pp_file_ref,
                              int32_t open_flags,
                              PP_CompletionCallback callback) {
  EnterResourceNoLock<PPB_FileRef_API> enter(pp_file_ref, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_FileRef_Impl* file_ref = static_cast<PPB_FileRef_Impl*>(enter.object());

  int32_t rv = CommonCallValidation(false, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  int flags = 0;
  if (!PepperFileOpenFlagsToPlatformFileFlags(open_flags, &flags))
    return PP_ERROR_BADARGUMENT;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return false;

  file_system_type_ = file_ref->GetFileSystemType();
  switch (file_system_type_) {
    case PP_FILESYSTEMTYPE_EXTERNAL:
      if (!plugin_delegate->AsyncOpenFile(
              file_ref->GetSystemPath(), flags,
              callback_factory_.NewCallback(
                  &PPB_FileIO_Impl::AsyncOpenFileCallback)))
        return PP_ERROR_FAILED;
      break;
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      file_system_url_ = file_ref->GetFileSystemURL();
      if (!plugin_delegate->AsyncOpenFileSystemURL(
              file_system_url_, flags,
              callback_factory_.NewCallback(
                  &PPB_FileIO_Impl::AsyncOpenFileCallback)))
        return PP_ERROR_FAILED;
      break;
    default:
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::Query(PP_FileInfo* info,
                               PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  if (!info)
    return PP_ERROR_BADARGUMENT;

  DCHECK(!info_);  // If |info_|, a callback should be pending (caught above).
  info_ = info;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
          plugin_delegate->GetFileThreadMessageLoopProxy(), file_,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::QueryInfoCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::Touch(PP_Time last_access_time,
                               PP_Time last_modified_time,
                               PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Touch(
          plugin_delegate->GetFileThreadMessageLoopProxy(),
          file_, PPTimeToTime(last_access_time),
          PPTimeToTime(last_modified_time),
          callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::Read(int64_t offset,
                              char* buffer,
                              int32_t bytes_to_read,
                              PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_READ, callback);
  if (rv != PP_OK)
    return rv;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Read(
          plugin_delegate->GetFileThreadMessageLoopProxy(),
          file_, offset, bytes_to_read,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::ReadCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_READ, callback, buffer);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::Write(int64_t offset,
                               const char* buffer,
                               int32_t bytes_to_write,
                               PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_WRITE, callback);
  if (rv != PP_OK)
    return rv;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (quota_file_io_.get()) {
    if (!quota_file_io_->Write(
            offset, buffer, bytes_to_write,
            callback_factory_.NewCallback(&PPB_FileIO_Impl::WriteCallback)))
      return PP_ERROR_FAILED;
  } else {
    if (!base::FileUtilProxy::Write(
            plugin_delegate->GetFileThreadMessageLoopProxy(),
            file_, offset, buffer, bytes_to_write,
            callback_factory_.NewCallback(&PPB_FileIO_Impl::WriteCallback)))
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_WRITE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::SetLength(int64_t length,
                                   PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (quota_file_io_.get()) {
    if (!quota_file_io_->SetLength(
            length,
            callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
      return PP_ERROR_FAILED;
  } else {
    if (!base::FileUtilProxy::Truncate(
            plugin_delegate->GetFileThreadMessageLoopProxy(),
            file_, length,
            callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
      return PP_ERROR_FAILED;
  }

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::Flush(PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Flush(
          plugin_delegate->GetFileThreadMessageLoopProxy(), file_,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_FileIO_Impl::Close() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (file_ != base::kInvalidPlatformFileValue && plugin_delegate) {
    base::FileUtilProxy::Close(
        plugin_delegate->GetFileThreadMessageLoopProxy(), file_, NULL);
    file_ = base::kInvalidPlatformFileValue;
    quota_file_io_.reset();
  }
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
                                   PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  if (!quota_file_io_.get())
    return PP_OK;

  if (!quota_file_io_->WillWrite(
          offset, bytes_to_write,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::WillWriteCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::WillSetLength(int64_t length,
                                       PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE, callback);
  if (rv != PP_OK)
    return rv;

  if (!quota_file_io_.get())
    return PP_OK;

  if (!quota_file_io_->WillSetLength(
          length,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileIO_Impl::CommonCallValidation(bool should_be_open,
                                              OperationType new_op,
                                              PP_CompletionCallback callback) {
  // Only asynchronous operation is supported.
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (should_be_open) {
    if (file_ == base::kInvalidPlatformFileValue)
      return PP_ERROR_FAILED;
  } else {
    if (file_ != base::kInvalidPlatformFileValue)
      return PP_ERROR_FAILED;
  }

  if (pending_op_ != OPERATION_NONE &&
      (pending_op_ != new_op || pending_op_ == OPERATION_EXCLUSIVE)) {
    return PP_ERROR_INPROGRESS;
  }

  return PP_OK;
}

void PPB_FileIO_Impl::RegisterCallback(OperationType op,
                                       PP_CompletionCallback callback,
                                       char* read_buffer) {
  DCHECK(callback.func);
  DCHECK(pending_op_ == OPERATION_NONE ||
         (pending_op_ != OPERATION_EXCLUSIVE && pending_op_ == op));

  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (!plugin_module)
    return;

  CallbackEntry entry;
  entry.callback = new TrackedCompletionCallback(
      plugin_module->GetCallbackTracker(), pp_resource(), callback);
  entry.read_buffer = read_buffer;

  callbacks_.push(entry);
  pending_op_ = op;
}

void PPB_FileIO_Impl::RunAndRemoveFirstPendingCallback(int32_t result) {
  DCHECK(!callbacks_.empty());

  CallbackEntry front = callbacks_.front();
  callbacks_.pop();
  if (callbacks_.empty())
    pending_op_ = OPERATION_NONE;

  front.callback->Run(result);  // Will complete abortively if necessary.
}

void PPB_FileIO_Impl::StatusCallback(base::PlatformFileError error_code) {
  if (pending_op_ != OPERATION_EXCLUSIVE || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  RunAndRemoveFirstPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::AsyncOpenFileCallback(
    base::PlatformFileError error_code,
    base::PassPlatformFile file) {
  if (pending_op_ != OPERATION_EXCLUSIVE || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file.ReleaseValue();

  DCHECK(!quota_file_io_.get());
  if (file_ != base::kInvalidPlatformFileValue &&
      (file_system_type_ == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
       file_system_type_ == PP_FILESYSTEMTYPE_LOCALPERSISTENT)) {
    quota_file_io_.reset(new QuotaFileIO(
        pp_instance(), file_, file_system_url_, file_system_type_));
  }

  RunAndRemoveFirstPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::QueryInfoCallback(
    base::PlatformFileError error_code,
    const base::PlatformFileInfo& file_info) {
  if (pending_op_ != OPERATION_EXCLUSIVE || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  DCHECK(info_);
  if (error_code == base::PLATFORM_FILE_OK) {
    info_->size = file_info.size;
    info_->creation_time = TimeToPPTime(file_info.creation_time);
    info_->last_access_time = TimeToPPTime(file_info.last_accessed);
    info_->last_modified_time = TimeToPPTime(file_info.last_modified);
    info_->system_type = file_system_type_;
    if (file_info.is_directory)
      info_->type = PP_FILETYPE_DIRECTORY;
    else
      info_->type = PP_FILETYPE_REGULAR;
  }
  info_ = NULL;
  RunAndRemoveFirstPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::ReadCallback(base::PlatformFileError error_code,
                                   const char* data, int bytes_read) {
  if (pending_op_ != OPERATION_READ || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  char* read_buffer = callbacks_.front().read_buffer;
  DCHECK(data);
  DCHECK(read_buffer);

  int rv;
  if (error_code == base::PLATFORM_FILE_OK) {
    rv = bytes_read;
    if (file_ != base::kInvalidPlatformFileValue)
      memcpy(read_buffer, data, bytes_read);
  } else {
    rv = PlatformFileErrorToPepperError(error_code);
  }

  RunAndRemoveFirstPendingCallback(rv);
}

void PPB_FileIO_Impl::WriteCallback(base::PlatformFileError error_code,
                                    int bytes_written) {
  if (pending_op_ != OPERATION_WRITE || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  if (error_code != base::PLATFORM_FILE_OK) {
    RunAndRemoveFirstPendingCallback(
        PlatformFileErrorToPepperError(error_code));
  } else {
    RunAndRemoveFirstPendingCallback(bytes_written);
  }
}

void PPB_FileIO_Impl::WillWriteCallback(base::PlatformFileError error_code,
                                        int bytes_written) {
  if (pending_op_ != OPERATION_EXCLUSIVE || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  if (error_code != base::PLATFORM_FILE_OK) {
    RunAndRemoveFirstPendingCallback(
        PlatformFileErrorToPepperError(error_code));
  } else {
    RunAndRemoveFirstPendingCallback(bytes_written);
  }
}

}  // namespace ppapi
}  // namespace webkit
