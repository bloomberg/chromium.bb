// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_TRANSPORT_DEV_H_
#define PPAPI_CPP_DEV_TRANSPORT_DEV_H_

#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;
class Var;

class Transport_Dev : public Resource {
 public:
  Transport_Dev(Instance* instance, const char* name, const char* proto);

  bool IsWritable();
  int32_t Connect(const CompletionCallback& cc);
  int32_t GetNextAddress(pp::Var* address, const CompletionCallback& cc);
  int32_t ReceiveRemoteAddress(const pp::Var& address);
  int32_t Recv(void* data, uint32_t len,
               const CompletionCallback& cc);
  int32_t Send(const void* data, uint32_t len,
               const CompletionCallback& cb);
  int32_t Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_TRANSPORT_DEV_H_
