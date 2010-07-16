/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_H_

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
class Plugin;
class SocketAddress;
class SrtSocket;
class ScriptableHandle;

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  // TODO(sehr): This class should also implement factory methods, using the
  // current contents of the Start methods below.
  ServiceRuntime(BrowserInterface* browser_interface, Plugin* plugin);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  //  The constructor is passed the name of the nacl_file (from the
  //  browser cache, typically).  It spawns a sel_ldr instance and establishes
  //  a ConnectedSocket to it.
  bool Start(const char* nacl_file);
  bool Start(const char* url, nacl::DescWrapper *);

  bool Kill();
  bool Log(int severity, nacl::string msg);
  ScriptableHandle* default_socket_address() const;
  ScriptableHandle* default_socket() const;
  ScriptableHandle* GetSocketAddress(Plugin* plugin, nacl::Handle channel);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  // We need two IMC sockets rather than one because IMC sockets are
  // not full-duplex on Windows.
  // See http://code.google.com/p/nativeclient/issues/detail?id=690.
  // TODO(mseaborn): We should not have to work around this.
  nacl::DescWrapper* async_receive_desc;
  nacl::DescWrapper* async_send_desc;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool InitCommunication(nacl::DescWrapper*);
  BrowserInterface* browser_interface_;
  ScriptableHandle* default_socket_address_;
  ScriptableHandle* default_socket_;
  Plugin* plugin_;
  SrtSocket* runtime_channel_;
  nacl::SelLdrLauncher* subprocess_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SERVICE_RUNTIME_H_
