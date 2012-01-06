// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "native_client/src/trusted/plugin/manifest.h"
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

nacl::string ExtensionUrl() {
  // TODO(sehr,jvoung): Find a better way to express the URL for the pnacl
  // extension than a constant string here.
  const nacl::string kPnaclExtensionOrigin =
      "chrome-extension://gcodniebolpnpaiggndmcmmfpldlknih/";
  return kPnaclExtensionOrigin + GetSandboxISA() + "/";
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
//  Temporary file descriptors.
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
  rng_desc_ = (struct NaClDescRng *) malloc(sizeof *rng_desc_);
  CHECK(rng_desc_ != NULL);
  CHECK(NaClDescRngCtor(rng_desc_));
  file_io_trusted_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  // Get a random temp file name.
  filename_ = "/" + Random32CharHexString(rng_desc_);
}

PnaclFileDescPair::~PnaclFileDescPair() {
  PLUGIN_PRINTF(("PnaclFileDescPair::~PnaclFileDescPair\n"));
  NaClDescUnref(reinterpret_cast<NaClDesc*>(rng_desc_));
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
    coordinator_->ReportNonPpapiError("could not open write temp file.");
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
    coordinator_->ReportNonPpapiError("could not open read temp file.");
    return;
  }
  read_wrapper_.reset(plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, done_callback_, PP_OK);
}

//////////////////////////////////////////////////////////////////////
//  Pnacl-specific manifest support.
//////////////////////////////////////////////////////////////////////
class PnaclManifest : public Manifest {
 public:
  explicit PnaclManifest(const pp::URLUtil_Dev* url_util)
      : Manifest(url_util, ExtensionUrl(), GetSandboxISA(), false) {
  }
  virtual ~PnaclManifest() { }

  virtual bool GetProgramURL(nacl::string* full_url,
                             ErrorInfo* error_info,
                             bool* is_portable) {
    // Does not contain program urls.
    UNREFERENCED_PARAMETER(full_url);
    UNREFERENCED_PARAMETER(error_info);
    UNREFERENCED_PARAMETER(is_portable);
    PLUGIN_PRINTF(("PnaclManifest does not contain a program\n"));
    error_info->SetReport(ERROR_MANIFEST_GET_NEXE_URL,
                          "pnacl manifest does not contain a program.");
    return false;
  }

  virtual bool ResolveURL(const nacl::string& relative_url,
                          nacl::string* full_url,
                          ErrorInfo* error_info) const {
    // Does not do general URL resolution, simply appends relative_url to
    // the end of manifest_base_url_.
    UNREFERENCED_PARAMETER(error_info);
    *full_url = manifest_base_url_ + relative_url;
    return true;
  }

  virtual bool GetFileKeys(std::set<nacl::string>* keys) const {
    // Does not support enumeration.
    PLUGIN_PRINTF(("PnaclManifest does not support key enumeration\n"));
    UNREFERENCED_PARAMETER(keys);
    return false;
  }

  virtual bool ResolveKey(const nacl::string& key,
                          nacl::string* full_url,
                          ErrorInfo* error_info,
                          bool* is_portable) const {
    *is_portable = false;
    // We can only resolve keys in the files/ namespace.
    const nacl::string kFilesPrefix = "files/";
    size_t files_prefix_pos = key.find(kFilesPrefix);
    if (files_prefix_pos == nacl::string::npos) {
      error_info->SetReport(ERROR_MANIFEST_RESOLVE_URL,
                            "key did not start with files/");
      return false;
    }
    // Append what follows files to the pnacl URL prefix.
    nacl::string key_basename = key.substr(kFilesPrefix.length());
    return ResolveURL(key_basename, full_url, error_info);
  }

  // Since the pnacl coordinator manifest provides access to resources
  // in the chrome extension, lookups will need to access resources in their
  // extension origin rather than the plugin's origin.
  virtual bool PermitsExtensionUrls() const {
    return true;
  }
};

//////////////////////////////////////////////////////////////////////
//  The coordinator class.
//////////////////////////////////////////////////////////////////////
PnaclCoordinator* PnaclCoordinator::BitcodeToNative(
    Plugin* plugin,
    const nacl::string& pexe_url,
    const pp::CompletionCallback& translate_notify_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (plugin=%p, pexe=%s)\n",
                 static_cast<void*>(plugin), pexe_url.c_str()));
  PnaclCoordinator* coordinator =
      new PnaclCoordinator(plugin, pexe_url, translate_notify_callback);
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (manifest=%p)\n",
                 reinterpret_cast<const void*>(coordinator->manifest_)));
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
                         coordinator->manifest_,
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
      ReportPpapiError(pp_error, component + " load failed.");
    }
    return -1;
  }
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    ReportPpapiError(PP_ERROR_FAILED, component + " could not dup fd.");
    return -1;
  }
  return file_desc_ok_to_close;
}

