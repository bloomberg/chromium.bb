// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/plugin.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/module.h"

#include "ppapi/native_client/src/trusted/plugin/nacl_subprocess.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

// Up to 20 seconds
const int64_t kTimeSmallMin = 1;         // in ms
const int64_t kTimeSmallMax = 20000;     // in ms
const uint32_t kTimeSmallBuckets = 100;

}  // namespace

void Plugin::ShutDownSubprocesses() {
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (this=%p)\n",
                 static_cast<void*>(this)));

  // Shut down service runtime. This must be done before all other calls so
  // they don't block forever when waiting for the upcall thread to exit.
  main_subprocess_.Shutdown();

  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (this=%p, return)\n",
                 static_cast<void*>(this)));
}

void Plugin::HistogramTimeSmall(const std::string& name,
                                int64_t ms) {
  if (ms < 0) return;
  uma_interface_.HistogramCustomTimes(name,
                                      ms,
                                      kTimeSmallMin, kTimeSmallMax,
                                      kTimeSmallBuckets);
}

bool Plugin::LoadHelperNaClModuleInternal(NaClSubprocess* subprocess,
                                          const SelLdrStartParams& params) {
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  ServiceRuntime* service_runtime =
      new ServiceRuntime(this,
                         pp_instance(),
                         false,  // No main_service_runtime.
                         false,  // No non-SFI mode (i.e. in SFI-mode).
                         pp::BlockUntilComplete());
  subprocess->set_service_runtime(service_runtime);

  // Now start the SelLdr instance.  This must be created on the main thread.
  bool service_runtime_started = false;
  pp::CompletionCallback sel_ldr_callback =
      callback_factory_.NewCallback(&Plugin::SignalStartSelLdrDone,
                                    &service_runtime_started,
                                    service_runtime);
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&Plugin::StartSelLdrOnMainThread,
                                    service_runtime, params,
                                    sel_ldr_callback);
  pp::Module::Get()->core()->CallOnMainThread(0, callback, 0);
  if (!service_runtime->WaitForSelLdrStart()) {
    PLUGIN_PRINTF(("Plugin::LoadHelperNaClModule "
                   "WaitForSelLdrStart timed out!\n"));
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadHelperNaClModule (service_runtime_started=%d)\n",
                 service_runtime_started));
  if (!service_runtime_started)
    return false;

  // Now actually start the nexe.
  //
  // We can't use pp::BlockUntilComplete() inside an in-process plugin, so we
  // have to roll our own blocking logic, similar to WaitForSelLdrStart()
  // above, except without timeout logic.
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      callback_factory_.NewCallback(&Plugin::StartNexe, service_runtime));
  return service_runtime->WaitForNexeStart();
}

void Plugin::StartSelLdrOnMainThread(int32_t pp_error,
                                     ServiceRuntime* service_runtime,
                                     const SelLdrStartParams& params,
                                     pp::CompletionCallback callback) {
  CHECK(pp_error == PP_OK);
  service_runtime->StartSelLdr(params, callback);
}

void Plugin::SignalStartSelLdrDone(int32_t pp_error,
                                   bool* started,
                                   ServiceRuntime* service_runtime) {
  *started = (pp_error == PP_OK);
  service_runtime->SignalStartSelLdrDone();
}

