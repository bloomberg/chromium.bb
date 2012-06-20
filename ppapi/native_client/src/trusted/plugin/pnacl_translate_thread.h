// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/plugin/plugin_error.h"

#include "ppapi/cpp/completion_callback.h"

namespace nacl {
class DescWrapper;
}


namespace plugin {

class LocalTempFile;
class Manifest;
class NaClSubprocess;
class Plugin;
class PnaclResources;

class PnaclTranslateThread {
 public:
  PnaclTranslateThread();
  virtual ~PnaclTranslateThread();
  // TODO(jvoung/dschuff): handle surfaway issues when coordinator/plugin
  // goes away. This data may have to be refcounted not touched in that case.
  virtual void RunTranslate(const pp::CompletionCallback& finish_callback,
                    const Manifest* manifest,
                    const Manifest* ld_manifest,
                    LocalTempFile* obj_file,
                    LocalTempFile* nexe_file,
                    nacl::DescWrapper* pexe_wrapper,
                    ErrorInfo* error_info,
                    PnaclResources* resources,
                    Plugin* plugin);
  // Returns true if the translate thread and subprocesses should stop.
  bool SubprocessesShouldDie();
  // Signal the translate thread and subprocesses that they should stop.
  virtual void SetSubprocessesShouldDie();

 protected:
  // Starts an individual llc or ld subprocess used for translation.
  NaClSubprocess* StartSubprocess(const nacl::string& url,
                                  const Manifest* manifest,
                                  ErrorInfo* error_info);
  // Helper thread entry point for translation. Takes a pointer to
  // PnaclTranslateThread and calls DoTranslate().
  static void WINAPI DoTranslateThread(void* arg);
  // Runs the SRPCs that control translation. Called from the helper thread.
  virtual void DoTranslate();
  // Signal that Pnacl translation failed, from the translation thread only.
  void TranslateFailed(const nacl::string& error_string);
  // Run the LD subprocess, returning true on success
  bool RunLdSubprocess(int is_shared_library,
                       const nacl::string& soname,
                       const nacl::string& lib_dependencies);

  // Callback to run when tasks are completed or an error has occurred.
  pp::CompletionCallback report_translate_finished_;
  // True if the translation thread and related subprocesses should exit.
  bool subprocesses_should_die_;
  // Used to guard and publish subprocesses_should_die_.
  struct NaClMutex subprocess_mu_;

  nacl::scoped_ptr<NaClThread> translate_thread_;

  // Data about the translation files, owned by the coordinator
  const Manifest* manifest_;
  const Manifest* ld_manifest_;
  LocalTempFile* obj_file_;
  LocalTempFile* nexe_file_;
  nacl::DescWrapper* pexe_wrapper_;
  ErrorInfo* coordinator_error_info_;
  PnaclResources* resources_;
  Plugin* plugin_;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclTranslateThread);
};

}
#endif // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_
