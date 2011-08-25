// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_coordinator.h"

#include <utility>
#include <vector>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_srpc_lib.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/pp_errors.h"

namespace {

typedef std::vector<nacl::string> string_vector;
int32_t kArbitraryStackSize = 128 << 10;

}  // namespace

namespace plugin {

class Plugin;

void PnaclCoordinator::Initialize(Plugin* instance) {
  PLUGIN_PRINTF(("PnaclCoordinator::Initialize (this=%p)\n",
                 static_cast<void*>(this)));
  CHECK(instance != NULL);
  CHECK(instance_ == NULL);   // Can only initialize once.
  instance_ = instance;
  callback_factory_.Initialize(this);
}

PnaclCoordinator::~PnaclCoordinator() {
  PLUGIN_PRINTF(("PnaclCoordinator::~PnaclCoordinator (this=%p)\n",
                 static_cast<void*>(this)));

  // Delete helper thread args. Join helper thread first (since it may be
  // using the args), which will block the page from Refreshing while a
  // translation is happening.
  if (translate_args_ != NULL) {
    translate_args_->should_die = true;
    // Assume that when X_args_ != NULL, then X_thread_ != NULL too.
    NaClThreadJoin(translate_thread_.get());
  }
  if (link_args_ != NULL) {
    link_args_->should_die = true;
    NaClThreadJoin(link_thread_.get());
  }

  // Delete all delayed_callbacks.
  delayed_callbacks.erase(delayed_callbacks.begin(), delayed_callbacks.end());

  for (std::map<nacl::string, nacl::DescWrapper*>::iterator
           i = linker_resource_fds_.begin(), e = linker_resource_fds_.end();
       i != e;
       ++i) {
    delete i->second;
  }
  linker_resource_fds_.clear();
}

void PnaclCoordinator::ReportLoadAbort() {
  instance_->ReportLoadAbort();
}

void PnaclCoordinator::ReportLoadError(const ErrorInfo& error) {
  instance_->ReportLoadError(error);
}

void PnaclCoordinator::PnaclPpapiError(int32_t pp_error) {
  // Attempt to free all the intermediate callbacks we ever created.
  callback_factory_.CancelAll();
  translate_notify_callback_.Run(pp_error);
}

void PnaclCoordinator::PnaclNonPpapiError() {
  PnaclPpapiError(PP_ERROR_FAILED);
}

void PnaclCoordinator::PnaclDidFinish(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::PnaclDidFinish (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  translate_notify_callback_.Run(pp_error);
}

//////////////////////////////////////////////////////////////////////

DelayedCallback*
PnaclCoordinator::MakeDelayedCallback(pp::CompletionCallback cb,
                                      uint32_t num_deps) {
  DelayedCallback* delayed_callback = new DelayedCallback(cb, num_deps);
  delayed_callbacks.insert(delayed_callback);
  return delayed_callback;
}

void PnaclCoordinator::SetObjectFile(NaClSrpcImcDescType fd, int32_t len) {
  obj_fd_.reset(instance_->wrapper_factory()->MakeGeneric(fd));
  obj_len_ = len;
}

void PnaclCoordinator::SetTranslatedFile(NaClSrpcImcDescType fd) {
  translated_fd_.reset(instance_->wrapper_factory()->MakeGeneric(fd));
}

int32_t PnaclCoordinator::GetLoadedFileDesc(int32_t pp_error,
                                            const nacl::string& url,
                                            const nacl::string& component) {
  ErrorInfo error_info;
  int32_t file_desc = instance_->GetPOSIXFileDesc(url);
  if (pp_error != PP_OK || file_desc == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      // TODO(jvoung): Make a generic load error, or just use ERROR_UNKNOWN?
      error_info.SetReport(ERROR_UNKNOWN,
                           "PNaCl " + component + " load failed.");
      ReportLoadError(error_info);
    }
    return -1;
  }
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    // TODO(jvoung): Make a generic load error, or just use ERROR_UNKNOWN?
    error_info.SetReport(ERROR_UNKNOWN,
                         "PNaCl " + component + " load failed: "
                         "could not dup fd.");
    ReportLoadError(error_info);
    return -1;
  }
  return file_desc_ok_to_close;
}

