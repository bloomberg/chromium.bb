// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_FILE_IO_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_FILE_IO_PRIVATE_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"

namespace pp {

class FileIO;

class FileIO_Private : public FileIO {
 public:
  FileIO_Private();
  explicit FileIO_Private(const InstanceHandle& instance);

  // This interface may leak the memory and file handle if the factory
  // of |cc| is destructed.  We need to use
  // CompletionCallbackWithOutput to prevent the memory leak and
  // create something like PassFileHandle to prevent the handle leak.
  // See https://codereview.chromium.org/13032002/#ps33001
  //
  // Currently, TestCompletionCallbackWithOutput won't work for non
  // vector types, so we should fix it first.
  // http://crbug.com/224741
  //
  // Meanwhile, this interface should be used only from tests and
  // other code should use the C interface of this API.
  //
  // TODO(hamaji): Fix. http://crbug.com/224745
  int32_t RequestOSFileHandle(PP_FileHandle* result_handle,
                              const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FILE_IO_PRIVATE_H_
