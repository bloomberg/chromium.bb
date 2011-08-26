// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TRANSPORT_API_H_
#define PPAPI_THUNK_PPB_TRANSPORT_API_H_

#include "ppapi/c/dev/ppb_transport_dev.h"

namespace ppapi {
namespace thunk {

class PPB_Transport_API {
 public:
  virtual ~PPB_Transport_API() {}

  virtual PP_Bool IsWritable() = 0;
  virtual int32_t SetProperty(PP_TransportProperty property, PP_Var value) = 0;
  virtual int32_t Connect(PP_CompletionCallback callback) = 0;
  virtual int32_t GetNextAddress(PP_Var* address,
                                 PP_CompletionCallback callback) = 0;
  virtual int32_t ReceiveRemoteAddress(PP_Var address) = 0;
  virtual int32_t Recv(void* data, uint32_t len,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t Send(const void* data, uint32_t len,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TRANSPORT_API_H_
