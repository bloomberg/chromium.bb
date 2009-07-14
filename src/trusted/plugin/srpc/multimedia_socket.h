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


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_

#include "native_client/src/trusted/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

namespace nacl_srpc {

class ServiceRuntimeInterface;

class MultimediaSocket {
 public:
  MultimediaSocket(ScriptableHandle<ConnectedSocket>* s,
                   PortablePluginInterface* plugin_interface,
                   ServiceRuntimeInterface* serv_rtm_info);

  ~MultimediaSocket();

  // Routine for starting the upcall handler thread (used by multimedia)
  bool InitializeModuleMultimedia(Plugin* plugin);

  // accessor
  ConnectedSocket *connected_socket() const {
    return static_cast<ConnectedSocket*>(connected_socket_->get_handle());
  }

  // Not really constants.  Do not modify.  Use only after at least
  // one MultimediaSocket instance has been constructed.
  static int kNaClMultimediaBridgeIdent;

  void UpcallThreadExiting();
  bool UpcallThreadShouldExit();
  void set_upcall_thread_id(uint32_t tid);

 private:
  ScriptableHandle<ConnectedSocket>* connected_socket_;
  struct NaClThread upcall_thread_;

  PortablePluginInterface* plugin_interface_;
  ServiceRuntimeInterface* service_runtime_;

  struct NaClMutex  mu_;
  struct NaClCondVar cv_;
  enum UpcallThreadState {
    UPCALL_THREAD_NOT_STARTED,
    UPCALL_THREAD_RUNNING,
    UPCALL_THREAD_EXITED
  } upcall_thread_state_;

  bool upcall_thread_should_exit_;
  uint32_t upcall_thread_id_;

  static void InitializeIdentifiers(PortablePluginInterface* plugin_interface);

  static int const kMaxUpcallThreadWaitSec = 5;
  static int const kNanoXinMicroX = 1000;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_