void Plugin::LoadNaClModule(PP_NaClFileInfo file_info,
                            bool uses_nonsfi_mode,
                            bool enable_dyncode_syscalls,
                            bool enable_exception_handling,
                            bool enable_crash_throttling,
                            const pp::CompletionCallback& init_done_cb) {
  CHECK(pp::Module::Get()->core()->IsMainThread());
  // Before forking a new sel_ldr process, ensure that we do not leak
  // the ServiceRuntime object for an existing subprocess, and that any
  // associated listener threads do not go unjoined because if they
  // outlive the Plugin object, they will not be memory safe.
  ShutDownSubprocesses();
  pp::Var manifest_base_url =
      pp::Var(pp::PASS_REF, nacl_interface_->GetManifestBaseURL(pp_instance()));
  std::string manifest_base_url_str = manifest_base_url.AsString();

  SelLdrStartParams params(manifest_base_url_str,
                           file_info,
                           true /* uses_irt */,
                           true /* uses_ppapi */,
                           enable_dyncode_syscalls,
                           enable_exception_handling,
                           enable_crash_throttling);
  ErrorInfo error_info;
  ServiceRuntime* service_runtime = new ServiceRuntime(
      this, pp_instance(), true, uses_nonsfi_mode, init_done_cb);
  main_subprocess_.set_service_runtime(service_runtime);
  if (NULL == service_runtime) {
    error_info.SetReport(
        PP_NACL_ERROR_SEL_LDR_INIT,
        "sel_ldr init failure " + main_subprocess_.description());
    ReportLoadError(error_info);
    return;
  }

  // We don't take any action once nexe loading has completed, so pass an empty
  // callback here for |callback|.
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &Plugin::StartNexe, service_runtime);
  StartSelLdrOnMainThread(
      static_cast<int32_t>(PP_OK), service_runtime, params, callback);
}

void Plugin::StartNexe(int32_t pp_error, ServiceRuntime* service_runtime) {
  CHECK(pp::Module::Get()->core()->IsMainThread());
  if (pp_error != PP_OK)
    return;
  service_runtime->StartNexe();
}

bool Plugin::LoadNaClModuleContinuationIntern() {
  ErrorInfo error_info;
  if (!uses_nonsfi_mode_) {
    if (!main_subprocess_.StartSrpcServices()) {
      // The NaCl process probably crashed. On Linux, a crash causes this
      // error, while on other platforms, the error is detected below, when we
      // attempt to start the proxy. Report a module initialization error here,
      // to make it less confusing for developers.
      NaClLog(LOG_ERROR, "LoadNaClModuleContinuationIntern: "
              "StartSrpcServices failed\n");
      error_info.SetReport(PP_NACL_ERROR_START_PROXY_MODULE,
                           "could not initialize module.");
      ReportLoadError(error_info);
      return false;
    }
  }

  return PP_ToBool(nacl_interface_->StartPpapiProxy(pp_instance()));
}

NaClSubprocess* Plugin::LoadHelperNaClModule(const std::string& helper_url,
                                             PP_NaClFileInfo file_info,
                                             ErrorInfo* error_info) {
  nacl::scoped_ptr<NaClSubprocess> nacl_subprocess(
      new NaClSubprocess("helper module", NULL, NULL));
  if (NULL == nacl_subprocess.get()) {
    error_info->SetReport(PP_NACL_ERROR_SEL_LDR_INIT,
                          "unable to allocate helper subprocess.");
    return NULL;
  }

  // Do not report UMA stats for translator-related nexes.
  // TODO(sehr): define new UMA stats for translator related nexe events.
  // NOTE: The PNaCl translator nexes are not built to use the IRT.  This is
  // done to save on address space and swap space.
  SelLdrStartParams params(helper_url,
                           file_info,
                           false /* uses_irt */,
                           false /* uses_ppapi */,
                           false /* enable_dyncode_syscalls */,
                           false /* enable_exception_handling */,
                           true /* enable_crash_throttling */);

  // Helper NaCl modules always use the PNaCl manifest, as there is no
  // corresponding NMF.
  if (!LoadHelperNaClModuleInternal(nacl_subprocess.get(), params))
    return NULL;

  // We need not wait for the init_done callback.  We can block
  // here in StartSrpcServices, since helper NaCl modules
  // are spawned from a private thread.
  //
  // TODO(bsy): if helper module crashes, we should abort.
  // crash_cb is not used here, so we are relying on crashes
  // being detected in StartSrpcServices or later.
  //
  // NB: More refactoring might be needed, however, if helper
  // NaCl modules have their own manifest.  Currently the
  // manifest is a per-plugin-instance object, not a per
  // NaClSubprocess object.
  if (!nacl_subprocess->StartSrpcServices()) {
    error_info->SetReport(PP_NACL_ERROR_SRPC_CONNECTION_FAIL,
                          "SRPC connection failure for " +
                          nacl_subprocess->description());
    return NULL;
  }

  PLUGIN_PRINTF(("Plugin::LoadHelperNaClModule (%s, %s)\n",
                 helper_url.c_str(),
                 nacl_subprocess.get()->detailed_description().c_str()));

  return nacl_subprocess.release();
}

