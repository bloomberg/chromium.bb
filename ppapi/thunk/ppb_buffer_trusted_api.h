// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_BUFFER_TRUSTED_API_H_
#define PPAPI_THUNK_BUFFER_TRUSTED_API_H_

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/trusted/ppb_buffer_trusted.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_BufferTrusted_API {
 public:
  virtual ~PPB_BufferTrusted_API() {}

  virtual int32_t GetSharedMemory(int* handle) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_BUFFER_TRUSTED_API_H_