PnaclCoordinator::PnaclCoordinator(
    Plugin* plugin,
    const nacl::string& pexe_url,
    const pp::CompletionCallback& translate_notify_callback)
  : plugin_(plugin),
    translate_notify_callback_(translate_notify_callback),
    llc_subprocess_(NULL),
    ld_subprocess_(NULL),
    subprocesses_should_die_(false),
    file_system_(new pp::FileSystem(plugin, PP_FILESYSTEMTYPE_LOCALTEMPORARY)),
    manifest_(new PnaclManifest(plugin->url_util())),
    pexe_url_(pexe_url),
    error_already_reported_(false) {
  PLUGIN_PRINTF(("PnaclCoordinator::PnaclCoordinator (this=%p, plugin=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(plugin)));
  callback_factory_.Initialize(this);
  NaClXMutexCtor(&subprocess_mu_);
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
  // Note: this doesn't *cancel* the callbacks from the factories attached
  // to the various helper classes (e.g., pnacl_resources). Thus, those
  // callbacks may still run asynchronously.  We let those run but ignore
  // any other errors they may generate so that they do not end up running
  // translate_notify_callback_, which has already been freed.
  callback_factory_.CancelAll();
  if (!error_already_reported_) {
    error_already_reported_ = true;
    translate_notify_callback_.Run(pp_error);
  } else {
    PLUGIN_PRINTF(("PnaclCoordinator::ReportPpapiError an earlier error was "
                   "already reported -- Skipping.\n"));
  }
}

void PnaclCoordinator::TranslateFinished(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::TranslateFinished (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Transfer ownership of the nexe wrapper to the coordinator.
  // TODO(sehr): figure out when/how to delete/reap these temporary files.
  translated_fd_.reset(nexe_file_->release_read_wrapper());
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
    ReportPpapiError(pp_error, "resources failed to load.");
    return;
  }
  // Open the local temporary file system to create the temporary files
  // for the object and nexe.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::FileSystemDidOpen);
  if (!file_system_->Open(0, cb)) {
    ReportNonPpapiError("failed to open file system.");
  }
}

void PnaclCoordinator::FileSystemDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::FileSystemDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error, "file system didn't open.");
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

  if (!plugin_->StreamAsFile(pexe_url_,
                             manifest_->PermitsExtensionUrls(),
                             cb.pp_completion_callback())) {
    ReportNonPpapiError(nacl::string("failed to download ") + pexe_url_ + ".");
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
  llc_subprocess_ = StartSubprocess(kLlcUrl, manifest_);
  if (llc_subprocess_ == NULL) {
    ReportPpapiError(PP_ERROR_FAILED);
    return;
  }
  ld_subprocess_ = StartSubprocess(kLdUrl, manifest_);
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
    ReportNonPpapiError("could not allocate thread struct.");
    return;
  }
  const int32_t kArbitraryStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(translate_thread_.get(),
                                DoTranslateThread,
                                this,
                                kArbitraryStackSize)) {
    ReportNonPpapiError("could not create thread.");
  }
}

NaClSubprocess* PnaclCoordinator::StartSubprocess(
    const nacl::string& url_for_nexe,
    const Manifest* manifest) {
  PLUGIN_PRINTF(("PnaclCoordinator::StartSubprocess (url_for_nexe=%s)\n",
                 url_for_nexe.c_str()));
  nacl::DescWrapper* wrapper = resources_->WrapperForUrl(url_for_nexe);
  NaClSubprocessId id =
      plugin_->LoadHelperNaClModule(wrapper, manifest, &error_info_);
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
  NaClSubprocess* ld_subprocess = coordinator->ld_subprocess_;
  nacl::DescWrapper* ld_in_file = coordinator->obj_file_->read_wrapper();
  nacl::DescWrapper* ld_out_file = coordinator->nexe_file_->write_wrapper();
  if (!PnaclSrpcLib::InvokeSrpcMethod(browser_interface,
                                      ld_subprocess,
                                      "RunWithDefaultCommandLine",
                                      "hhiCC",
                                      &params,
                                      ld_in_file->desc(),
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

}  // namespace plugin
