// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_io_dev.h"

#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_FileIO_Dev> file_io_f(
    PPB_FILEIO_DEV_INTERFACE);
DeviceFuncs<PPB_FileIOTrusted_Dev> file_io_trusted_f(
    PPB_FILEIOTRUSTED_DEV_INTERFACE);

}  // namespace

namespace pp {

FileIO_Dev::FileIO_Dev() {
  if (!file_io_f)
    return;
  PassRefFromConstructor(file_io_f->Create(Module::Get()->pp_module()));
}

FileIO_Dev::FileIO_Dev(const FileIO_Dev& other)
    : Resource(other) {
}

FileIO_Dev& FileIO_Dev::operator=(const FileIO_Dev& other) {
  FileIO_Dev copy(other);
  swap(copy);
  return *this;
}

void FileIO_Dev::swap(FileIO_Dev& other) {
  Resource::swap(other);
}

int32_t FileIO_Dev::Open(const FileRef_Dev& file_ref,
                         int32_t open_flags,
                         const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->Open(pp_resource(), file_ref.pp_resource(), open_flags,
                         cc.pp_completion_callback());
}

int32_t FileIO_Dev::Query(PP_FileInfo_Dev* result_buf,
                          const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->Query(pp_resource(), result_buf,
                          cc.pp_completion_callback());
}

int32_t FileIO_Dev::Touch(PP_Time last_access_time,
                          PP_Time last_modified_time,
                          const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->Touch(pp_resource(), last_access_time, last_modified_time,
                          cc.pp_completion_callback());
}

int32_t FileIO_Dev::Read(int64_t offset,
                         char* buffer,
                         int32_t bytes_to_read,
                         const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->Read(pp_resource(), offset, buffer, bytes_to_read,
                         cc.pp_completion_callback());
}

int32_t FileIO_Dev::Write(int64_t offset,
                          const char* buffer,
                          int32_t bytes_to_write,
                          const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->Write(pp_resource(), offset, buffer, bytes_to_write,
                          cc.pp_completion_callback());
}

int32_t FileIO_Dev::SetLength(int64_t length,
                          const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->SetLength(pp_resource(), length,
                              cc.pp_completion_callback());
}

int32_t FileIO_Dev::Flush(const CompletionCallback& cc) {
  if (!file_io_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_f->Flush(pp_resource(), cc.pp_completion_callback());
}

void FileIO_Dev::Close() {
  if (!file_io_f)
    return;
  file_io_f->Close(pp_resource());
}

int32_t FileIO_Dev::GetOSFileDescriptor() {
  if (!file_io_trusted_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_trusted_f->GetOSFileDescriptor(pp_resource());
}

int32_t FileIO_Dev::WillWrite(int64_t offset,
                              int32_t bytes_to_write,
                              const CompletionCallback& cc) {
  if (!file_io_trusted_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_trusted_f->WillWrite(pp_resource(), offset, bytes_to_write,
                                      cc.pp_completion_callback());
}

int32_t FileIO_Dev::WillSetLength(int64_t length,
                                  const CompletionCallback& cc) {
  if (!file_io_trusted_f)
    return PP_ERROR_NOINTERFACE;
  return file_io_trusted_f->WillSetLength(pp_resource(), length,
                                          cc.pp_completion_callback());
}

}  // namespace pp