NaClSubprocessId PnaclCoordinator::HelperNexeDidLoad(int32_t fd,
                                                     ErrorInfo* error_info) {
  // Inform JavaScript that we successfully loaded a helper nexe.
  instance_->EnqueueProgressEvent("progress",
                                  Plugin::LENGTH_IS_NOT_COMPUTABLE,
                                  Plugin::kUnknownBytes,
                                  Plugin::kUnknownBytes);
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(instance_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));

  return instance_->LoadHelperNaClModule(wrapper.get(), error_info);
}

//////////////////////////////////////////////////////////////////////
// First few callbacks.

void PnaclCoordinator::LLCReady(int32_t pp_error,
                                const nacl::string& llc_url,
                                DelayedCallback* delayed_callback) {
  // pp_error is checked by GetLoadedFileDesc.
  int32_t file_desc_ok_to_close = GetLoadedFileDesc(pp_error, llc_url, "llc");
  ErrorInfo error_info;
  if (file_desc_ok_to_close < 0) {
    PnaclPpapiError(pp_error);
    return;
  }
  NaClSubprocessId llc_id =
      HelperNexeDidLoad(file_desc_ok_to_close, &error_info);
  PLUGIN_PRINTF(("PnaclCoordinator::LLCReady (pp_error=%"
                 NACL_PRId32" nexe_id=%"
                 NACL_PRId32")\n",
                 pp_error,
                 llc_id));
  if (kInvalidNaClSubprocessId == llc_id) {
    error_info.SetReport(ERROR_UNKNOWN,
                         "Could not load pnacl compiler nexe");
    ReportLoadError(error_info);
    PnaclNonPpapiError();
    return;
  }
  llc_subprocess_ = instance_ ->nacl_subprocess(llc_id);
  delayed_callback->RunIfTime();
}

void PnaclCoordinator::LDReady(int32_t pp_error,
                               const nacl::string& ld_url,
                               DelayedCallback* delayed_callback) {
  // pp_error is checked by GetLoadedFileDesc.
  int32_t file_desc_ok_to_close = GetLoadedFileDesc(pp_error, ld_url, "ld");
  ErrorInfo error_info;
  if (file_desc_ok_to_close < 0) {
    PnaclPpapiError(pp_error);
    return;
  }
  NaClSubprocessId ld_id =
      HelperNexeDidLoad(file_desc_ok_to_close, &error_info);
  PLUGIN_PRINTF(("PnaclCoordinator::LDReady (pp_error=%"
                 NACL_PRId32" nexe_id=%"
                 NACL_PRId32")\n",
                 pp_error,
                 ld_id));
  if (kInvalidNaClSubprocessId == ld_id) {
    error_info.SetReport(ERROR_UNKNOWN,
                         "Could not load pnacl linker nexe");
    ReportLoadError(error_info);
    PnaclNonPpapiError();
    return;
  }
  ld_subprocess_ = instance_ ->nacl_subprocess(ld_id);
  delayed_callback->RunIfTime();
}

void PnaclCoordinator::PexeReady(int32_t pp_error,
                                 const nacl::string& pexe_url,
                                 DelayedCallback* delayed_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::PexeReady (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // pp_error is checked by GetLoadedFileDesc.
  int32_t fd = GetLoadedFileDesc(pp_error, pexe_url, "pexe");
  if (fd < 0) {
    PnaclPpapiError(pp_error);
  } else {
    pexe_fd_.reset(instance_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));
    delayed_callback->RunIfTime();
  }
}

void PnaclCoordinator::LinkResourceReady(int32_t pp_error,
                                         const nacl::string& url,
                                         DelayedCallback* delayed_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::LinkResourceReady (pp_error=%"
                 NACL_PRId32", url=%s)\n", pp_error, url.c_str()));
  // pp_error is checked by GetLoadedFileDesc.
  int32_t fd = GetLoadedFileDesc(pp_error, url, "linker resource " + url);
  if (fd < 0) {
    PnaclPpapiError(pp_error);
  } else {
    linker_resource_fds_[url] =
        instance_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
    delayed_callback->RunIfTime();
  }
}

//////////////////////////////////////////////////////////////////////

