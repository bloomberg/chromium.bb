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
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  PPB_FileIO_Impl* file_io = new PPB_FileIO_Impl(instance);
  return file_io->GetReference();
}

PP_Bool IsFileIO(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_FileIO_Impl>(resource));
}

int32_t Open(PP_Resource file_io_id,
             PP_Resource file_ref_id,
             int32_t open_flags,
             PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  return file_io->Open(file_ref, open_flags, callback);
}

int32_t Query(PP_Resource file_io_id,
              PP_FileInfo_Dev* info,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Query(info, callback);
}

int32_t Touch(PP_Resource file_io_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Touch(last_access_time, last_modified_time, callback);
}

int32_t Read(PP_Resource file_io_id,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Read(offset, buffer, bytes_to_read, callback);
}

int32_t Write(PP_Resource file_io_id,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Write(offset, buffer, bytes_to_write, callback);
}

int32_t SetLength(PP_Resource file_io_id,
                  int64_t length,
                  PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->SetLength(length, callback);
}

int32_t Flush(PP_Resource file_io_id,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Flush(callback);
}

void Close(PP_Resource file_io_id) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return;
  file_io->Close();
}

const PPB_FileIO_Dev ppb_fileio = {
  &Create,
  &IsFileIO,
  &Open,
  &Query,
  &Touch,
  &Read,
  &Write,
  &SetLength,
  &Flush,
  &Close
};

int32_t GetOSFileDescriptor(PP_Resource file_io_id) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->GetOSFileDescriptor();
}

int32_t WillWrite(PP_Resource file_io_id,
                  int64_t offset,
                  int32_t bytes_to_write,
                  PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->WillWrite(offset, bytes_to_write, callback);
}

int32_t WillSetLength(PP_Resource file_io_id,
                      int64_t length,
                      PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileIO_Impl>
      file_io(Resource::GetAs<PPB_FileIO_Impl>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->WillSetLength(length, callback);
}

const PPB_FileIOTrusted_Dev ppb_fileiotrusted = {
  &GetOSFileDescriptor,
  &WillWrite,
  &WillSetLength
};

int PlatformFileErrorToPepperError(base::PlatformFileError error_code) {
  switch (error_code) {
    case base::PLATFORM_FILE_OK:
      return PP_OK;
    case base::PLATFORM_FILE_ERROR_EXISTS:
      return PP_ERROR_FILEEXISTS;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return PP_ERROR_FILENOTFOUND;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return PP_ERROR_NOACCESS;
    case base::PLATFORM_FILE_ERROR_NO_MEMORY:
      return PP_ERROR_NOMEMORY;
    case base::PLATFORM_FILE_ERROR_NO_SPACE:
      return PP_ERROR_NOSPACE;
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      NOTREACHED();
      return PP_ERROR_FAILED;
    default:
      return PP_ERROR_FAILED;
  }
}

}  // namespace

PPB_FileIO_Impl::PPB_FileIO_Impl(PluginInstance* instance)
    : Resource(instance),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
      file_(base::kInvalidPlatformFileValue),
      callback_(),
      info_(NULL),
      read_buffer_(NULL) {
}

PPB_FileIO_Impl::~PPB_FileIO_Impl() {
  Close();
}

// static
const PPB_FileIO_Dev* PPB_FileIO_Impl::GetInterface() {
  return &ppb_fileio;
}

// static
const PPB_FileIOTrusted_Dev* PPB_FileIO_Impl::GetTrustedInterface() {
  return &ppb_fileiotrusted;
}

PPB_FileIO_Impl* PPB_FileIO_Impl::AsPPB_FileIO_Impl() {
  return this;
}

