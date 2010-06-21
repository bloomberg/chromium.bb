/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// The socket class used for the av library to talk to the plugin.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

namespace plugin {

class BrowserInterface;
class ServiceRuntimeInterface;

class MultimediaSocket {
 public:
  MultimediaSocket(ScriptableHandle* s,
                   BrowserInterface* browser_interface,
                   ServiceRuntimeInterface* serv_rtm_info);

  ~MultimediaSocket();

  // Routine for starting the upcall handler thread (used by multimedia)
  bool InitializeModuleMultimedia(Plugin* plugin);

  // accessor
  ConnectedSocket *connected_socket() const {
    return static_cast<ConnectedSocket*>(connected_socket_->handle());
  }

  void UpcallThreadExiting();
  bool UpcallThreadShouldExit();
  void set_upcall_thread_id(uint32_t tid);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MultimediaSocket);
  ScriptableHandle* connected_socket_;
  struct NaClThread upcall_thread_;

  BrowserInterface* browser_interface_;
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
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_MULTIMEDIA_SOCKET_H_
