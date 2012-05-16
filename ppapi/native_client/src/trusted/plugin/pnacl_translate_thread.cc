// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_translate_thread.h"

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"
#include "native_client/src/trusted/plugin/srpc_params.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

PnaclTranslateThread::PnaclTranslateThread() : subprocesses_should_die_(false),
                                               manifest_(NULL),
                                               obj_file_(NULL),
                                               nexe_file_(NULL),
                                               pexe_wrapper_(NULL),
                                               error_info_(NULL),
                                               resources_(NULL),
                                               plugin_(NULL) {
  NaClXMutexCtor(&subprocess_mu_);
}

void PnaclTranslateThread::RunTranslate(pp::CompletionCallback finish_callback,
                                        const Manifest* manifest,
                                        const Manifest* ld_manifest,
                                        LocalTempFile* obj_file,
                                        LocalTempFile* nexe_file,
                                        nacl::DescWrapper* pexe_wrapper,
                                        ErrorInfo* error_info,
                                        PnaclResources* resources,
                                        Plugin* plugin) {
  PLUGIN_PRINTF(("PnaclTranslateThread::RunTranslate)\n"));
  manifest_ = manifest;
  ld_manifest_ = ld_manifest;
  obj_file_ = obj_file;
  nexe_file_ = nexe_file;
  pexe_wrapper_ = pexe_wrapper;
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
                                DoTranslateThread,
                                this,
                                kArbitraryStackSize)) {
    TranslateFailed("could not create thread.");
  }
}

NaClSubprocess* PnaclTranslateThread::StartSubprocess(
    const nacl::string& url_for_nexe,
    const Manifest* manifest) {
  PLUGIN_PRINTF(("PnaclTranslateThread::StartSubprocess (url_for_nexe=%s)\n",
                 url_for_nexe.c_str()));
  nacl::DescWrapper* wrapper = resources_->WrapperForUrl(url_for_nexe);
  nacl::scoped_ptr<NaClSubprocess> subprocess(
      plugin_->LoadHelperNaClModule(wrapper, manifest, error_info_));
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
  if (!llc_subprocess->InvokeSrpcMethod("RunWithDefaultCommandLine",
                                        "hh",
                                        &params,
                                        translator->pexe_wrapper_->desc(),
                                        llc_out_file->desc())) {
    translator->TranslateFailed("compile failed.");
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
  nacl::scoped_ptr<NaClSubprocess> ld_subprocess(
      translator->StartSubprocess(PnaclUrls::GetLdUrl(),
                                  translator->ld_manifest_));
  if (ld_subprocess == NULL) {
    translator->TranslateFailed("Link process could not be created.");
    return;
  }
  // Run LD.
  nacl::DescWrapper* ld_in_file = translator->obj_file_->read_wrapper();
  nacl::DescWrapper* ld_out_file = translator->nexe_file_->write_wrapper();
  PluginReverseInterface* ld_reverse =
      ld_subprocess->service_runtime()->rev_interface();
  ld_reverse->AddQuotaManagedFile(translator->nexe_file_->identifier(),
                                  translator->nexe_file_->write_file_io());
  if (!ld_subprocess->InvokeSrpcMethod("RunWithDefaultCommandLine",
                                       "hhiCC",
                                       &params,
                                       ld_in_file->desc(),
                                       ld_out_file->desc(),
                                       is_shared_library,
                                       soname.c_str(),
                                       lib_dependencies.c_str())) {
    translator->TranslateFailed("link failed.");
    return;
  }
  PLUGIN_PRINTF(("PnaclCoordinator: link (translator=%p) succeeded\n", arg));
  // Shut down the ld subprocess.
  ld_subprocess.reset(NULL);
  if (translator->SubprocessesShouldDie()) {
    translator->TranslateFailed("stopped by coordinator.");
    return;
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, translator->report_translate_finished_, PP_OK);
}

void PnaclTranslateThread::TranslateFailed(const nacl::string& error_string) {
  PLUGIN_PRINTF(("PnaclTranslateThread::TranslateFailed (error_string='%s')\n",
                 error_string.c_str()));
  pp::Core* core = pp::Module::Get()->core();
  error_info_->SetReport(ERROR_UNKNOWN,
                        nacl::string("PnaclCoordinator: ") + error_string);
  core->CallOnMainThread(0, report_translate_finished_, PP_ERROR_FAILED);
}

bool PnaclTranslateThread::SubprocessesShouldDie() {
    nacl::MutexLocker ml(&subprocess_mu_);
  return subprocesses_should_die_;
}

void PnaclTranslateThread::SetSubprocessesShouldDie(
    bool subprocesses_should_die) {
  nacl::MutexLocker ml(&subprocess_mu_);
  subprocesses_should_die_ = subprocesses_should_die;
}

PnaclTranslateThread::~PnaclTranslateThread() {
  NaClMutexDtor(&subprocess_mu_);
}

} // namespace plugin