int32_t PPB_FileIO_Impl::Open(PPB_FileRef_Impl* file_ref,
                              int32_t open_flags,
                              PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(false, callback);
  if (rv != PP_OK)
    return rv;

  int flags = 0;
  if (open_flags & PP_FILEOPENFLAG_READ)
    flags |= base::PLATFORM_FILE_READ;
  if (open_flags & PP_FILEOPENFLAG_WRITE) {
    flags |= base::PLATFORM_FILE_WRITE;
    flags |= base::PLATFORM_FILE_WRITE_ATTRIBUTES;
  }

  if (open_flags & PP_FILEOPENFLAG_TRUNCATE) {
    DCHECK(open_flags & PP_FILEOPENFLAG_WRITE);
    flags |= base::PLATFORM_FILE_TRUNCATE;
  } else if (open_flags & PP_FILEOPENFLAG_CREATE) {
    if (open_flags & PP_FILEOPENFLAG_EXCLUSIVE)
      flags |= base::PLATFORM_FILE_CREATE;
    else
      flags |= base::PLATFORM_FILE_OPEN_ALWAYS;
  } else {
    flags |= base::PLATFORM_FILE_OPEN;
  }

  file_system_type_ = file_ref->GetFileSystemType();
  if (!instance()->delegate()->AsyncOpenFile(
          file_ref->GetSystemPath(), flags,
          callback_factory_.NewCallback(
              &PPB_FileIO_Impl::AsyncOpenFileCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_FileIO_Impl::Query(PP_FileInfo_Dev* info,
                               PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, callback);
  if (rv != PP_OK)
    return rv;

  if (!info)
    return PP_ERROR_BADARGUMENT;

  DCHECK(!info_);  // If |info_|, a callback should be pending (caught above).
  info_ = info;

  if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
          instance()->delegate()->GetFileThreadMessageLoopProxy(), file_,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::QueryInfoCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_FileIO_Impl::Touch(PP_Time last_access_time,
                      PP_Time last_modified_time,
                      PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, callback);
  if (rv != PP_OK)
    return rv;

  if (!base::FileUtilProxy::Touch(
          instance()->delegate()->GetFileThreadMessageLoopProxy(),
          file_, base::Time::FromDoubleT(last_access_time),
          base::Time::FromDoubleT(last_modified_time),
          callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_FileIO_Impl::Read(int64_t offset,
                              char* buffer,
                              int32_t bytes_to_read,
                              PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, callback);
  if (rv != PP_OK)
    return rv;

  // If |read_buffer__|, a callback should be pending (caught above).
  DCHECK(!read_buffer_);
  read_buffer_ = buffer;

  if (!base::FileUtilProxy::Read(
          instance()->delegate()->GetFileThreadMessageLoopProxy(),
          file_, offset, bytes_to_read,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::ReadCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_FileIO_Impl::Write(int64_t offset,
                               const char* buffer,
                               int32_t bytes_to_write,
                               PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, callback);
  if (rv != PP_OK)
    return rv;

  if (!base::FileUtilProxy::Write(
          instance()->delegate()->GetFileThreadMessageLoopProxy(),
          file_, offset, buffer, bytes_to_write,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::WriteCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_FileIO_Impl::SetLength(int64_t length,
                          PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, callback);
  if (rv != PP_OK)
    return rv;

  if (!base::FileUtilProxy::Truncate(
          instance()->delegate()->GetFileThreadMessageLoopProxy(),
          file_, length,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_FileIO_Impl::Flush(PP_CompletionCallback callback) {
  int32_t rv = CommonCallValidation(true, callback);
  if (rv != PP_OK)
    return rv;

  if (!base::FileUtilProxy::Flush(
          instance()->delegate()->GetFileThreadMessageLoopProxy(), file_,
          callback_factory_.NewCallback(&PPB_FileIO_Impl::StatusCallback)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

void PPB_FileIO_Impl::Close() {
  if (file_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        instance()->delegate()->GetFileThreadMessageLoopProxy(), file_, NULL);
    file_ = base::kInvalidPlatformFileValue;
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
  // TODO(dumi): implement me
  return PP_OK;
}

int32_t PPB_FileIO_Impl::WillSetLength(int64_t length,
                                       PP_CompletionCallback callback) {
  // TODO(dumi): implement me
  return PP_OK;
}

int32_t PPB_FileIO_Impl::CommonCallValidation(bool should_be_open,
                                              PP_CompletionCallback callback) {
  // Only asynchronous operation is supported.
  if (!callback.func) {
    NOTIMPLEMENTED();
    return PP_ERROR_BADARGUMENT;
  }

  if (should_be_open) {
    if (file_ == base::kInvalidPlatformFileValue)
      return PP_ERROR_FAILED;
  } else {
    if (file_ != base::kInvalidPlatformFileValue)
      return PP_ERROR_FAILED;
  }

  if (callback_.get() && !callback_->completed())
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void PPB_FileIO_Impl::RegisterCallback(PP_CompletionCallback callback) {
  DCHECK(callback.func);
  DCHECK(!callback_.get() || callback_->completed());

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
}

void PPB_FileIO_Impl::RunPendingCallback(int32_t result) {
  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(callback_);
  callback->Run(result);  // Will complete abortively if necessary.
}

void PPB_FileIO_Impl::StatusCallback(base::PlatformFileError error_code) {
  RunPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::AsyncOpenFileCallback(
    base::PlatformFileError error_code,
    base::PlatformFile file) {
  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file;
  RunPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::QueryInfoCallback(
    base::PlatformFileError error_code,
    const base::PlatformFileInfo& file_info) {
  DCHECK(info_);
  if (error_code == base::PLATFORM_FILE_OK) {
    info_->size = file_info.size;
    info_->creation_time = file_info.creation_time.ToDoubleT();
    info_->last_access_time = file_info.last_accessed.ToDoubleT();
    info_->last_modified_time = file_info.last_modified.ToDoubleT();
    info_->system_type = file_system_type_;
    if (file_info.is_directory)
      info_->type = PP_FILETYPE_DIRECTORY;
    else
      info_->type = PP_FILETYPE_REGULAR;
  }
  info_ = NULL;
  RunPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void PPB_FileIO_Impl::ReadCallback(base::PlatformFileError error_code,
                                   const char* data, int bytes_read) {
  DCHECK(data);
  DCHECK(read_buffer_);

  int rv;
  if (error_code == base::PLATFORM_FILE_OK) {
    rv = bytes_read;
    if (file_ != base::kInvalidPlatformFileValue)
      memcpy(read_buffer_, data, bytes_read);
  } else
    rv = PlatformFileErrorToPepperError(error_code);

  read_buffer_ = NULL;
  RunPendingCallback(rv);
}

void PPB_FileIO_Impl::WriteCallback(base::PlatformFileError error_code,
                                    int bytes_written) {
  if (error_code != base::PLATFORM_FILE_OK)
    RunPendingCallback(PlatformFileErrorToPepperError(error_code));
  else
    RunPendingCallback(bytes_written);
}

}  // namespace ppapi
}  // namespace webkit
