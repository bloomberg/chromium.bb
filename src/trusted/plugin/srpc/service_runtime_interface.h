/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl {
struct SelLdrLauncher;
}  // namespace nacl

namespace nacl_srpc {

//  Class declarations.
class Plugin;
class ConnectedSocket;
class MultimediaSocket;
class SocketAddress;
class SrtSocket;

template <typename HandleType>
class ScriptableHandle;

//  ServiceRuntimeInterface abstracts a NativeClient sel_ldr instance.
class ServiceRuntimeInterface {
 public:
  //  The constructor is passed the name of the nacl_file (from the
  //  browser cache, typically).  It spawns a sel_ldr instance and establishes
  //  a ConnectedSocket to it.
  ServiceRuntimeInterface(PortablePluginInterface* plugin_interface,
                          Plugin* plugin);

  //  The destructor terminates the sel_ldr.
  ~ServiceRuntimeInterface();

  bool Start(const char* nacl_file);
  bool Start(const char* url, const void* buffer, int32_t size);
  bool Kill();
  bool LogAtServiceRuntime(int severity, nacl::string msg);
  ScriptableHandle<SocketAddress>* default_socket_address() const;
  ScriptableHandle<ConnectedSocket>* default_socket() const;
  ScriptableHandle<SocketAddress>* GetSocketAddress(Plugin* plugin,
                                                    NaClHandle channel);
  Plugin* plugin() const { return plugin_; }
  bool Shutdown();
 private:
  bool InitCommunication(const void* buffer, int32_t size);
 private:
  PortablePluginInterface* plugin_interface_;
  ScriptableHandle<SocketAddress>* default_socket_address_;
  ScriptableHandle<ConnectedSocket>* default_socket_;
  Plugin* plugin_;
  SrtSocket* runtime_channel_;
  MultimediaSocket* multimedia_channel_;
  nacl::SelLdrLauncher* subprocess_;
  static int number_alive_counter;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_
