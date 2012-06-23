// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_BROKER_API_H_
#define PPAPI_THUNK_PPB_BROKER_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_stdint.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_Broker_API {
 public:
  virtual ~PPB_Broker_API() {}

  virtual int32_t Connect(scoped_refptr<TrackedCallback> connect_callback) = 0;
  virtual int32_t GetHandle(int32_t* handle) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_BROKER_API_H_