// All failures of this function will show up as "Missing Plugin-in", so
// there is no need to log to JS console that there was an initialization
// failure. Note that module loading functions will log their own errors.
bool Plugin::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  nacl_interface_->InitializePlugin(pp_instance(), argc, argn, argv);
  wrapper_factory_ = new nacl::DescWrapperFactory();
  pp::CompletionCallback open_cb =
      callback_factory_.NewCallback(&Plugin::NaClManifestFileDidOpen);
  nacl_interface_->RequestNaClManifest(pp_instance(),
                                       open_cb.pp_completion_callback());
  return true;
}

Plugin::Plugin(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      main_subprocess_("main subprocess", NULL, NULL),
      uses_nonsfi_mode_(false),
      wrapper_factory_(NULL),
      nacl_interface_(NULL),
      uma_interface_(this) {
  callback_factory_.Initialize(this);
  nacl_interface_ = GetNaClInterface();
  CHECK(nacl_interface_ != NULL);

  // Notify PPB_NaCl_Private that the instance is created before altering any
  // state that it tracks.
  nacl_interface_->InstanceCreated(pp_instance);
  nexe_file_info_ = kInvalidNaClFileInfo;
}

Plugin::~Plugin() {
  int64_t shutdown_start = NaClGetTimeOfDayMicroseconds();

  // Destroy the coordinator while the rest of the data is still there
  pnacl_coordinator_.reset(NULL);

  nacl_interface_->InstanceDestroyed(pp_instance());

  // ShutDownSubprocesses shuts down the main subprocess, which shuts
  // down the main ServiceRuntime object, which kills the subprocess.
  // As a side effect of the subprocess being killed, the reverse
  // services thread(s) will get EOF on the reverse channel(s), and
  // the thread(s) will exit.  In ServiceRuntime::Shutdown, we invoke
  // ReverseService::WaitForServiceThreadsToExit(), so that there will
  // not be an extent thread(s) hanging around.  This means that the
  // ~Plugin will block until this happens.  This is a requirement,
  // since the renderer should be free to unload the plugin code, and
  // we cannot have threads running code that gets unloaded before
  // they exit.
  //
  // By waiting for the threads here, we also ensure that the Plugin
  // object and the subprocess and ServiceRuntime objects is not
  // (fully) destroyed while the threads are running, so resources
  // that are destroyed after ShutDownSubprocesses (below) are
  // guaranteed to be live and valid for access from the service
  // threads.
  //
  // The main_subprocess object, which wraps the main service_runtime
  // object, is dtor'd implicitly after the explicit code below runs,
  // so the main service runtime object will not have been dtor'd,
  // though the Shutdown method may have been called, during the
  // lifetime of the service threads.
  ShutDownSubprocesses();

  delete wrapper_factory_;

  HistogramTimeSmall(
      "NaCl.Perf.ShutdownTime.Total",
      (NaClGetTimeOfDayMicroseconds() - shutdown_start)
          / NACL_MICROS_PER_MILLI);
}

bool Plugin::HandleDocumentLoad(const pp::URLLoader& url_loader) {
  // We don't know if the plugin will handle the document load, but return
  // true in order to give it a chance to respond once the proxy is started.
  return true;
}

void Plugin::NexeFileDidOpen(int32_t pp_error) {
  if (pp_error != PP_OK)
    return;
  LoadNaClModule(
      nexe_file_info_,
      uses_nonsfi_mode_,
      true, /* enable_dyncode_syscalls */
      true, /* enable_exception_handling */
      false, /* enable_crash_throttling */
      callback_factory_.NewCallback(&Plugin::NexeFileDidOpenContinuation));
}

void Plugin::NexeFileDidOpenContinuation(int32_t pp_error) {
  UNREFERENCED_PARAMETER(pp_error);
  NaClLog(4, "Entered NexeFileDidOpenContinuation\n");
  if (LoadNaClModuleContinuationIntern()) {
    NaClLog(4, "NexeFileDidOpenContinuation: success;"
            " setting histograms\n");
    int64_t nexe_size = nacl_interface_->GetNexeSize(pp_instance());
    nacl_interface_->ReportLoadSuccess(
        pp_instance(), nexe_size, nexe_size);
  } else {
    NaClLog(4, "NexeFileDidOpenContinuation: failed.");
  }
  NaClLog(4, "Leaving NexeFileDidOpenContinuation\n");
}

void Plugin::BitcodeDidTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::BitcodeDidTranslate (pp_error=%" NACL_PRId32 ")\n",
                 pp_error));
  if (pp_error != PP_OK) {
    // Error should have been reported by pnacl. Just return.
    return;
  }

  // Inform JavaScript that we successfully translated the bitcode to a nexe.
  PP_FileHandle handle = pnacl_coordinator_->TakeTranslatedFileHandle();

  PP_NaClFileInfo info;
  info.handle = handle;
  info.token_lo = 0;
  info.token_hi = 0;
  LoadNaClModule(
      info,
      false, /* uses_nonsfi_mode */
      false, /* enable_dyncode_syscalls */
      false, /* enable_exception_handling */
      true, /* enable_crash_throttling */
      callback_factory_.NewCallback(&Plugin::BitcodeDidTranslateContinuation));
}

