// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_coordinator.h"

#include <utility>
#include <vector>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_srpc_lib.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"

namespace plugin {

class Plugin;

namespace {

const char kLlcUrl[] = "llc";
const char kLdUrl[] = "ld";

nacl::string ResourceBaseUrl() {
  return nacl::string("pnacl_support/") + GetSandboxISA() + "/";
}

nacl::string Random32CharHexString(struct NaClDescRng* rng) {
  struct NaClDesc* desc = reinterpret_cast<struct NaClDesc*>(rng);
  const struct NaClDescVtbl* vtbl =
      reinterpret_cast<const struct NaClDescVtbl*>(desc->base.vtbl);

  nacl::string hex_string;
  const int32_t kTempFileNameWords = 4;
  for (int32_t i = 0; i < kTempFileNameWords; ++i) {
    int32_t num;
    CHECK(sizeof num == vtbl->Read(desc,
                                   reinterpret_cast<char*>(&num),
                                   sizeof num));
    char frag[16];
    SNPRINTF(frag, sizeof frag, "%08x", num);
    hex_string += nacl::string(frag);
  }
  return hex_string;
}

// Some constants for PnaclFileDescPair::GetFD readability.
const bool kReadOnly = false;
const bool kWriteable = true;

}  // namespace

//////////////////////////////////////////////////////////////////////
PnaclFileDescPair::PnaclFileDescPair(Plugin* plugin,
                                     pp::FileSystem* file_system,
                                     PnaclCoordinator* coordinator)
    : plugin_(plugin),
      file_system_(file_system),
      coordinator_(coordinator) {
  PLUGIN_PRINTF(("PnaclFileDescPair::PnaclFileDescPair (plugin=%p, "
                 "file_system=%p, coordinator=%p)\n",
                 static_cast<void*>(plugin), static_cast<void*>(file_system),
                 static_cast<void*>(coordinator)));
  callback_factory_.Initialize(this);
  CHECK(NaClDescRngCtor(&rng_desc_));
  file_io_trusted_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  // Get a random temp file name.
  filename_ = "/" + Random32CharHexString(&rng_desc_);
}

PnaclFileDescPair::~PnaclFileDescPair() {
  PLUGIN_PRINTF(("PnaclFileDescPair::~PnaclFileDescPair\n"));
  NaClDescUnref(reinterpret_cast<NaClDesc*>(&rng_desc_));
}

void PnaclFileDescPair::Open(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("PnaclFileDescPair::Open\n"));
  done_callback_ = cb;

  write_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  write_io_.reset(new pp::FileIO(plugin_));
  read_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  read_io_.reset(new pp::FileIO(plugin_));

  pp::CompletionCallback open_write_cb =
      callback_factory_.NewCallback(&PnaclFileDescPair::WriteFileDidOpen);
  // Open the writeable file.
  write_io_->Open(*write_ref_,
                  PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
                  open_write_cb);
}

int32_t PnaclFileDescPair::GetFD(int32_t pp_error,
                                 const pp::Resource& resource,
                                 bool is_writable) {
  PLUGIN_PRINTF(("PnaclFileDescPair::GetFD (pp_error=%"NACL_PRId32
                 ", is_writable=%d)\n", pp_error, is_writable));
  if (pp_error != PP_OK) {
    PLUGIN_PRINTF(("PnaclFileDescPair::GetFD pp_error != PP_OK\n"));
    return -1;
  }
  int32_t file_desc =
      file_io_trusted_->GetOSFileDescriptor(resource.pp_resource());
#if NACL_WINDOWS
  // Convert the Windows HANDLE from Pepper to a POSIX file descriptor.
  int32_t open_flags = ((is_writable ? _O_RDWR : _O_RDONLY) | _O_BINARY);
  int32_t posix_desc = _open_osfhandle(file_desc, open_flags);
  if (posix_desc == -1) {
    // Close the Windows HANDLE if it can't be converted.
    CloseHandle(reinterpret_cast<HANDLE>(file_desc));
    PLUGIN_PRINTF(("PnaclFileDescPair::GetFD _open_osfhandle failed.\n"));
    return NACL_NO_FILE_DESC;
  }
  file_desc = posix_desc;
#endif
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    PLUGIN_PRINTF(("PnaclFileDescPair::GetFD dup failed.\n"));
    return -1;
  }
  return file_desc_ok_to_close;
}