namespace {
void AbortTranslateThread(DoTranslateArgs* args,
                          const nacl::string& error_string) {
  pp::Core* core = pp::Module::Get()->core();
  args->error_info.SetReport(ERROR_UNKNOWN, error_string);
  core->CallOnMainThread(0, args->finish_cb, PP_ERROR_FAILED);
  NaClThreadExit(1);
}
}  // namespace

void WINAPI DoTranslateThread(void* arg) {
  DoTranslateArgs* p = reinterpret_cast<DoTranslateArgs*>(arg);
  NaClSubprocess* llc_subprocess = p->subprocess;

  // Set up LLC flags first.
  // TODO(jvoung): Bake these into the llc nexe?
  // May also want to improve scriptability, but the only thing we need
  // probably is PIC vs non-PIC and micro-arch specification.
  const char* llc_args_x8632[] = { "-march=x86",
                                   "-mcpu=pentium4",
                                   "-mtriple=i686-none-linux-gnu",
                                   "-asm-verbose=false",
                                   "-filetype=obj" };
  const char* llc_args_x8664[] = { "-march=x86-64",
                                   "-mcpu=core2",
                                   "-mtriple=x86_64-none-linux-gnu",
                                   "-asm-verbose=false",
                                   "-filetype=obj" };
  const char* llc_args_arm[] = { "-march=arm",
                                 "-mcpu=cortex-a8",
                                 "-mtriple=armv7a-none-linux-gnueabi",
                                 "-asm-verbose=false",
                                 "-filetype=obj",
                                 "-arm-reserve-r9",
                                 "-sfi-disable-cp",
                                 "-arm_static_tls",
                                 "-sfi-store -sfi-stack -sfi-branch -sfi-data",
                                 "-no-inline-jumptables" };

  nacl::string sandbox_isa = GetSandboxISA();
  const char** llc_args;
  size_t num_args;

  if (sandbox_isa.compare("x86-32") == 0) {
    llc_args = llc_args_x8632;
    num_args = NACL_ARRAY_SIZE(llc_args_x8632);
  } else if (sandbox_isa.compare("x86-64") == 0) {
    llc_args = llc_args_x8664;
    num_args = NACL_ARRAY_SIZE(llc_args_x8664);
  } else if (sandbox_isa.compare("arm") == 0) {
    llc_args = llc_args_arm;
    num_args = NACL_ARRAY_SIZE(llc_args_arm);
  } else {
    AbortTranslateThread(p,
                         "PnaclCoordinator compiler unhandled ISA " +
                         sandbox_isa + ".");
    return;
  }

  for (uint32_t i = 0; i < num_args; i++) {
    if (p->should_die) {
      NaClThreadExit(1);
    }
    SrpcParams dummy_params;
    if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                        llc_subprocess,
                                        "AddArg",
                                        "C",
                                        &dummy_params,
                                        llc_args[i])) {
      AbortTranslateThread(p,
                           "PnaclCoordinator compiler AddArg(" +
                           nacl::string(llc_args[i]) + ") failed.");
    }
  }

  if (p->should_die) {
    NaClThreadExit(1);
  }
  SrpcParams params;
  if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                      llc_subprocess,
                                      "Translate",
                                      "h",
                                      &params,
                                      p->pexe_fd->desc())) {
    AbortTranslateThread(p,
                         "PnaclCoordinator compile failed.");
  } else {
    // Grab the outparams.
    p->obj_fd = params.outs()[0]->u.hval;
    p->obj_len = params.outs()[1]->u.ival;
    PLUGIN_PRINTF(("PnaclCoordinator::InvokeTranslate succeeded (bytes=%"
                   NACL_PRId32")\n", p->obj_len));
  }
  if (p->should_die) {
    NaClThreadExit(1);
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, p->finish_cb, PP_OK);
  NaClThreadExit(0);
}

void
PnaclCoordinator::RunTranslateDidFinish(int32_t pp_error,
                                        DelayedCallback* delayed_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunTranslateDidFinish (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportLoadError(translate_args_->error_info);
    PnaclPpapiError(pp_error);
    return;
  }
  SetObjectFile(translate_args_->obj_fd, translate_args_->obj_len);
  instance_->EnqueueProgressEvent("progress",
                                  Plugin::LENGTH_IS_NOT_COMPUTABLE,
                                  Plugin::kUnknownBytes,
                                  Plugin::kUnknownBytes);
  delayed_callback->RunIfTime();
}

