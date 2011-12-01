// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/file_io.h"

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileIO>() {
  return PPB_FILEIO_INTERFACE;
}

}  // namespace

FileIO::FileIO() {
}

FileIO::FileIO(Instance* instance) {
  if (!has_interface<PPB_FileIO>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileIO>()->Create(
      instance->pp_instance()));
}

FileIO::FileIO(const FileIO& other)
    : Resource(other) {
}

int32_t FileIO::Open(const FileRef& file_ref,
                     int32_t open_flags,
                     const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->Open(
      pp_resource(), file_ref.pp_resource(), open_flags,
      cc.pp_completion_callback());
}

int32_t FileIO::Query(PP_FileInfo* result_buf,
                      const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->Query(
      pp_resource(), result_buf, cc.pp_completion_callback());
}

int32_t FileIO::Touch(PP_Time last_access_time,
                      PP_Time last_modified_time,
                      const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->Touch(
      pp_resource(), last_access_time, last_modified_time,
      cc.pp_completion_callback());
}

int32_t FileIO::Read(int64_t offset,
                     char* buffer,
                     int32_t bytes_to_read,
                     const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->Read(pp_resource(),
      offset, buffer, bytes_to_read, cc.pp_completion_callback());
}

int32_t FileIO::Write(int64_t offset,
                      const char* buffer,
                      int32_t bytes_to_write,
                      const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->Write(
      pp_resource(), offset, buffer, bytes_to_write,
      cc.pp_completion_callback());
}

int32_t FileIO::SetLength(int64_t length,
                          const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->SetLength(
      pp_resource(), length, cc.pp_completion_callback());
}

int32_t FileIO::Flush(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileIO>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIO>()->Flush(
      pp_resource(), cc.pp_completion_callback());
}

void FileIO::Close() {
  if (!has_interface<PPB_FileIO>())
    return;
  get_interface<PPB_FileIO>()->Close(pp_resource());
}

}  // namespace pp