void PnaclFileDescPair::WriteFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclFileDescPair::WriteFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Remember the object temporary file descriptor.
  int32_t fd = GetFD(pp_error, *write_io_, kWriteable);
  if (fd < 0) {
    coordinator_->ReportNonPpapiError("could not open write temp file\n");
    return;
  }
  write_wrapper_.reset(plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDWR));
  pp::CompletionCallback open_read_cb =
      callback_factory_.NewCallback(&PnaclFileDescPair::ReadFileDidOpen);
  // Open the read only file.
  read_io_->Open(*read_ref_, PP_FILEOPENFLAG_READ, open_read_cb);
}

void PnaclFileDescPair::ReadFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclFileDescPair::ReadFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Remember the object temporary file descriptor.
  int32_t fd = GetFD(pp_error, *read_io_, kReadOnly);
  if (fd < 0) {
    coordinator_->ReportNonPpapiError("could not open read temp file\n");
    return;
  }
  read_wrapper_.reset(plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, done_callback_, PP_OK);
}

//////////////////////////////////////////////////////////////////////
PnaclCoordinator* PnaclCoordinator::BitcodeToNative(
    Plugin* plugin,
    const nacl::string& pexe_url,
    const pp::CompletionCallback& translate_notify_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (plugin=%p, pexe=%s)\n",
                 static_cast<void*>(plugin), pexe_url.c_str()));
  PnaclCoordinator* coordinator =
      new PnaclCoordinator(plugin,
                           pexe_url,
                           translate_notify_callback,
                           ResourceBaseUrl());
  // Load llc and ld.
  std::vector<nacl::string> resource_urls;
  resource_urls.push_back(kLlcUrl);
  resource_urls.push_back(kLdUrl);
  pp::CompletionCallback resources_cb =
      coordinator->callback_factory_.NewCallback(
          &PnaclCoordinator::ResourcesDidLoad);
  coordinator->resources_.reset(
      new PnaclResources(plugin,
                         coordinator,
                         coordinator->resource_base_url_,
                         resource_urls,
                         resources_cb));
  CHECK(coordinator->resources_ != NULL);
  coordinator->resources_->StartDownloads();
  // ResourcesDidLoad will be invoked when all resources have been received.
  return coordinator;
}

int32_t PnaclCoordinator::GetLoadedFileDesc(int32_t pp_error,
                                            const nacl::string& url,
                                            const nacl::string& component) {
  PLUGIN_PRINTF(("PnaclCoordinator::GetLoadedFileDesc (pp_error=%"
                 NACL_PRId32", url=%s, component=%s)\n", pp_error,
                 url.c_str(), component.c_str()));
  PLUGIN_PRINTF(("PnaclCoordinator::GetLoadedFileDesc (pp_error=%d\n"));
  ErrorInfo error_info;
  int32_t file_desc = plugin_->GetPOSIXFileDesc(url);
  if (pp_error != PP_OK || file_desc == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      plugin_->ReportLoadAbort();
    } else {
      ReportPpapiError(pp_error, component + " load failed.\n");
    }
    return -1;
  }
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    ReportPpapiError(PP_ERROR_FAILED, component + " could not dup fd.\n");
    return -1;
  }
  return file_desc_ok_to_close;
}