void PnaclCoordinator::RunTranslate(int32_t pp_error,
                                    DelayedCallback* delayed_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunTranslate (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  assert(PP_OK == pp_error);

  // Invoke llvm asynchronously.
  pp::CompletionCallback finish_cb =
      callback_factory_.NewCallback(&PnaclCoordinator::RunTranslateDidFinish,
                                    delayed_callback);
  translate_args_.reset(new DoTranslateArgs(llc_subprocess_,
                                            instance_->browser_interface(),
                                            finish_cb,
                                            pexe_fd_.get()));
  translate_thread_.reset(new NaClThread);
  if (translate_thread_ != NULL && translate_args_ != NULL) {
    if (!NaClThreadCreateJoinable(translate_thread_.get(),
                                  DoTranslateThread,
                                  translate_args_.get(),
                                  kArbitraryStackSize)) {
      ErrorInfo error_info;
      error_info.SetReport(ERROR_UNKNOWN,
                           "Could not create a translator thread.\n");
      ReportLoadError(error_info);
      PnaclNonPpapiError();
    }
  } else {
    ErrorInfo error_info;
    error_info.SetReport(ERROR_UNKNOWN,
                         "Could not allocate DoTranslateThread()\n");
    ReportLoadError(error_info);
    PnaclNonPpapiError();
  }
}

//////////////////////////////////////////////////////////////////////
// Helper functions for loading native libs.
// Done here to avoid hacking on the manifest parser further...

namespace {

// Fake filename for the object file generated by llvm.
const nacl::string kGeneratedObjectFileName =
    nacl::string("___PNACL_GENERATED");

string_vector LinkResources(const nacl::string& sandbox_isa,
                            bool withGenerated) {
  string_vector results;
  nacl::string base_dir = "pnacl_support/" + sandbox_isa;

  // NOTE: order of items == link order.
  if (withGenerated) {
    results.push_back(kGeneratedObjectFileName);
  }
  results.push_back(base_dir + "/libcrt_platform.a");
  results.push_back(base_dir + "/libgcc.a");
  results.push_back(base_dir + "/libgcc_eh.a");
  return results;
}

}  // namespace

//////////////////////////////////////////////////////////////////////
// Final link callbacks.

namespace {

void AbortLinkThread(DoLinkArgs* args, const nacl::string& error_string) {
  ErrorInfo error_info;
  pp::Core* core = pp::Module::Get()->core();
  args->error_info.SetReport(ERROR_UNKNOWN, error_string);
  core->CallOnMainThread(0, args->finish_cb, PP_ERROR_FAILED);
  NaClThreadExit(1);
}

}  // namespace

void WINAPI DoLinkThread(void* arg) {
  DoLinkArgs* p = reinterpret_cast<DoLinkArgs*>(arg);
  NaClSubprocess* ld_subprocess = p->subprocess;

  // Set up command line arguments (flags then files).

  //// Flags.
  // TODO(jvoung): Be able to handle the dynamic linking flags too,
  // and don't hardcode so much here.
  string_vector flags;
  nacl::string sandbox_isa = GetSandboxISA();
  flags.push_back("-nostdlib");
  flags.push_back("-m");
  if (sandbox_isa.compare("x86-32") == 0) {
    flags.push_back("elf_nacl");
  } else if (sandbox_isa.compare("x86-64") == 0) {
    flags.push_back("elf64_nacl");
  } else if (sandbox_isa.compare("arm") == 0) {
    flags.push_back("armelf_nacl");
  } else {
    AbortLinkThread(p,
                    "PnaclCoordinator linker unhandled ISA " +
                    sandbox_isa + ".");
  }

  for (string_vector::iterator i = flags.begin(), e = flags.end();
       i != e; ++i) {
    const nacl::string& flag = *i;
    if (p->should_die) {
      NaClThreadExit(1);
    }
    SrpcParams dummy_params;
    if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                        ld_subprocess,
                                        "AddArg",
                                        "C",
                                        &dummy_params,
                                        flag.c_str())) {
      AbortLinkThread(p,
                      "PnaclCoordinator linker AddArg(" + flag +
                      ") failed.");
    }
  }

  //// Files.
  PnaclCoordinator* pnacl = p->coordinator;
  string_vector files = LinkResources(sandbox_isa, true);
  for (string_vector::iterator i = files.begin(), e = files.end();
       i != e; ++i) {
    const nacl::string& link_file = *i;
    if (p->should_die) {
      NaClThreadExit(1);
    }
    // Add as argument.
    SrpcParams dummy_params;
    if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                        ld_subprocess,
                                        "AddArg",
                                        "C",
                                        &dummy_params,
                                        link_file.c_str())) {
      AbortLinkThread(p,
                      "PnaclCoordinator linker AddArg(" +
                      link_file + ") failed.");
    }
    // Also map the file name to descriptor.
    if (i->compare(kGeneratedObjectFileName) == 0) {
      SrpcParams dummy_params2;
      if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                          ld_subprocess,
                                          "AddFileWithSize",
                                          "Chi",
                                          &dummy_params2,
                                          link_file.c_str(),
                                          p->obj_fd->desc(),
                                          p->obj_len)) {
        AbortLinkThread(p,
                        "PnaclCoordinator linker AddFileWithSize"
                        "(" + link_file + ") failed.");
      }
    } else {
      SrpcParams dummy_params2;
      if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                          ld_subprocess,
                                          "AddFile",
                                          "Ch",
                                          &dummy_params2,
                                          link_file.c_str(),
                                          pnacl->GetLinkerResourceFD(
                                            link_file))) {
        AbortLinkThread(p,
                        "PnaclCoordinator linker AddFile(" + link_file +
                        ") failed.");
      }
    }
  }

  if (p->should_die) {
    NaClThreadExit(1);
  }

  // Finally, do the Link!
  SrpcParams params;
  if (!PnaclSrpcLib::InvokeSrpcMethod(p->browser,
                                      ld_subprocess,
                                      "Link",
                                      "",
                                      &params)) {
    AbortLinkThread(p, "PnaclCoordinator link failed.");
  } else {
    // Grab the outparams.
    p->nexe_fd = params.outs()[0]->u.hval;
    int32_t nexe_size = params.outs()[1]->u.ival;  // only for debug.
    PLUGIN_PRINTF(("PnaclCoordinator::InvokeLink succeeded (bytes=%"
                   NACL_PRId32")\n", nexe_size));
  }
  if (p->should_die) {
    NaClThreadExit(1);
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, p->finish_cb, PP_OK);
  NaClThreadExit(0);
}

