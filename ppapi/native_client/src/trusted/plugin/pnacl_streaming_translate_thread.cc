// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_streaming_translate_thread.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"
#include "native_client/src/trusted/plugin/srpc_params.h"

namespace plugin {

PnaclStreamingTranslateThread::PnaclStreamingTranslateThread() : done_(false) {
  NaClXMutexCtor(&cond_mu_);
  NaClXCondVarCtor(&buffer_cond_);
}

PnaclStreamingTranslateThread::~PnaclStreamingTranslateThread() {}

void PnaclStreamingTranslateThread::RunTranslate(
    const pp::CompletionCallback& finish_callback,
    const Manifest* manifest,
    const Manifest* ld_manifest,
    LocalTempFile* obj_file,
    LocalTempFile* nexe_file,
    nacl::DescWrapper* pexe_wrapper,
    ErrorInfo* error_info,
    PnaclResources* resources,
    Plugin* plugin) {
  PLUGIN_PRINTF(("PnaclStreamingTranslateThread::RunTranslate)\n"));
  manifest_ = manifest;
  ld_manifest_ = ld_manifest;
  obj_file_ = obj_file;
  nexe_file_ = nexe_file;
  pexe_wrapper_ = NULL; // The streaming translator doesn't use a pexe desc.
  DCHECK(pexe_wrapper == NULL);
  error_info_ = error_info;
  resources_ = resources;
  plugin_ = plugin;

  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  report_translate_finished_ = finish_callback;
  translate_thread_.reset(new NaClThread);
  if (translate_thread_ == NULL) {
    TranslateFailed("could not allocate thread struct.");
    return;
  }
  const int32_t kArbitraryStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(translate_thread_.get(),
                                DoStreamingTranslateThread,
                                this,
                                kArbitraryStackSize)) {
    TranslateFailed("could not create thread.");
    translate_thread_.reset(NULL);
  }
}

// Called from main thread to send bytes to the translator.
void PnaclStreamingTranslateThread::PutBytes(std::vector<char>* bytes,
                                             int count) {
  PLUGIN_PRINTF(("PutBytes, this %p bytes %p, size %d, count %d\n", this, bytes,
                 bytes ? bytes->size(): 0, count));
  int buffer_size = 0;
  // Ensure that the buffer we send to the translation thread is the right size
  // (count can be < the buffer size). This can be done without the lock.
  if (bytes != NULL && count >= 0) {
    buffer_size = bytes->size();
    bytes->resize(count);
  }
  NaClXMutexLock(&cond_mu_);
  if (count == PP_OK || count < 0) {
    done_ = true;
  } else {
    data_buffers_.push_back(std::vector<char>());
    bytes->swap(data_buffers_.back()); // Avoid copying the buffer data.
  }
  NaClXCondVarSignal(&buffer_cond_);
  NaClXMutexUnlock(&cond_mu_);
  // Ensure the buffer we send back to the coordinator is the expected size
  if (bytes != NULL) bytes->resize(buffer_size);
}

void WINAPI PnaclStreamingTranslateThread::DoStreamingTranslateThread(
    void* arg) {
  PnaclStreamingTranslateThread* translator =
      reinterpret_cast<PnaclStreamingTranslateThread*>(arg);
  nacl::scoped_ptr<NaClSubprocess> llc_subprocess(
      translator->StartSubprocess(PnaclUrls::GetLlcUrl(),
                                  translator->manifest_));
  if (llc_subprocess == NULL) {
    translator->TranslateFailed("Compile process could not be created.");
    return;
  }
  // Run LLC.
  SrpcParams params;
  nacl::DescWrapper* llc_out_file = translator->obj_file_->write_wrapper();
  PluginReverseInterface* llc_reverse =
      llc_subprocess->service_runtime()->rev_interface();
  llc_reverse->AddQuotaManagedFile(translator->obj_file_->identifier(),
                                   translator->obj_file_->write_file_io());

  if (!llc_subprocess->InvokeSrpcMethod("StreamInit",
                                        "h",
                                        &params,
                                        llc_out_file->desc())) {
    // StreamInit returns an error message if the RPC fails.
    translator->TranslateFailed(nacl::string("Stream init failed: ") +
                                nacl::string(params.outs()[0]->arrays.str));
    return;
  }

  PLUGIN_PRINTF(("PnaclCoordinator: StreamInit successful\n"));

  // llc process is started.
  while(!translator->done_ || translator->data_buffers_.size() > 0) {
    NaClXMutexLock(&translator->cond_mu_);
    while(!translator->done_ && translator->data_buffers_.size() == 0) {
      NaClXCondVarWait(&translator->buffer_cond_, &translator->cond_mu_);
    }
    PLUGIN_PRINTF(("PnaclTranslateThread awake, done %d, size %d\n",
                   translator->done_, translator->data_buffers_.size()));
    if (translator->data_buffers_.size() > 0) {
      std::vector<char> data;
      data.swap(translator->data_buffers_.front());
      translator->data_buffers_.pop_front();
      NaClXMutexUnlock(&translator->cond_mu_);
      PLUGIN_PRINTF(("StreamChunk\n"));
      if (!llc_subprocess->InvokeSrpcMethod("StreamChunk",
                                            "C",
                                            &params,
                                            &data[0],
                                            data.size())) {
        translator->TranslateFailed("Compile stream chunk failed.");
        return;
      }
      PLUGIN_PRINTF(("StreamChunk Successful\n"));
    } else {
      NaClXMutexUnlock(&translator->cond_mu_);
    }
  }
  PLUGIN_PRINTF(("PnaclTranslateThread done with chunks\n"));
  // Finish llc.
  if(!llc_subprocess->InvokeSrpcMethod("StreamEnd",
                                       "",
                                       &params)) {
    PLUGIN_PRINTF(("PnaclTranslateThread StreamEnd failed\n"));
    translator->TranslateFailed(params.outs()[3]->arrays.str);
    return;
  }
  // LLC returns values that are used to determine how linking is done.
  int is_shared_library = (params.outs()[0]->u.ival != 0);
  nacl::string soname = params.outs()[1]->arrays.str;
  nacl::string lib_dependencies = params.outs()[2]->arrays.str;
  PLUGIN_PRINTF(("PnaclCoordinator: compile (translator=%p) succeeded"
                 " is_shared_library=%d, soname='%s', lib_dependencies='%s')\n",
                 arg, is_shared_library, soname.c_str(),
                 lib_dependencies.c_str()));

  // Shut down the llc subprocess.
  llc_subprocess.reset(NULL);
  if (translator->SubprocessesShouldDie()) {
    translator->TranslateFailed("stopped by coordinator.");
    return;
  }

  if(!RunLdSubprocess(translator, is_shared_library, soname,
                      lib_dependencies)) {
    return;
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, translator->report_translate_finished_, PP_OK);
}

void PnaclStreamingTranslateThread::SetSubprocessesShouldDie() {
  PnaclTranslateThread::SetSubprocessesShouldDie();
  nacl::MutexLocker ml(&cond_mu_);
  done_ = true;
  NaClXCondVarSignal(&buffer_cond_);
}

} // namespace plugin
