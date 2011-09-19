/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// C++ wrapper around C reverse service objects.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SOCKET_H_

#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/trusted/simple_service/nacl_simple_rservice.h"

#include "native_client/src/trusted/threading/nacl_thread_interface.h"

namespace nacl {

class DescWrapper;

// This class is an IMC client that connects to an IMC server but then
// behaves as an SRPC server.  The IMC server runs callbacks that
// takes ownership of the connected socket for use as SRPC clients
// (perhaps passing to other threads).

class ReverseSocket {
 public:

  // Ctor takes ownership of conn_cap.  Both handlers and
  // server_instance_data is shared with the caller and is expected to
  // have lifetimes that are not shorter than the constructed
  // ReverseSocket.
  ReverseSocket(nacl::DescWrapper* conn_cap,
                NaClSrpcHandlerDesc const* handlers,
                NaClThreadIfFactoryFunction thread_factory_fn,
                void* thread_factory_data)
      : conn_cap_(conn_cap),
        handlers_(handlers),
        thread_factory_fn_(thread_factory_fn),
        thread_factory_data_(thread_factory_data),
        rev_service_(NULL) {}

  ~ReverseSocket();  // dtor will delete conn_cap

  bool StartServiceCb(void (*exit_cb)(void *server_instance_data,
                                      int  server_loop_ret),
                      void *server_instance_data);

  /* StartService just invokes StartServiceCb with exit_cb boudn to NULL */
  bool StartService(void* server_instance_data);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ReverseSocket);

  nacl::DescWrapper* conn_cap_;
  NaClSrpcHandlerDesc const* handlers_;

  NaClThreadIfFactoryFunction thread_factory_fn_;
  void* thread_factory_data_;
  NaClSimpleRevService* rev_service_;
};
}  // namespace nacl

#endif
