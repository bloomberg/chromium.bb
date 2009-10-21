/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRT_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRT_SOCKET_H_

#include <string>

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
  bool SetOrigin(std::string origin);
  bool StartModule(int *load_status);
  bool LoadModule(NaClSrpcImcDescType desc);
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  bool InitHandlePassing(NaClSrpcImcDescType desc, nacl::Handle sel_ldr_handle);
#endif
  bool Log(int severity, std::string msg);

  ConnectedSocket *connected_socket() const {
    return static_cast<ConnectedSocket*>(connected_socket_->get_handle());
  }

  // Not really constants.  Do not modify.  Use only after at least
  // one SrtSocket instance has been constructed.
  static int kHardShutdownIdent;
  static int kSetOriginIdent;
  static int kStartModuleIdent;
  static int kLogIdent;
  static int kLoadModule;
  static int kInitHandlePassing;
 private:
  ScriptableHandle<ConnectedSocket> *connected_socket_;
  PortablePluginInterface *plugin_interface_;
  bool is_shut_down_;

  static void InitializeIdentifiers(PortablePluginInterface *plugin_interface);
};  // class SrtSocket

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRT_SOCKET_H_
