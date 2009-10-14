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


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_

#include <string>

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
  bool LogAtServiceRuntime(int severity, std::string msg);
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
