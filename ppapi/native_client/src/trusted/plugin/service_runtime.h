/* -*- c++ -*- */
/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/plugin/file_downloader.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "ppapi/cpp/completion_callback.h"

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

// Callback resources are essentially our continuation state.

struct LogToJavaScriptConsoleResource {
 public:
  explicit LogToJavaScriptConsoleResource(std::string msg)
      : message(msg) {}
  std::string message;
};

struct OpenManifestEntryResource {
 public:
  OpenManifestEntryResource(std::string target_url,
                            int32_t* descp,
                            ErrorInfo* infop,
                            bool* portablep,
                            bool* op_complete)
      : url(target_url),
        out_desc(descp),
        error_info(infop),
        is_portable(portablep),
        op_complete_ptr(op_complete) {}
  std::string url;
  int32_t* out_desc;
  ErrorInfo* error_info;
  bool* is_portable;
  bool* op_complete_ptr;
};

struct CloseManifestEntryResource {
 public:
  CloseManifestEntryResource(int32_t desc_to_close,
                             bool* op_complete,
                             bool* op_result)
      : desc(desc_to_close),
        op_complete_ptr(op_complete),
        op_result_ptr(op_result) {}

  int32_t desc;
  bool* op_complete_ptr;
  bool* op_result_ptr;
};

// Do not invoke from the main thread, since the main methods will
// invoke CallOnMainThread and then wait on a condvar for the task to
// complete: if invoked from the main thread, the main method not
// returning (and thus unblocking the main thread) means that the
// main-thread continuation methods will never get called, and thus
// we'd get a deadlock.
class PluginReverseInterface: public nacl::ReverseInterface {
 public:
  PluginReverseInterface(nacl::WeakRefAnchor* anchor,
                         Plugin* plugin,
                         pp::CompletionCallback init_done_cb);

  virtual ~PluginReverseInterface();

  void ShutDown();

  virtual void Log(nacl::string message);

  virtual void StartupInitializationComplete();

  virtual bool EnumerateManifestKeys(std::set<nacl::string>* out_keys);

  virtual bool OpenManifestEntry(nacl::string url_key, int32_t* out_desc);

  virtual bool CloseManifestEntry(int32_t desc);

 protected:
  virtual void Log_MainThreadContinuation(LogToJavaScriptConsoleResource* p,
                                          int32_t err);

  virtual void OpenManifestEntry_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t err);

  virtual void StreamAsFile_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t result);

  virtual void CloseManifestEntry_MainThreadContinuation(
      CloseManifestEntryResource* cls,
      int32_t err);

 private:
  nacl::WeakRefAnchor* anchor_;  // holds a ref
  Plugin* plugin_;  // value may be copied, but should be used only in
                    // main thread in WeakRef-protected callbacks.
  NaClMutex mu_;
  NaClCondVar cv_;
  bool shutting_down_;

  pp::CompletionCallback init_done_cb_;
};

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  // TODO(sehr): This class should also implement factory methods, using the
  // Start method below.
  ServiceRuntime(Plugin* plugin,
                 pp::CompletionCallback init_done_cb);
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

  nacl::WeakRefAnchor* anchor_;

  PluginReverseInterface* rev_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
