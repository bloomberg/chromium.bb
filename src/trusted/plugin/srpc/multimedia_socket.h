/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_

#include "native_client/src/shared/platform/nacl_sync_checked.h"
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