PnaclCoordinator::PnaclCoordinator(
    Plugin* plugin,
    const nacl::string& pexe_url,
    const pp::CompletionCallback& translate_notify_callback,
    const nacl::string& resource_base_url)
  : plugin_(plugin),
    translate_notify_callback_(translate_notify_callback),
    resource_base_url_(resource_base_url),
    llc_subprocess_(NULL),
    ld_subprocess_(NULL),
    subprocesses_should_die_(false),
    file_system_(new pp::FileSystem(plugin, PP_FILESYSTEMTYPE_LOCALTEMPORARY)),
    pexe_url_(pexe_url) {
  PLUGIN_PRINTF(("PnaclCoordinator::PnaclCoordinator (this=%p, plugin=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(plugin)));
  callback_factory_.Initialize(this);
  NaClXMutexCtor(&subprocess_mu_);
  // Initialize the file lookup related members.
  NaClXMutexCtor(&lookup_service_mu_);
  NaClXCondVarCtor(&lookup_service_cv_);
  // Open the temporary file system.
  CHECK(file_system_ != NULL);
}

PnaclCoordinator::~PnaclCoordinator() {
  PLUGIN_PRINTF(("PnaclCoordinator::~PnaclCoordinator (this=%p)\n",
                 static_cast<void*>(this)));
  // Join helper thread which will block the page from refreshing while a
  // translation is happening.
  if (translate_thread_.get() != NULL) {
    SetSubprocessesShouldDie(true);
    NaClThreadJoin(translate_thread_.get());
  }
  NaClCondVarDtor(&lookup_service_cv_);
  NaClMutexDtor(&lookup_service_mu_);
  NaClMutexDtor(&subprocess_mu_);
}

void PnaclCoordinator::ReportNonPpapiError(const nacl::string& message) {
  error_info_.SetReport(ERROR_UNKNOWN,
                        nacl::string("PnaclCoordinator: ") + message);
  ReportPpapiError(PP_ERROR_FAILED);
}

void PnaclCoordinator::ReportPpapiError(int32_t pp_error,
                                        const nacl::string& message) {
  error_info_.SetReport(ERROR_UNKNOWN,
                        nacl::string("PnaclCoordinator: ") + message);
  ReportPpapiError(pp_error);
}

void PnaclCoordinator::ReportPpapiError(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ReportPpappiError (pp_error=%"
                 NACL_PRId32", error_code=%d, message=%s)\n",
                 pp_error, error_info_.error_code(),
                 error_info_.message().c_str()));
  plugin_->ReportLoadError(error_info_);
  // Free all the intermediate callbacks we ever created.
  callback_factory_.CancelAll();
  translate_notify_callback_.Run(pp_error);
}

void PnaclCoordinator::TranslateFinished(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::TranslateFinished (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Transfer ownership of the nexe wrapper to the coordinator.
  // TODO(sehr): need to release the translation unit here while transferring.
  translated_fd_.reset(nexe_file_->read_wrapper());
  plugin_->EnqueueProgressEvent(Plugin::kProgressEventProgress);
  translate_notify_callback_.Run(pp_error);
}

void PnaclCoordinator::TranslateFailed(const nacl::string& error_string) {
  PLUGIN_PRINTF(("PnaclCoordinator::TranslateFailed (error_string=%"
                 NACL_PRId32")\n", error_string.c_str()));
  pp::Core* core = pp::Module::Get()->core();
  error_info_.SetReport(ERROR_UNKNOWN,
                        nacl::string("PnaclCoordinator: ") + error_string);
  core->CallOnMainThread(0, translate_done_cb_, PP_ERROR_FAILED);
  NaClThreadExit(1);
}

void PnaclCoordinator::ResourcesDidLoad(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ResourcesDidLoad (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error, "resources failed to load\n");
    return;
  }
  // Open the local temporary file system to create the temporary files
  // for the object and nexe.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::FileSystemDidOpen);
  if (!file_system_->Open(0, cb)) {
    ReportNonPpapiError("failed to open file system.\n");
  }
}

void PnaclCoordinator::FileSystemDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::FileSystemDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error, "file system didn't open.\n");
    return;
  }
  // Create the object file pair for connecting llc and ld.
  obj_file_.reset(new PnaclFileDescPair(plugin_, file_system_.get(), this));
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::ObjectPairDidOpen);
  obj_file_->Open(cb);
}

void PnaclCoordinator::ObjectPairDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ObjectPairDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Create the nexe file pair for connecting ld and sel_ldr.
  nexe_file_.reset(new PnaclFileDescPair(plugin_, file_system_.get(), this));
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::NexePairDidOpen);
  nexe_file_->Open(cb);
}

void PnaclCoordinator::NexePairDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexePairDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Load the pexe file and get the translation started.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::RunTranslate);

  if (!plugin_->StreamAsFile(pexe_url_, cb.pp_completion_callback())) {
    ReportNonPpapiError(nacl::string("failed to download ") + pexe_url_ + "\n");
  }
}

void PnaclCoordinator::RunTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunTranslate (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  int32_t fd = GetLoadedFileDesc(pp_error, pexe_url_, "pexe");
  if (fd < 0) {
    return;
  }
  pexe_wrapper_.reset(plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));
  // It would really be nice if we could create subprocesses from other than
  // the main thread.  Until we can, we create them both up front.
  // TODO(sehr): allow creation of subrpocesses from other threads.
  llc_subprocess_ = StartSubprocess(kLlcUrl);
  if (llc_subprocess_ == NULL) {
    ReportPpapiError(PP_ERROR_FAILED);
    return;
  }
  ld_subprocess_ = StartSubprocess(kLdUrl);
  if (ld_subprocess_ == NULL) {
    ReportPpapiError(PP_ERROR_FAILED);
    return;
  }
  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  translate_done_cb_ =
      callback_factory_.NewCallback(&PnaclCoordinator::TranslateFinished);
  translate_thread_.reset(new NaClThread);
  if (translate_thread_ == NULL) {
    ReportNonPpapiError("could not allocate thread struct\n");
    return;
  }
  const int32_t kArbitraryStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(translate_thread_.get(),
                                DoTranslateThread,
                                this,
                                kArbitraryStackSize)) {
    ReportNonPpapiError("could not create thread\n");
  }
}

