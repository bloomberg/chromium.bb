// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_BROKER_API_H_
#define PPAPI_THUNK_PPB_BROKER_API_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_stdint.h"

namespace ppapi {
namespace thunk {

class PPB_Broker_API {
 public:
  virtual int32_t Connect(PP_CompletionCallback connect_callback) = 0;
  virtual int32_t GetHandle(int32_t* handle) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_BROKER_API_H_
