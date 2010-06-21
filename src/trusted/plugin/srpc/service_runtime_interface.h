/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl {
class DescWrapper;
struct SelLdrLauncher;
}  // namespace


namespace plugin {

class BrowserInterface;
class ConnectedSocket;
class MultimediaSocket;
class Plugin;
class SocketAddress;
class SrtSocket;
class ScriptableHandle;

//  ServiceRuntimeInterface abstracts a NativeClient sel_ldr instance.
//  TODO(sehr): this name does not conform to the standard.  Change to remove
//  Interface from the name and move the files.
class ServiceRuntimeInterface {
 public:
  //  The constructor is passed the name of the nacl_file (from the
  //  browser cache, typically).  It spawns a sel_ldr instance and establishes
  //  a ConnectedSocket to it.
  ServiceRuntimeInterface(BrowserInterface* browser_interface,
                          Plugin* plugin);

  //  The destructor terminates the sel_ldr.
  ~ServiceRuntimeInterface();

  bool Start(const char* nacl_file);
  bool Start(const char* url, nacl::DescWrapper *);
  bool Kill();
  bool Log(int severity, nacl::string msg);
  ScriptableHandle* default_socket_address() const;
  ScriptableHandle* default_socket() const;
  ScriptableHandle* GetSocketAddress(Plugin* plugin, nacl::Handle channel);
  Plugin* plugin() const { return plugin_; }
  bool Shutdown();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntimeInterface);
  bool InitCommunication(nacl::DescWrapper*);
  BrowserInterface* browser_interface_;
  ScriptableHandle* default_socket_address_;
  ScriptableHandle* default_socket_;
  Plugin* plugin_;
  SrtSocket* runtime_channel_;
  MultimediaSocket* multimedia_channel_;
  nacl::SelLdrLauncher* subprocess_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_INTERFACE_H_
