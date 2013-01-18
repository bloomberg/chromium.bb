// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_translate_thread.h"

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"
#include "native_client/src/trusted/plugin/srpc_params.h"
#include "native_client/src/trusted/plugin/temporary_file.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

PnaclTranslateThread::PnaclTranslateThread() : llc_subprocess_active_(false),
                                               ld_subprocess_active_(false),
                                               done_(false),
                                               time_stats_(),
                                               manifest_(NULL),
                                               ld_manifest_(NULL),
                                               obj_file_(NULL),
                                               nexe_file_(NULL),
                                               coordinator_error_info_(NULL),
                                               resources_(NULL),
                                               plugin_(NULL) {
  NaClXMutexCtor(&subprocess_mu_);
  NaClXMutexCtor(&cond_mu_);
  NaClXCondVarCtor(&buffer_cond_);
}

void PnaclTranslateThread::RunTranslate(
    const pp::CompletionCallback& finish_callback,
    const Manifest* manifest,
    const Manifest* ld_manifest,
    TempFile* obj_file,
    TempFile* nexe_file,
    ErrorInfo* error_info,
    PnaclResources* resources,
    Plugin* plugin) {
  PLUGIN_PRINTF(("PnaclStreamingTranslateThread::RunTranslate)\n"));
  manifest_ = manifest;
  ld_manifest_ = ld_manifest;
  obj_file_ = obj_file;
  nexe_file_ = nexe_file;
  coordinator_error_info_ = error_info;
  resources_ = resources;
  plugin_ = plugin;

  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  report_translate_finished_ = finish_callback;
  translate_thread_.reset(new NaClThread);
  if (translate_thread_ == NULL) {
    TranslateFailed(ERROR_PNACL_THREAD_CREATE,
                    "could not allocate thread struct.");
    return;
  }
  const int32_t kArbitraryStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(translate_thread_.get(),
                                DoTranslateThread,
                                this,
                                kArbitraryStackSize)) {
    TranslateFailed(ERROR_PNACL_THREAD_CREATE,
                    "could not create thread.");
    translate_thread_.reset(NULL);
  }
}

// Called from main thread to send bytes to the translator.
void PnaclTranslateThread::PutBytes(std::vector<char>* bytes,
                                             int count) {
  PLUGIN_PRINTF(("PutBytes, this %p bytes %p, size %d, count %d\n", this, bytes,
                 bytes ? bytes->size(): 0, count));
  size_t buffer_size = 0;
  // If we are done (error or not), Signal the translation thread to stop.
  if (count <= PP_OK) {
    NaClXMutexLock(&cond_mu_);
    done_ = true;
    NaClXCondVarSignal(&buffer_cond_);
    NaClXMutexUnlock(&cond_mu_);
    return;
  }

  CHECK(bytes != NULL);
  // Ensure that the buffer we send to the translation thread is the right size
  // (count can be < the buffer size). This can be done without the lock.
  buffer_size = bytes->size();
  bytes->resize(count);

  NaClXMutexLock(&cond_mu_);

  data_buffers_.push_back(std::vector<char>());
  bytes->swap(data_buffers_.back()); // Avoid copying the buffer data.

  NaClXCondVarSignal(&buffer_cond_);
  NaClXMutexUnlock(&cond_mu_);

  // Ensure the buffer we send back to the coordinator is the expected size
  bytes->resize(buffer_size);
}

NaClSubprocess* PnaclTranslateThread::StartSubprocess(
    const nacl::string& url_for_nexe,
    const Manifest* manifest,
    ErrorInfo* error_info) {
  PLUGIN_PRINTF(("PnaclTranslateThread::StartSubprocess (url_for_nexe=%s)\n",
                 url_for_nexe.c_str()));
  nacl::DescWrapper* wrapper = resources_->WrapperForUrl(url_for_nexe);
  nacl::scoped_ptr<NaClSubprocess> subprocess(
      plugin_->LoadHelperNaClModule(wrapper, manifest, error_info));
  if (subprocess.get() == NULL) {
    PLUGIN_PRINTF((
        "PnaclTranslateThread::StartSubprocess: subprocess creation failed\n"));
    return NULL;
  }
  return subprocess.release();
}

void WINAPI PnaclTranslateThread::DoTranslateThread(void* arg) {
  PnaclTranslateThread* translator =
      reinterpret_cast<PnaclTranslateThread*>(arg);
  translator->DoTranslate();
}