void Plugin::BitcodeDidTranslateContinuation(int32_t pp_error) {
  NaClLog(4, "Entered BitcodeDidTranslateContinuation\n");
  UNREFERENCED_PARAMETER(pp_error);
  if (LoadNaClModuleContinuationIntern()) {
    int64_t loaded;
    int64_t total;
    // TODO(teravest): Tighten this up so we can get rid of
    // GetCurrentProgress(). loaded should always equal total.
    pnacl_coordinator_->GetCurrentProgress(&loaded, &total);
    nacl_interface_->ReportLoadSuccess(pp_instance(), loaded, total);
  }
}

void Plugin::NaClManifestFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  if (pp_error != PP_OK)
    return;

  PP_Var pp_program_url;
  PP_PNaClOptions pnacl_options = {PP_FALSE, PP_FALSE, 2};
  PP_Bool uses_nonsfi_mode;
  if (nacl_interface_->GetManifestProgramURL(
          pp_instance(), &pp_program_url, &pnacl_options, &uses_nonsfi_mode)) {
    std::string program_url = pp::Var(pp::PASS_REF, pp_program_url).AsString();
    // TODO(teravest): Make ProcessNaClManifest take responsibility for more of
    // this function.
    nacl_interface_->ProcessNaClManifest(pp_instance(), program_url.c_str());
    uses_nonsfi_mode_ = PP_ToBool(uses_nonsfi_mode);
    if (pnacl_options.translate) {
      pp::CompletionCallback translate_callback =
          callback_factory_.NewCallback(&Plugin::BitcodeDidTranslate);
      pnacl_coordinator_.reset(
          PnaclCoordinator::BitcodeToNative(this,
                                            program_url,
                                            pnacl_options,
                                            translate_callback));
      return;
    } else {
      pp::CompletionCallback open_callback =
          callback_factory_.NewCallback(&Plugin::NexeFileDidOpen);
      // Will always call the callback on success or failure.
      nacl_interface_->DownloadNexe(pp_instance(),
                                    program_url.c_str(),
                                    &nexe_file_info_,
                                    open_callback.pp_completion_callback());
      return;
    }
  }
}

void Plugin::ReportLoadError(const ErrorInfo& error_info) {
  nacl_interface_->ReportLoadError(pp_instance(),
                                   error_info.error_code(),
                                   error_info.message().c_str());
}

}  // namespace plugin
