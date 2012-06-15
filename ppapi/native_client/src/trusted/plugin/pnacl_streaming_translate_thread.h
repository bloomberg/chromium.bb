// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_STREAMING_TRANSLATE_THREAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_STREAMING_TRANSLATE_THREAD_H_

#include <deque>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/plugin/pnacl_translate_thread.h"

namespace plugin {

class PnaclStreamingTranslateThread : public PnaclTranslateThread {
 public:
  PnaclStreamingTranslateThread();
  virtual ~PnaclStreamingTranslateThread();

  // Start the translation process. It will continue to run and consume data
  // as it is passed in with PutBytes.
  virtual void RunTranslate(const pp::CompletionCallback& finish_callback,
                            const Manifest* manifest,
                            const Manifest* ld_manifest,
                            LocalTempFile* obj_file,
                            LocalTempFile* nexe_file,
                            nacl::DescWrapper* pexe_wrapper,
                            ErrorInfo* error_info,
                            PnaclResources* resources,
                            Plugin* plugin);

  // Kill the translation and/or linking processes.
  virtual void SetSubprocessesShouldDie();

  // Send bitcode bytes to the translator. Called from the main thread.
  void PutBytes(std::vector<char>* data, int count);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclStreamingTranslateThread);
  // Run the streaming translation. Call on the translation/SRPC thread.
  void DoTranslate();

  bool done_;
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
};

} // namespace plugin
#endif // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_STREAMING_TRANSLATE_THREAD_H_