void PnaclCoordinator::RunLinkDidFinish(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunLinkDidFinish (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportLoadError(link_args_->error_info);
    PnaclPpapiError(pp_error);
    return;
  }
  SetTranslatedFile(link_args_->nexe_fd);
  instance_->EnqueueProgressEvent("progress",
                                  Plugin::LENGTH_IS_NOT_COMPUTABLE,
                                  Plugin::kUnknownBytes,
                                  Plugin::kUnknownBytes);
  PnaclDidFinish(PP_OK);
}

void PnaclCoordinator::RunLink(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunLink (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  assert(PP_OK == pp_error);

  // Invoke llvm asynchronously.
  pp::CompletionCallback finish_cb =
      callback_factory_.NewCallback(&PnaclCoordinator::RunLinkDidFinish);
  link_args_.reset(new DoLinkArgs(ld_subprocess_,
                                  instance_->browser_interface(),
                                  finish_cb,
                                  this,
                                  obj_fd_.get(),
                                  obj_len_));
  link_thread_.reset(new NaClThread);
  if (link_args_ != NULL && link_thread_ != NULL) {
    if (!NaClThreadCreateJoinable(link_thread_.get(),
                                  DoLinkThread,
                                  link_args_.get(),
                                  kArbitraryStackSize)) {
      ErrorInfo error_info;
      error_info.SetReport(ERROR_UNKNOWN,
                           "Could not create a linker thread.\n");
      ReportLoadError(error_info);
      PnaclNonPpapiError();
    }
  } else {
    ErrorInfo error_info;
    error_info.SetReport(ERROR_UNKNOWN,
                         "Could not allocate DoLinkThread()\n");
    ReportLoadError(error_info);
    PnaclNonPpapiError();
  }
}

//////////////////////////////////////////////////////////////////////

bool PnaclCoordinator::ScheduleDownload(const nacl::string& url,
                                        const pp::CompletionCallback& cb) {
  if (!instance_->StreamAsFile(url,
                               cb.pp_completion_callback())) {
    ErrorInfo error_info;
    error_info.SetReport(ERROR_UNKNOWN,
                         "PnaclCoordinator: Failed to download file: " +
                         url + "\n");
    ReportLoadError(error_info);
    PnaclNonPpapiError();
    return false;
  }
  return true;
}

void PnaclCoordinator::AddDownloadToDelayedCallback(
    void (PnaclCoordinator::*handler)(int32_t,
                                      const nacl::string&,
                                      DelayedCallback*),
    DelayedCallback* delayed_callback,
    const nacl::string& url,
    std::vector<url_callback_pair>& queue) {
  // Queue up the URL download w/ a callback that invokes the delayed_callback.
  queue.push_back(std::make_pair(
      url,
      callback_factory_.NewCallback(handler,
                                    url,
                                    delayed_callback)));
  delayed_callback->IncrRequirements(1);
}

void PnaclCoordinator::BitcodeToNative(
    const nacl::string& pexe_url,
    const nacl::string& llc_url,
    const nacl::string& ld_url,
    const pp::CompletionCallback& finish_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (pexe=%s, llc=%s, ld=%s)\n",
                 pexe_url.c_str(),
                 llc_url.c_str(),
                 ld_url.c_str()));
  translate_notify_callback_ = finish_callback;

  string_vector link_resources = LinkResources(GetSandboxISA(), false);

  // Steps:
  // (1) Schedule downloads for llc, ld nexes, and native libraries.
  // (2) When llc download and pexe has completed, run the translation.
  // (3) When llc translation has finished, and ld, native libs are available,
  // do the link.
  // (4) When the link is done, we are done, call the finish_callback.
  // Hand off the SHM file descriptor returned by link.

  // Set up async callbacks for these steps in reverse order.

  // (3) Run link.

  pp::CompletionCallback run_link_callback =
      callback_factory_.NewCallback(&PnaclCoordinator::RunLink);
  DelayedCallback* delayed_link_callback =
      MakeDelayedCallback(run_link_callback, 0);

  // (2) Run translation.
  pp::CompletionCallback run_translate_callback =
      callback_factory_.NewCallback(&PnaclCoordinator::RunTranslate,
                                    delayed_link_callback);
  // Linking depends on the compile finishing, so incr requirements by one.
  delayed_link_callback->IncrRequirements(1);

  DelayedCallback* delayed_translate_callback = MakeDelayedCallback(
      run_translate_callback, 0);

  // (1) Load nexes and assets using StreamAsFile(). This will kick off
  // the whole process.

  // First, just collect the list of stuff to download.
  std::vector<url_callback_pair> downloads;

  AddDownloadToDelayedCallback(&PnaclCoordinator::PexeReady,
                               delayed_translate_callback,
                               pexe_url,
                               downloads);
  AddDownloadToDelayedCallback(&PnaclCoordinator::LLCReady,
                               delayed_translate_callback,
                               llc_url,
                               downloads);
  AddDownloadToDelayedCallback(&PnaclCoordinator::LDReady,
                               delayed_link_callback,
                               ld_url,
                               downloads);
  for (string_vector::iterator
           i = link_resources.begin(), e = link_resources.end();
       i != e;
       ++i) {
    AddDownloadToDelayedCallback(&PnaclCoordinator::LinkResourceReady,
                                 delayed_link_callback,
                                 *i,
                                 downloads);
  }

  // Finally, actually schedule the downloads.
  for (size_t i = 0; i < downloads.size(); ++i) {
    if (!ScheduleDownload(downloads[i].first, downloads[i].second)) {
      break;  // error should have been reported by ScheduleDownload.
    }
  }
  downloads.clear();

  return;
}

}  // namespace plugin