NaClSubprocess* PnaclCoordinator::StartSubprocess(
    const nacl::string& url_for_nexe) {
  PLUGIN_PRINTF(("PnaclCoordinator::StartSubprocess (url_for_nexe=%s)\n",
                 url_for_nexe.c_str()));
  nacl::DescWrapper* wrapper = resources_->WrapperForUrl(url_for_nexe);
  NaClSubprocessId id = plugin_->LoadHelperNaClModule(wrapper, &error_info_);
  if (kInvalidNaClSubprocessId == id) {
    PLUGIN_PRINTF((
        "PnaclCoordinator::StartSubprocess: invalid subprocess id\n"));
    return NULL;
  }
  return plugin_->nacl_subprocess(id);
}

// TODO(sehr): the thread body should be in a class by itself with a delegate
// class for interfacing with the rest of the coordinator.
void WINAPI PnaclCoordinator::DoTranslateThread(void* arg) {
  PnaclCoordinator* coordinator = reinterpret_cast<PnaclCoordinator*>(arg);
  Plugin* plugin = coordinator->plugin_;
  BrowserInterface* browser_interface = plugin->browser_interface();

  // Run LLC.
  SrpcParams params;
  nacl::DescWrapper* llc_out_file = coordinator->obj_file_->write_wrapper();
  if (!PnaclSrpcLib::InvokeSrpcMethod(browser_interface,
                                      coordinator->llc_subprocess_,
                                      "RunWithDefaultCommandLine",
                                      "hh",
                                      &params,
                                      coordinator->pexe_wrapper_->desc(),
                                      llc_out_file->desc())) {
    coordinator->TranslateFailed("compile failed.");
  }
  // LLC returns values that are used to determine how linking is done.
  int is_shared_library = (params.outs()[0]->u.ival != 0);
  nacl::string soname = params.outs()[1]->arrays.str;
  nacl::string lib_dependencies = params.outs()[2]->arrays.str;
  PLUGIN_PRINTF(("PnaclCoordinator: compile (coordinator=%p) succeeded"
                 " is_shared_library=%d, soname='%s', lib_dependencies='%s')\n",
                 arg, is_shared_library, soname.c_str(),
                 lib_dependencies.c_str()));
  if (coordinator->SubprocessesShouldDie()) {
    PLUGIN_PRINTF((
        "PnaclCoordinator::DoTranslateThread: killed by coordinator.\n"));
    NaClThreadExit(1);
  }
  // Set up the lookup service for filename to handle resolution.
  NaClSrpcService* service =
      reinterpret_cast<NaClSrpcService*>(calloc(1, sizeof(*service)));
  if (NULL == service) {
    coordinator->TranslateFailed("lookup service alloc failed.");
  }
  if (!NaClSrpcServiceHandlerCtor(service, lookup_methods)) {
    free(service);
    coordinator->TranslateFailed("lookup service constructor failed.");
  }
  char* service_string = const_cast<char*>(service->service_string);
  NaClSubprocess* ld_subprocess = coordinator->ld_subprocess_;
  ld_subprocess->srpc_client()->AttachService(service, coordinator);
  nacl::DescWrapper* ld_out_file = coordinator->nexe_file_->write_wrapper();
  if (!PnaclSrpcLib::InvokeSrpcMethod(browser_interface,
                                      ld_subprocess,
                                      "RunWithDefaultCommandLine",
                                      "ChiCC",
                                      &params,
                                      service_string,
                                      ld_out_file->desc(),
                                      is_shared_library,
                                      soname.c_str(),
                                      lib_dependencies.c_str())) {
    coordinator->TranslateFailed("link failed.");
  }
  PLUGIN_PRINTF(("PnaclCoordinator: link (coordinator=%p) succeeded\n", arg));
  if (coordinator->SubprocessesShouldDie()) {
    PLUGIN_PRINTF((
        "PnaclCoordinator::DoTranslateThread: killed by coordinator.\n"));
    NaClThreadExit(1);
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, coordinator->translate_done_cb_, PP_OK);
  NaClThreadExit(0);
}

bool PnaclCoordinator::SubprocessesShouldDie() {
  nacl::MutexLocker ml(&subprocess_mu_);
  return subprocesses_should_die_;
}

void PnaclCoordinator::SetSubprocessesShouldDie(bool subprocesses_should_die) {
  nacl::MutexLocker ml(&subprocess_mu_);
  subprocesses_should_die_ = subprocesses_should_die;
}

