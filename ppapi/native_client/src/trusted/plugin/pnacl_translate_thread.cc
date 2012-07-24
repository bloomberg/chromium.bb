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
                                               ld_manifest_(NULL),
                                               obj_file_(NULL),
                                               nexe_file_(NULL),
                                               coordinator_error_info_(NULL),
                                               resources_(NULL),
                                               plugin_(NULL) {
  NaClXMutexCtor(&subprocess_mu_);
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

bool PnaclTranslateThread::RunLdSubprocess(int is_shared_library,
                                           const nacl::string& soname,
                                           const nacl::string& lib_dependencies
                                           ) {
  ErrorInfo error_info;
  nacl::scoped_ptr<NaClSubprocess> ld_subprocess(
      StartSubprocess(PnaclUrls::GetLdUrl(), ld_manifest_, &error_info));
  if (ld_subprocess == NULL) {
    TranslateFailed("Link process could not be created: " +
                    error_info.message());
    return false;
  }
  // Run LD.
  SrpcParams params;
  nacl::DescWrapper* ld_in_file = obj_file_->read_wrapper();
  nacl::DescWrapper* ld_out_file = nexe_file_->write_wrapper();
  PluginReverseInterface* ld_reverse =
      ld_subprocess->service_runtime()->rev_interface();
  ld_reverse->AddQuotaManagedFile(nexe_file_->identifier(),
                                  nexe_file_->write_file_io());
  if (!ld_subprocess->InvokeSrpcMethod("RunWithDefaultCommandLine",
                                       "hhiss",
                                       &params,
                                       ld_in_file->desc(),
                                       ld_out_file->desc(),
                                       is_shared_library,
                                       soname.c_str(),
                                       lib_dependencies.c_str())) {
    TranslateFailed("link failed.");
    return false;
  }
  PLUGIN_PRINTF(("PnaclCoordinator: link (translator=%p) succeeded\n",
                 this));
  // Shut down the ld subprocess.
  ld_subprocess.reset(NULL);
  if (SubprocessesShouldDie()) {
    TranslateFailed("stopped by coordinator.");
    return false;
  }
  return true;
}

void PnaclTranslateThread::TranslateFailed(const nacl::string& error_string) {
  PLUGIN_PRINTF(("PnaclTranslateThread::TranslateFailed (error_string='%s')\n",
                 error_string.c_str()));
  pp::Core* core = pp::Module::Get()->core();
  if (coordinator_error_info_->message().empty()) {
    // Only use our message if one hasn't already been set by the coordinator
    // (e.g. pexe load failed).
    coordinator_error_info_->SetReport(ERROR_UNKNOWN,
                                       nacl::string("PnaclCoordinator: ") +
                                       error_string);
  }
  core->CallOnMainThread(0, report_translate_finished_, PP_ERROR_FAILED);
}

bool PnaclTranslateThread::SubprocessesShouldDie() {
  nacl::MutexLocker ml(&subprocess_mu_);
  return subprocesses_should_die_;
}

void PnaclTranslateThread::SetSubprocessesShouldDie() {
  PLUGIN_PRINTF(("PnaclTranslateThread::SetSubprocessesShouldDie\n"));
  nacl::MutexLocker ml(&subprocess_mu_);
  subprocesses_should_die_ = true;
}

PnaclTranslateThread::~PnaclTranslateThread() {
  PLUGIN_PRINTF(("~PnaclTranslateThread (translate_thread=%p)\n",
                 translate_thread_.get()));
  if (translate_thread_ != NULL) {
    SetSubprocessesShouldDie();
    NaClThreadJoin(translate_thread_.get());
    PLUGIN_PRINTF(("~PnaclTranslateThread joined\n"));
  }
  NaClMutexDtor(&subprocess_mu_);
}

} // namespace plugin
