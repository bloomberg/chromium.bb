// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_io_dev.h"

#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileIO_Dev>() {
  return PPB_FILEIO_DEV_INTERFACE;
}

}  // namespace

FileIO_Dev::FileIO_Dev() {
}

FileIO_Dev::FileIO_Dev(Instance* instance) {
  if (!has_interface<PPB_FileIO_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileIO_Dev>()->Create(
      instance->pp_instance()));
}

FileIO_Dev::FileIO_Dev(const FileIO_Dev& other)
    : Resource(other) {
}

int32_t FileIO_Dev::Open(const FileRef_Dev& file_ref,
                         int32_t open_flags,
                         const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->Open(
      pp_resource(), file_ref.pp_resource(), open_flags,
      cc.pp_completion_callback());
}

int32_t FileIO_Dev::Query(PP_FileInfo_Dev* result_buf,
                          const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->Query(
      pp_resource(), result_buf, cc.pp_completion_callback());
}

int32_t FileIO_Dev::Touch(PP_Time last_access_time,
                          PP_Time last_modified_time,
                          const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->Touch(
      pp_resource(), last_access_time, last_modified_time,
      cc.pp_completion_callback());
}

int32_t FileIO_Dev::Read(int64_t offset,
                         char* buffer,
                         int32_t bytes_to_read,
                         const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->Read(pp_resource(),
      offset, buffer, bytes_to_read, cc.pp_completion_callback());
}

int32_t FileIO_Dev::Write(int64_t offset,
                          const char* buffer,
                          int32_t bytes_to_write,
                          const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->Write(
      pp_resource(), offset, buffer, bytes_to_write,
      cc.pp_completion_callback());
}

int32_t FileIO_Dev::SetLength(int64_t length,
                          const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->SetLength(
      pp_resource(), length, cc.pp_completion_callback());
}

int32_t FileIO_Dev::Flush(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO_Dev>()->Flush(
      pp_resource(), cc.pp_completion_callback());
}

void FileIO_Dev::Close() {
  if (!has_interface<PPB_FileIO_Dev>())
    return;
  get_interface<PPB_FileIO_Dev>()->Close(pp_resource());
}

}  // namespace pp
