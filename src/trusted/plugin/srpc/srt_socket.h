/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRT_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRT_SOCKET_H_

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

namespace nacl_srpc {

class SrtSocket {
 public:
  SrtSocket(ScriptableHandle<ConnectedSocket> *s,
            PortablePluginInterface* plugin_interface);
  ~SrtSocket();

  bool HardShutdown();
  bool SetOrigin(nacl::string origin);
  bool StartModule(int *load_status);
  bool LoadModule(NaClSrpcImcDescType desc);
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  bool InitHandlePassing(NaClSrpcImcDescType desc, nacl::Handle sel_ldr_handle);
#endif
  bool Log(int severity, nacl::string msg);

  ConnectedSocket *connected_socket() const {
    return static_cast<ConnectedSocket*>(connected_socket_->get_handle());
  }

  // Not really constants.  Do not modify.  Use only after at least
  // one SrtSocket instance has been constructed.
  static uintptr_t kHardShutdownIdent;
  static uintptr_t kSetOriginIdent;
  static uintptr_t kStartModuleIdent;
  static uintptr_t kLogIdent;
  static uintptr_t kLoadModule;
  static uintptr_t kInitHandlePassing;
 private:
  ScriptableHandle<ConnectedSocket> *connected_socket_;
  PortablePluginInterface *plugin_interface_;
  bool is_shut_down_;

  static void InitializeIdentifiers(PortablePluginInterface *plugin_interface);
};  // class SrtSocket

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRT_SOCKET_H_
