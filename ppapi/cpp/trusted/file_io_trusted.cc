// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/trusted/file_io_trusted.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileIOTrusted>() {
  return PPB_FILEIOTRUSTED_INTERFACE_0_4;
}

}  // namespace

FileIO_Trusted::FileIO_Trusted() {
}

int32_t FileIO_Trusted::GetOSFileDescriptor(const FileIO& file_io) {
  const int32_t kInvalidOSFileDescriptor = -1;
  if (!has_interface<PPB_FileIOTrusted>())
    return kInvalidOSFileDescriptor;
  return get_interface<PPB_FileIOTrusted>()->GetOSFileDescriptor(
      file_io.pp_resource());
}

int32_t FileIO_Trusted::WillWrite(const FileIO& file_io,
                                  int64_t offset,
                                  int32_t bytes_to_write,
                                  const CompletionCallback& callback) {
  if (!has_interface<PPB_FileIOTrusted>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIOTrusted>()->WillWrite(
      file_io.pp_resource(), offset, bytes_to_write,
      callback.pp_completion_callback());
}

int32_t FileIO_Trusted::WillSetLength(const FileIO& file_io,
                                      int64_t length,
                                      const CompletionCallback& callback) {
  if (!has_interface<PPB_FileIOTrusted>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileIOTrusted>()->WillSetLength(
      file_io.pp_resource(), length, callback.pp_completion_callback());
}

}  // namespace pp
