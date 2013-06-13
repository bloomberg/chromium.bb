// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_TCP_SOCKET_DEV_H_
#define PPAPI_CPP_DEV_TCP_SOCKET_DEV_H_

#include "ppapi/c/dev/ppb_tcp_socket_dev.h"
#include "ppapi/cpp/dev/net_address_dev.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;

class TCPSocket_Dev: public Resource {
 public:
  TCPSocket_Dev();

  explicit TCPSocket_Dev(const InstanceHandle& instance);

  TCPSocket_Dev(PassRef, PP_Resource resource);

  TCPSocket_Dev(const TCPSocket_Dev& other);

  virtual ~TCPSocket_Dev();

  TCPSocket_Dev& operator=(const TCPSocket_Dev& other);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t Connect(const NetAddress_Dev& addr,
                  const CompletionCallback& callback);
  NetAddress_Dev GetLocalAddress() const;
  NetAddress_Dev GetRemoteAddress() const;
  int32_t Read(char* buffer,
               int32_t bytes_to_read,
               const CompletionCallback& callback);
  int32_t Write(const char* buffer,
                int32_t bytes_to_write,
                const CompletionCallback& callback);
  void Close();
  int32_t SetOption(PP_TCPSocket_Option_Dev name,
                    const Var& value,
                    const CompletionCallback& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_TCP_SOCKET_DEV_H_
