/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "ppapi/c/pp_errors.h"  // for PP_OK
#include "ppapi/cpp/core.h"  // for pp::
#include "ppapi/cpp/module.h"  // for pp::Module

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
class ServiceRuntime;

// a typesafe utility to schedule a completion callback using weak
// references
template <typename R> bool WeakRefCompletionCallback(
    nacl::WeakRefAnchor* anchor,
    int32_t delay_in_milliseconds,
    void callback_fn(nacl::WeakRef<R>* weak_data, int32_t err),
    R* raw_data) {
  nacl::WeakRef<R>* wp = anchor->MakeWeakRef<R>(raw_data);
  if (wp == NULL) {
    return false;
  }
  pp::CompletionCallback cc(reinterpret_cast<void (*)(void*, int32_t)>(
                                callback_fn),
                            reinterpret_cast<void*>(wp));
  pp::Module::Get()->core()->CallOnMainThread(
      delay_in_milliseconds,
      cc,
      PP_OK);
  return true;
}

class LogToJavaScriptConsoleInterface: public nacl::ReverseInterface {
 public:

  explicit LogToJavaScriptConsoleInterface(
      nacl::WeakRefAnchor* anchor,
      Plugin* plugin)
      : anchor_(anchor),
        plugin_(plugin) {
  }

  virtual ~LogToJavaScriptConsoleInterface() {}

  virtual void Log(nacl::string message);

 private:
  nacl::WeakRefAnchor* anchor_;  // holds a ref
  Plugin* plugin_;  // value may be copied, but should be used only in
                    // main thread in WeakRef-protected callbacks.
};

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  // TODO(sehr): This class should also implement factory methods, using the
  // current contents of the Start methods below.
  explicit ServiceRuntime(Plugin* plugin);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Support for spawning sel_ldr instance and establishing a ConnectedSocket
  // to it. The sel_ldr process can be started directly using a command line
  // or by sending a message to the browser process (in Chrome only).
  // The nexe can be passed:
  // 1) via a local file (typically from browser cache, outside of a sandbox)
  //    as command-line argument
  // 2) as a file descriptor over RPC after sel_ldr launched
  // 3) as shared memory descriptor over RPC after sel_ldr launched
  // These 3 nexe options are mutually exclusive and map to the following usage:
  // 1) StartFromCommandLine(file_path, NULL, error_string)
  // 2) StartFromCommandLine(NACL_NO_FILE_PATH, file_desc, error_string)
  //    StartFromBrowser(url, file_desc, error_string)
  // 3) StartFromBrowser(url, shm_desc, error_string)
  bool StartFromCommandLine(nacl::string nacl_file,
                            nacl::DescWrapper* nacl_file_desc,
                            nacl::string* error_string);
  bool StartFromBrowser(nacl::string nacl_url,
                        nacl::DescWrapper* nacl_desc,
                        nacl::string* error_string);

  bool Kill();
  bool Log(int severity, nacl::string msg);
  ScriptableHandle* default_socket_address() const;
  ScriptableHandle* GetSocketAddress(Plugin* plugin, nacl::Handle channel);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  nacl::DescWrapper* async_receive_desc() { return async_receive_desc_; }
  nacl::DescWrapper* async_send_desc() { return async_send_desc_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool InitCommunication(nacl::Handle bootstrap_socket,
                         nacl::DescWrapper* shm,
                         nacl::string* error_string);

  ScriptableHandle* default_socket_address_;  // creates, but does not own
  Plugin* plugin_;
  BrowserInterface* browser_interface_;
  SrtSocket* runtime_channel_;
  nacl::ReverseService* reverse_service_;
  nacl::SelLdrLauncher* subprocess_;

  // We need two IMC sockets rather than one because IMC sockets are
  // not full-duplex on Windows.
  // See http://code.google.com/p/nativeclient/issues/detail?id=690.
  // TODO(mseaborn): We should not have to work around this.
  nacl::DescWrapper* async_receive_desc_;
  nacl::DescWrapper* async_send_desc_;

  nacl::WeakRefAnchor *anchor_;

  LogToJavaScriptConsoleInterface* log_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
