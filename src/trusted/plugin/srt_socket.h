/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A representation of a connected socket connection to the service runtime.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRT_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRT_SOCKET_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"

struct NaClDesc;

namespace plugin {

class SrtSocket {
 public:
  SrtSocket(ScriptableHandle* s, BrowserInterface* browser_interface);
  ~SrtSocket();

  bool StartModule(int* load_status);
  bool ReverseSetup(NaClSrpcImcDescType *out_conn);
  bool LoadModule(NaClDesc* desc);
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  bool InitHandlePassing(NaClDesc* desc, nacl::Handle sel_ldr_handle);
#endif
  bool Log(int severity, nacl::string msg);

  PortableHandle* connected_socket() const {
    return connected_socket_->handle();
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SrtSocket);
  ScriptableHandle* connected_socket_;
  BrowserInterface* browser_interface_;
};  // class SrtSocket

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRT_SOCKET_H_