void PnaclCoordinator::LoadOneFile(int32_t pp_error,
                                   const nacl::string& url,
                                   nacl::DescWrapper** wrapper,
                                   pp::CompletionCallback& done_cb) {
  PLUGIN_PRINTF(("PnaclCoordinator::LoadOneFile (pp_error=%"
                 NACL_PRId32", url=%s)\n", pp_error, url.c_str()));
  const nacl::string& full_url = resource_base_url_ + url;
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&PnaclCoordinator::DidLoadFile,
                                    full_url,
                                    wrapper,
                                    done_cb);
  if (!plugin_->StreamAsFile(full_url, callback.pp_completion_callback())) {
    ReportNonPpapiError(nacl::string("failed to load ") + url + "\n");
  }
}

void PnaclCoordinator::DidLoadFile(int32_t pp_error,
                                   const nacl::string& full_url,
                                   nacl::DescWrapper** wrapper,
                                   pp::CompletionCallback& done_cb) {
  PLUGIN_PRINTF(("PnaclCoordinator::DidLoadFile (pp_error=%"
                 NACL_PRId32", url=%s)\n", pp_error, full_url.c_str()));
  int32_t fd = GetLoadedFileDesc(pp_error, full_url, "resource");
  if (fd < 0) {
    PLUGIN_PRINTF((
        "PnaclCoordinator::DidLoadFile: GetLoadedFileDesc returned bad fd.\n"));
    return;
  }
  *wrapper = plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
  done_cb.Run(PP_OK);
}

void PnaclCoordinator::ResumeLookup(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ResumeLookup (pp_error=%"
                 NACL_PRId32", url=%s)\n", pp_error));
  UNREFERENCED_PARAMETER(pp_error);
  nacl::MutexLocker ml(&lookup_service_mu_);
  lookup_is_complete_ = true;
  NaClXCondVarBroadcast(&lookup_service_cv_);
}

// Lookup service called by translator nexes.
// TODO(sehr): replace this lookup by ReverseService.
void PnaclCoordinator::LookupInputFile(NaClSrpcRpc* rpc,
                                       NaClSrpcArg** inputs,
                                       NaClSrpcArg** outputs,
                                       NaClSrpcClosure* done) {
  PLUGIN_PRINTF(("PnaclCoordinator::LookupInputFile (url=%s)\n",
                 inputs[0]->arrays.str));
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  const char* file_name = inputs[0]->arrays.str;
  PnaclCoordinator* coordinator = reinterpret_cast<PnaclCoordinator*>(
      rpc->channel->server_instance_data);
  outputs[0]->u.hval = coordinator->LookupDesc(file_name);
  rpc->result = NACL_SRPC_RESULT_OK;
}

NaClSrpcHandlerDesc PnaclCoordinator::lookup_methods[] = {
  { "LookupInputFile:s:h", LookupInputFile },
  { NULL, NULL }
};

struct NaClDesc* PnaclCoordinator::LookupDesc(const nacl::string& url) {
  PLUGIN_PRINTF(("PnaclCoordinator::LookupDesc (url=%s)\n", url.c_str()));
  // This filename is part of the contract with the linker.  It is a
  // fake filename for the object file generated by llc and consumed by ld.
  // TODO(sehr): Pass the FD in, and move lookup for this file to the linker.
  const nacl::string kGeneratedObjectFileName = "___PNACL_GENERATED";
  if (url == kGeneratedObjectFileName) {
    return obj_file_->read_wrapper()->desc();
  }
  nacl::DescWrapper* wrapper;
  // Create the callback used to report when lookup is done.
  pp::CompletionCallback resume_cb =
    callback_factory_.NewCallback(&PnaclCoordinator::ResumeLookup);
  // Run the lookup request on the main thread.
  lookup_is_complete_ = false;
  pp::CompletionCallback load_cb =
    callback_factory_.NewCallback(&PnaclCoordinator::LoadOneFile,
                                  url, &wrapper, resume_cb);
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, load_cb, PP_OK);
  // Wait for completion (timeout every 10ms to check for process end).
  const int32_t kTenMilliseconds = 10 * 1000 * 1000;
  NACL_TIMESPEC_T reltime;
  reltime.tv_sec = 0;
  reltime.tv_nsec = kTenMilliseconds;
  NaClXMutexLock(&lookup_service_mu_);
  while (!lookup_is_complete_) {
    // Check for termination.
    if (SubprocessesShouldDie()) {
      NaClXMutexUnlock(&lookup_service_mu_);
      NaClThreadExit(0);
    }
    NaClXCondVarTimedWaitRelative(&lookup_service_cv_,
                                  &lookup_service_mu_,
                                  &reltime);
  }
  NaClXMutexUnlock(&lookup_service_mu_);
  return wrapper->desc();
}

}  // namespace plugin
