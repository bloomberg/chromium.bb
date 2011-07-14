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
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/weak_ref/weak_ref.h"

namespace nacl {
class DescWrapper;
struct SelLdrLauncher;
}  // namespace

namespace plugin {

class BrowserInterface;
class ErrorInfo;
class Plugin;
class SrpcClient;
class SrtSocket;
class ScriptableHandle;
class ServiceRuntime;

class PluginReverseInterface: public nacl::ReverseInterface {
 public:
  PluginReverseInterface(nacl::WeakRefAnchor* anchor,
                         Plugin* plugin)
      : anchor_(anchor),
        plugin_(plugin) {
  }

  virtual ~PluginReverseInterface() {}

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
  // Start method below.
  explicit ServiceRuntime(Plugin* plugin);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Spawn a sel_ldr instance and establish an SrpcClient to it.  The nexe
  // to be started is passed through |nacl_file_desc|.  On success, returns
  // true.  On failure, returns false and |error_string| is set to something
  // describing the error.
  bool Start(nacl::DescWrapper* nacl_file_desc, ErrorInfo* error_info);

  // Starts the application channel to the nexe.
  SrpcClient* SetupAppChannel();

  bool Kill();
  bool Log(int severity, nacl::string msg);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  nacl::DescWrapper* async_receive_desc() { return async_receive_desc_.get(); }
  nacl::DescWrapper* async_send_desc() { return async_send_desc_.get(); }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool InitCommunication(nacl::DescWrapper* shm, ErrorInfo* error_info);

  NaClSrpcChannel command_channel_;
  Plugin* plugin_;
  BrowserInterface* browser_interface_;
  nacl::ReverseService* reverse_service_;
  nacl::scoped_ptr<nacl::SelLdrLauncher> subprocess_;

  // We need two IMC sockets rather than one because IMC sockets are
  // not full-duplex on Windows.
  // See http://code.google.com/p/nativeclient/issues/detail?id=690.
  // TODO(mseaborn): We should not have to work around this.
  nacl::scoped_ptr<nacl::DescWrapper> async_receive_desc_;
  nacl::scoped_ptr<nacl::DescWrapper> async_send_desc_;

  nacl::WeakRefAnchor *anchor_;

  PluginReverseInterface* rev_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
