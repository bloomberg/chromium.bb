// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_SYSTEM_API_H_
#define PPAPI_THUNK_PPB_FILE_SYSTEM_API_H_

#include "ppapi/c/dev/ppb_file_system_dev.h"

namespace ppapi {
namespace thunk {

class PPB_FileSystem_API {
 public:
  virtual ~PPB_FileSystem_API() {}

  virtual int32_t Open(int64_t expected_size,
                       PP_CompletionCallback callback) = 0;
  virtual PP_FileSystemType_Dev GetType() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_SYSTEM_API_H_
