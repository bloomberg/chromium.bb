// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {
namespace protocol {

FileTransfer_Error MakeFileTransferError(
    base::Location location,
    FileTransfer_Error_Type type,
    base::Optional<int32_t> api_error_code) {
  FileTransfer_Error error;
  error.set_type(type);
  if (api_error_code) {
    error.set_api_error_code(*api_error_code);
  }
  error.set_function(location.function_name());
  error.set_source_file(location.file_name());
  error.set_line_number(location.line_number());
  return error;
}

}  // namespace protocol
}  // namespace remoting
