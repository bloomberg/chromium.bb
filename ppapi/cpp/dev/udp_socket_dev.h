// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_UDP_SOCKET_DEV_H_
#define PPAPI_CPP_DEV_UDP_SOCKET_DEV_H_

#include "ppapi/c/dev/ppb_udp_socket_dev.h"
#include "ppapi/cpp/dev/net_address_dev.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;
class Var;

template <typename T> class CompletionCallbackWithOutput;

class UDPSocket_Dev: public Resource {
 public:
  UDPSocket_Dev();

  explicit UDPSocket_Dev(const InstanceHandle& instance);

  UDPSocket_Dev(PassRef, PP_Resource resource);

  UDPSocket_Dev(const UDPSocket_Dev& other);

  virtual ~UDPSocket_Dev();

  UDPSocket_Dev& operator=(const UDPSocket_Dev& other);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t Bind(const NetAddress_Dev& addr,
               const CompletionCallback& callback);
  NetAddress_Dev GetBoundAddress();
  int32_t RecvFrom(
      char* buffer,
      int32_t num_bytes,
      const CompletionCallbackWithOutput<NetAddress_Dev>& callback);
  int32_t SendTo(const char* buffer,
                 int32_t num_bytes,
                 const NetAddress_Dev& addr,
                 const CompletionCallback& callback);
  void Close();
  int32_t SetOption(PP_UDPSocket_Option_Dev name,
                    const Var& value,
                    const CompletionCallback& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_UDP_SOCKET_DEV_H_