void PnaclTranslateThread::DoTranslate() {
  ErrorInfo error_info;
  SrpcParams params;
  nacl::DescWrapper* llc_out_file = obj_file_->write_wrapper();

  {
    nacl::MutexLocker ml(&subprocess_mu_);
    int64_t llc_start_time = NaClGetTimeOfDayMicroseconds();
    llc_subprocess_.reset(
      StartSubprocess(PnaclUrls::GetLlcUrl(), manifest_, &error_info));
    if (llc_subprocess_ == NULL) {
      TranslateFailed(ERROR_PNACL_LLC_SETUP,
                      "Compile process could not be created: " +
                      error_info.message());
      return;
    }
    llc_subprocess_active_ = true;
    time_stats_.pnacl_llc_load_time =
        (NaClGetTimeOfDayMicroseconds() - llc_start_time);
    // Run LLC.
    PluginReverseInterface* llc_reverse =
        llc_subprocess_->service_runtime()->rev_interface();
    llc_reverse->AddTempQuotaManagedFile(obj_file_->identifier());
  }

  int64_t compile_start_time = NaClGetTimeOfDayMicroseconds();
  if (!llc_subprocess_->InvokeSrpcMethod("StreamInit",
                                         "h",
                                         &params,
                                         llc_out_file->desc())) {
    if (llc_subprocess_->srpc_client()->GetLastError() ==
        NACL_SRPC_RESULT_APP_ERROR) {
      // The error message is only present if the error was returned from llc
      TranslateFailed(ERROR_PNACL_LLC_INTERNAL,
                      nacl::string("Stream init failed: ") +
                      nacl::string(params.outs()[0]->arrays.str));
    } else {
      TranslateFailed(ERROR_PNACL_LLC_INTERNAL,
                      "Stream init internal error");
    }
    return;
  }

  PLUGIN_PRINTF(("PnaclCoordinator: StreamInit successful\n"));

  // llc process is started.
  while(!done_ || data_buffers_.size() > 0) {
    NaClXMutexLock(&cond_mu_);
    while(!done_ && data_buffers_.size() == 0) {
      NaClXCondVarWait(&buffer_cond_, &cond_mu_);
    }
    PLUGIN_PRINTF(("PnaclTranslateThread awake, done %d, size %d\n",
                   done_, data_buffers_.size()));
    if (data_buffers_.size() > 0) {
      std::vector<char> data;
      data.swap(data_buffers_.front());
      data_buffers_.pop_front();
      NaClXMutexUnlock(&cond_mu_);
      PLUGIN_PRINTF(("StreamChunk\n"));
      if (!llc_subprocess_->InvokeSrpcMethod("StreamChunk",
                                             "C",
                                             &params,
                                             &data[0],
                                             data.size())) {
        TranslateFailed(ERROR_PNACL_LLC_INTERNAL,
                        "Compile stream chunk failed.");
        return;
      }
      PLUGIN_PRINTF(("StreamChunk Successful\n"));
    } else {
      NaClXMutexUnlock(&cond_mu_);
    }
  }
  PLUGIN_PRINTF(("PnaclTranslateThread done with chunks\n"));
  // Finish llc.
  if(!llc_subprocess_->InvokeSrpcMethod("StreamEnd",
                                       "",
                                       &params)) {
    PLUGIN_PRINTF(("PnaclTranslateThread StreamEnd failed\n"));
    if (llc_subprocess_->srpc_client()->GetLastError() ==
        NACL_SRPC_RESULT_APP_ERROR) {
      // The error string is only present if the error was sent back from llc.
      TranslateFailed(ERROR_PNACL_LLC_INTERNAL,
                      params.outs()[3]->arrays.str);
    } else {
      TranslateFailed(ERROR_PNACL_LLC_INTERNAL,
                      "Compile StreamEnd internal error");
    }
    return;
  }
  time_stats_.pnacl_compile_time =
      (NaClGetTimeOfDayMicroseconds() - compile_start_time);

  // LLC returns values that are used to determine how linking is done.
  int is_shared_library = (params.outs()[0]->u.ival != 0);
  nacl::string soname = params.outs()[1]->arrays.str;
  nacl::string lib_dependencies = params.outs()[2]->arrays.str;
  PLUGIN_PRINTF(("PnaclCoordinator: compile (translator=%p) succeeded"
                 " is_shared_library=%d, soname='%s', lib_dependencies='%s')\n",
                 this, is_shared_library, soname.c_str(),
                 lib_dependencies.c_str()));

  // Shut down the llc subprocess.
  NaClXMutexLock(&subprocess_mu_);
  llc_subprocess_active_ = false;
  llc_subprocess_.reset(NULL);
  NaClXMutexUnlock(&subprocess_mu_);

  if(!RunLdSubprocess(is_shared_library, soname, lib_dependencies)) {
    return;
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, report_translate_finished_, PP_OK);
}

