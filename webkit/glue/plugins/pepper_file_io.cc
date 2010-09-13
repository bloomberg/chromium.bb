// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_io.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/logging.h"
#include "base/time.h"
#include "third_party/ppapi/c/dev/ppb_file_io_dev.h"
#include "third_party/ppapi/c/dev/ppb_file_io_trusted_dev.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

namespace {

PP_Resource Create(PP_Module module_id) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;

  FileIO* file_io = new FileIO(module);
  return file_io->GetReference();
}

bool IsFileIO(PP_Resource resource) {
  return !!Resource::GetAs<FileIO>(resource);
}

int32_t Open(PP_Resource file_io_id,
             PP_Resource file_ref_id,
             int32_t open_flags,
             PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  return file_io->Open(file_ref, open_flags, callback);
}

int32_t Query(PP_Resource file_io_id,
              PP_FileInfo_Dev* info,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Query(info, callback);
}

int32_t Touch(PP_Resource file_io_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Touch(last_access_time, last_modified_time, callback);
}

int32_t Read(PP_Resource file_io_id,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Read(offset, buffer, bytes_to_read, callback);
}

int32_t Write(PP_Resource file_io_id,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Write(offset, buffer, bytes_to_write, callback);
}

int32_t SetLength(PP_Resource file_io_id,
                  int64_t length,
                  PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->SetLength(length, callback);
}

int32_t Flush(PP_Resource file_io_id,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->Flush(callback);
}

void Close(PP_Resource file_io_id) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
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
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->GetOSFileDescriptor();
}

int32_t WillWrite(PP_Resource file_io_id,
                  int64_t offset,
                  int32_t bytes_to_write,
                  PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;
  return file_io->WillWrite(offset, bytes_to_write, callback);
}

int32_t WillSetLength(PP_Resource file_io_id,
                      int64_t length,
                      PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
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

FileIO::FileIO(PluginModule* module)
    : Resource(module),
      delegate_(module->GetSomeInstance()->delegate()),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
      file_(base::kInvalidPlatformFileValue),
      callback_(),
      info_(NULL) {
}

FileIO::~FileIO() {
  Close();
}

// static
const PPB_FileIO_Dev* FileIO::GetInterface() {
  return &ppb_fileio;
}

// static
const PPB_FileIOTrusted_Dev* FileIO::GetTrustedInterface() {
  return &ppb_fileiotrusted;
}

int32_t FileIO::Open(FileRef* file_ref,
                     int32_t open_flags,
                     PP_CompletionCallback callback) {
  if (file_ != base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  int flags = 0;
  if (open_flags & PP_FILEOPENFLAG_READ)
    flags |= base::PLATFORM_FILE_READ;
  if (open_flags & PP_FILEOPENFLAG_WRITE) {
    flags |= base::PLATFORM_FILE_WRITE;
    flags |= base::PLATFORM_FILE_WRITE_ATTRIBUTES;
  }
  if (open_flags & PP_FILEOPENFLAG_TRUNCATE) {
    DCHECK(flags & PP_FILEOPENFLAG_WRITE);
    flags |= base::PLATFORM_FILE_TRUNCATE;
  }

  if (open_flags & PP_FILEOPENFLAG_CREATE) {
    if (open_flags & PP_FILEOPENFLAG_EXCLUSIVE)
      flags |= base::PLATFORM_FILE_CREATE;
    else
      flags |= base::PLATFORM_FILE_OPEN_ALWAYS;
  } else
    flags |= base::PLATFORM_FILE_OPEN;

  file_system_type_ = file_ref->file_system_type();
  if (!delegate_->AsyncOpenFile(
          file_ref->system_path(), flags,
          callback_factory_.NewCallback(&FileIO::AsyncOpenFileCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t FileIO::Query(PP_FileInfo_Dev* info,
                      PP_CompletionCallback callback) {
  if (file_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  DCHECK(!info_);
  DCHECK(info);
  info_ = info;

  if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
          delegate_->GetFileThreadMessageLoopProxy(), file_,
          callback_factory_.NewCallback(&FileIO::QueryInfoCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t FileIO::Touch(PP_Time last_access_time,
                      PP_Time last_modified_time,
                      PP_CompletionCallback callback) {
  if (file_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  if (!base::FileUtilProxy::Touch(
          delegate_->GetFileThreadMessageLoopProxy(),
          file_, base::Time::FromDoubleT(last_access_time),
          base::Time::FromDoubleT(last_modified_time),
          callback_factory_.NewCallback(&FileIO::StatusCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t FileIO::Read(int64_t offset,
                     char* buffer,
                     int32_t bytes_to_read,
                     PP_CompletionCallback callback) {
  if (file_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  if (!base::FileUtilProxy::Read(
          delegate_->GetFileThreadMessageLoopProxy(),
          file_, offset, buffer, bytes_to_read,
          callback_factory_.NewCallback(&FileIO::ReadWriteCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t FileIO::Write(int64_t offset,
                      const char* buffer,
                      int32_t bytes_to_write,
                      PP_CompletionCallback callback) {
  if (file_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  if (!base::FileUtilProxy::Write(
          delegate_->GetFileThreadMessageLoopProxy(),
          file_, offset, buffer, bytes_to_write,
          callback_factory_.NewCallback(&FileIO::ReadWriteCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t FileIO::SetLength(int64_t length,
                          PP_CompletionCallback callback) {
  if (file_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  if (!base::FileUtilProxy::Truncate(
          delegate_->GetFileThreadMessageLoopProxy(),
          file_, length,
          callback_factory_.NewCallback(&FileIO::StatusCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t FileIO::Flush(PP_CompletionCallback callback) {
  if (file_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  DCHECK(!callback_.func);
  callback_ = callback;

  if (!base::FileUtilProxy::Flush(
          delegate_->GetFileThreadMessageLoopProxy(), file_,
          callback_factory_.NewCallback(&FileIO::StatusCallback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

void FileIO::Close() {
  if (file_ != base::kInvalidPlatformFileValue)
    base::FileUtilProxy::Close(
        delegate_->GetFileThreadMessageLoopProxy(), file_, NULL);
}

int32_t FileIO::GetOSFileDescriptor() {
#if defined(OS_POSIX)
  return file_;
#elif defined(OS_WIN)
  return reinterpret_cast<uintptr_t>(file_);
#else
#error "Platform not supported."
#endif
}

int32_t FileIO::WillWrite(int64_t offset,
                          int32_t bytes_to_write,
                          PP_CompletionCallback callback) {
  // TODO(dumi): implement me
  return PP_OK;
}

int32_t FileIO::WillSetLength(int64_t length,
                              PP_CompletionCallback callback) {
  // TODO(dumi): implement me
  return PP_OK;
}

void FileIO::RunPendingCallback(int result) {
  if (!callback_.func)
    return;

  PP_CompletionCallback callback = {0};
  std::swap(callback, callback_);
  PP_RunCompletionCallback(&callback, result);
}

void FileIO::StatusCallback(base::PlatformFileError error_code) {
  RunPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void FileIO::AsyncOpenFileCallback(base::PlatformFileError error_code,
                                   base::PlatformFile file) {
  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file;
  RunPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void FileIO::QueryInfoCallback(base::PlatformFileError error_code,
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
  RunPendingCallback(PlatformFileErrorToPepperError(error_code));
}

void FileIO::ReadWriteCallback(base::PlatformFileError error_code,
                               int bytes_read_or_written) {
  if (error_code != base::PLATFORM_FILE_OK)
    RunPendingCallback(PlatformFileErrorToPepperError(error_code));
  else
    RunPendingCallback(bytes_read_or_written);
}

}  // namespace pepper
