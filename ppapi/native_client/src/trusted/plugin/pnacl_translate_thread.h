// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_

#include <deque>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/service_runtime.h"

#include "ppapi/cpp/completion_callback.h"

namespace nacl {
class DescWrapper;
}


namespace plugin {

class Manifest;
class NaClSubprocess;
class Plugin;
class PnaclResources;
class TempFile;

class PnaclTranslateThread {
 public:
  PnaclTranslateThread();
  ~PnaclTranslateThread();

  // Start the translation process. It will continue to run and consume data
  // as it is passed in with PutBytes.
  void RunTranslate(const pp::CompletionCallback& finish_callback,
                    const Manifest* manifest,
                    const Manifest* ld_manifest,
                    TempFile* obj_file,
                    TempFile* nexe_file,
                    ErrorInfo* error_info,
                    PnaclResources* resources,
                    Plugin* plugin);

  // Signal the translate thread and subprocesses that they should stop.
  void SetSubprocessesShouldDie();

  // Send bitcode bytes to the translator. Called from the main thread.
  void PutBytes(std::vector<char>* data, int count);

 private:
  // Returns true if the translate thread and subprocesses should stop.
  bool SubprocessesShouldDie();

  // Starts an individual llc or ld subprocess used for translation.
  NaClSubprocess* StartSubprocess(const nacl::string& url,
                                  const Manifest* manifest,
                                  ErrorInfo* error_info);
  // Helper thread entry point for translation. Takes a pointer to
  // PnaclTranslateThread and calls DoTranslate().
  static void WINAPI DoTranslateThread(void* arg);
  // Runs the streaming translation. Called from the helper thread.
  void DoTranslate() ;
  // Signal that Pnacl translation failed, from the translation thread only.
  void TranslateFailed(const nacl::string& error_string);
  // Run the LD subprocess, returning true on success
  bool RunLdSubprocess(int is_shared_library,
                       const nacl::string& soname,
                       const nacl::string& lib_dependencies);

  // Register a reverse service interface which will be shutdown if the
  // plugin is shutdown. The reverse service pointer must be available on the
  // main thread because the translation thread could be blocked on SRPC
  // waiting for the translator, which could be waiting on a reverse
  // service call.
  // (see also the comments in Plugin::~Plugin about ShutdownSubprocesses,
  // but that only handles the main nexe and not the translator nexes.)
  void RegisterReverseInterface(PluginReverseInterface *iface);

  // Callback to run when tasks are completed or an error has occurred.
  pp::CompletionCallback report_translate_finished_;
  // True if the translation thread and related subprocesses should exit.
  bool subprocesses_should_die_;
  // Used to guard and publish subprocesses_should_die_.
  struct NaClMutex subprocess_mu_;

  nacl::scoped_ptr<NaClThread> translate_thread_;

  // Reverse interface to shutdown on SetSubprocessesShouldDie
  PluginReverseInterface* current_rev_interface_;



  // Condition variable to synchronize communication with the SRPC thread.
  // SRPC thread waits on this condvar if data_buffers_ is empty (meaning
  // there is no bitcode to send to the translator), and the main thread
  // appends to data_buffers_ and signals it when it receives bitcode.
  struct NaClCondVar buffer_cond_;
  // Mutex for buffer_cond_.
  struct NaClMutex cond_mu_;
  // Data buffers from FileDownloader are enqueued here to pass from the
  // main thread to the SRPC thread. Protected by cond_mu_
  std::deque<std::vector<char> > data_buffers_;
  // Whether all data has been downloaded and copied to translation thread.
  // Associated with buffer_cond_
  bool done_;

  // Data about the translation files, owned by the coordinator
  const Manifest* manifest_;
  const Manifest* ld_manifest_;
  TempFile* obj_file_;
  TempFile* nexe_file_;
  ErrorInfo* coordinator_error_info_;
  PnaclResources* resources_;
  Plugin* plugin_;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclTranslateThread);
};

}
#endif // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_