bool PnaclTranslateThread::RunLdSubprocess(int is_shared_library,
                                           const nacl::string& soname,
                                           const nacl::string& lib_dependencies
                                           ) {
  ErrorInfo error_info;
  SrpcParams params;
  // Reset object file for reading first.
  if (!obj_file_->Reset()) {
    TranslateFailed(ERROR_PNACL_LD_SETUP,
                    "Link process could not reset object file");
    return false;
  }
  nacl::DescWrapper* ld_in_file = obj_file_->read_wrapper();
  nacl::DescWrapper* ld_out_file = nexe_file_->write_wrapper();

  {
    // Create LD process
    nacl::MutexLocker ml(&subprocess_mu_);
    int64_t ld_start_time = NaClGetTimeOfDayMicroseconds();
    ld_subprocess_.reset(
      StartSubprocess(PnaclUrls::GetLdUrl(), ld_manifest_, &error_info));
    if (ld_subprocess_ == NULL) {
      TranslateFailed(ERROR_PNACL_LD_SETUP,
                      "Link process could not be created: " +
                      error_info.message());
      return false;
    }
    ld_subprocess_active_ = true;
    time_stats_.pnacl_ld_load_time =
        (NaClGetTimeOfDayMicroseconds() - ld_start_time);
    PluginReverseInterface* ld_reverse =
        ld_subprocess_->service_runtime()->rev_interface();
    ld_reverse->AddTempQuotaManagedFile(nexe_file_->identifier());
  }

  int64_t link_start_time = NaClGetTimeOfDayMicroseconds();
  // Run LD.
  if (!ld_subprocess_->InvokeSrpcMethod("RunWithDefaultCommandLine",
                                       "hhiss",
                                       &params,
                                       ld_in_file->desc(),
                                       ld_out_file->desc(),
                                       is_shared_library,
                                       soname.c_str(),
                                       lib_dependencies.c_str())) {
    TranslateFailed(ERROR_PNACL_LD_INTERNAL,
                    "link failed.");
    return false;
  }
  time_stats_.pnacl_link_time =
      NaClGetTimeOfDayMicroseconds() - link_start_time;
  PLUGIN_PRINTF(("PnaclCoordinator: link (translator=%p) succeeded\n",
                 this));
  // Shut down the ld subprocess.
  NaClXMutexLock(&subprocess_mu_);
  ld_subprocess_active_ = false;
  ld_subprocess_.reset(NULL);
  NaClXMutexUnlock(&subprocess_mu_);
  return true;
}

void PnaclTranslateThread::TranslateFailed(
    enum PluginErrorCode err_code,
    const nacl::string& error_string) {
  PLUGIN_PRINTF(("PnaclTranslateThread::TranslateFailed (error_string='%s')\n",
                 error_string.c_str()));
  pp::Core* core = pp::Module::Get()->core();
  if (coordinator_error_info_->message().empty()) {
    // Only use our message if one hasn't already been set by the coordinator
    // (e.g. pexe load failed).
    coordinator_error_info_->SetReport(err_code,
                                       nacl::string("PnaclCoordinator: ") +
                                       error_string);
  }
  core->CallOnMainThread(0, report_translate_finished_, PP_ERROR_FAILED);
}

void PnaclTranslateThread::AbortSubprocesses() {
  PLUGIN_PRINTF(("PnaclTranslateThread::AbortSubprocesses\n"));
  NaClXMutexLock(&subprocess_mu_);
  if (llc_subprocess_ != NULL && llc_subprocess_active_) {
    llc_subprocess_->service_runtime()->Shutdown();
    llc_subprocess_active_ = false;
  }
  if (ld_subprocess_ != NULL && ld_subprocess_active_) {
    ld_subprocess_->service_runtime()->Shutdown();
    ld_subprocess_active_ = false;
  }
  NaClXMutexUnlock(&subprocess_mu_);
  nacl::MutexLocker ml(&cond_mu_);
  done_ = true;
  // Free all buffered bitcode chunks
  data_buffers_.clear();
  NaClXCondVarSignal(&buffer_cond_);
}

PnaclTranslateThread::~PnaclTranslateThread() {
  PLUGIN_PRINTF(("~PnaclTranslateThread (translate_thread=%p)\n", this));
  AbortSubprocesses();
  NaClThreadJoin(translate_thread_.get());
  PLUGIN_PRINTF(("~PnaclTranslateThread joined\n"));
  NaClCondVarDtor(&buffer_cond_);
  NaClMutexDtor(&cond_mu_);
  NaClMutexDtor(&subprocess_mu_);
}

} // namespace plugin
