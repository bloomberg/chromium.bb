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
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/srpc_params.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"

namespace plugin {

class Plugin;

namespace {

const char kLlcUrl[] = "llc";
const char kLdUrl[] = "ld";
const char kPnaclTempDir[] = "/.pnacl";

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
//  Local temporary file access.
//////////////////////////////////////////////////////////////////////

uint32_t LocalTempFile::next_identifier = 0;

LocalTempFile::LocalTempFile(Plugin* plugin,
                             pp::FileSystem* file_system,
                             PnaclCoordinator* coordinator)
    : plugin_(plugin),
      file_system_(file_system),
      coordinator_(coordinator) {
  PLUGIN_PRINTF(("LocalTempFile::LocalTempFile (plugin=%p, "
                 "file_system=%p, coordinator=%p)\n",
                 static_cast<void*>(plugin), static_cast<void*>(file_system),
                 static_cast<void*>(coordinator)));
  callback_factory_.Initialize(this);
  rng_desc_ = (struct NaClDescRng *) malloc(sizeof *rng_desc_);
  CHECK(rng_desc_ != NULL);
  CHECK(NaClDescRngCtor(rng_desc_));
  file_io_trusted_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  ++next_identifier;
  SNPRINTF(reinterpret_cast<char *>(identifier_), sizeof identifier_,
           "%"NACL_PRIu32, next_identifier);
}

LocalTempFile::~LocalTempFile() {
  PLUGIN_PRINTF(("LocalTempFile::~LocalTempFile\n"));
  NaClDescUnref(reinterpret_cast<NaClDesc*>(rng_desc_));
}

void LocalTempFile::OpenWrite(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::OpenWrite\n"));
  done_callback_ = cb;
  // If we don't already have a filename, generate one.
  if (filename_ == "") {
    // Get a random temp file name.
    filename_ =
        nacl::string(kPnaclTempDir) + "/" + Random32CharHexString(rng_desc_);
    // Remember the ref used to open for writing and reading.
    file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  }
  // Open the writeable file.
  write_io_.reset(new pp::FileIO(plugin_));
  pp::CompletionCallback open_write_cb =
      callback_factory_.NewCallback(&LocalTempFile::WriteFileDidOpen);
  write_io_->Open(*file_ref_,
                  PP_FILEOPENFLAG_WRITE |
                  PP_FILEOPENFLAG_CREATE |
                  PP_FILEOPENFLAG_EXCLUSIVE,
                  open_write_cb);
}

int32_t LocalTempFile::GetFD(int32_t pp_error,
                             const pp::Resource& resource,
                             bool is_writable) {
  PLUGIN_PRINTF(("LocalTempFile::GetFD (pp_error=%"NACL_PRId32
                 ", is_writable=%d)\n", pp_error, is_writable));
  if (pp_error != PP_OK) {
    PLUGIN_PRINTF(("LocalTempFile::GetFD pp_error != PP_OK\n"));
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
    PLUGIN_PRINTF(("LocalTempFile::GetFD _open_osfhandle failed.\n"));
    return NACL_NO_FILE_DESC;
  }
  file_desc = posix_desc;
#endif
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    PLUGIN_PRINTF(("LocalTempFile::GetFD dup failed.\n"));
    return -1;
  }
  return file_desc_ok_to_close;
}

void LocalTempFile::WriteFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("LocalTempFile::WriteFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error == PP_ERROR_FILEEXISTS) {
    // Filenames clashed, retry.
    filename_ = "";
    OpenWrite(done_callback_);
  }
  // Remember the object temporary file descriptor.
  int32_t fd = GetFD(pp_error, *write_io_, kWriteable);
  if (fd < 0) {
    coordinator_->ReportNonPpapiError("could not open write temp file.");
    return;
  }
  // The descriptor for a writeable file needs to have quota management.
  write_wrapper_.reset(
      plugin_->wrapper_factory()->MakeFileDescQuota(fd, O_RDWR, identifier_));
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, done_callback_, PP_OK);
}

void LocalTempFile::OpenRead(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::OpenRead\n"));
  done_callback_ = cb;
  // Open the read only file.
  read_io_.reset(new pp::FileIO(plugin_));
  pp::CompletionCallback open_read_cb =
      callback_factory_.NewCallback(&LocalTempFile::ReadFileDidOpen);
  read_io_->Open(*file_ref_, PP_FILEOPENFLAG_READ, open_read_cb);
}

void LocalTempFile::ReadFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("LocalTempFile::ReadFileDidOpen (pp_error=%"
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

void LocalTempFile::Close(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::Close\n"));
  // Close the open DescWrappers and FileIOs.
  if (write_io_.get() != NULL) {
    write_io_->Close();
  }
  write_wrapper_.reset(NULL);
  write_io_.reset(NULL);
  if (read_io_.get() != NULL) {
    read_io_->Close();
  }
  read_wrapper_.reset(NULL);
  read_io_.reset(NULL);
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, cb, PP_OK);
}

void LocalTempFile::Delete(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::Delete\n"));
  file_ref_->Delete(cb);
}

void LocalTempFile::Rename(const nacl::string& new_name,
                           const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::Rename\n"));
  // Rename the temporary file.
  filename_ = new_name;
  nacl::scoped_ptr<pp::FileRef> old_ref(file_ref_.release());
  file_ref_.reset(new pp::FileRef(*file_system_, new_name.c_str()));
  old_ref->Rename(*file_ref_, cb);
}

//////////////////////////////////////////////////////////////////////
//  Pnacl-specific manifest support.
//////////////////////////////////////////////////////////////////////
class ExtensionManifest : public Manifest {
 public:
  explicit ExtensionManifest(const pp::URLUtil_Dev* url_util)
      : url_util_(url_util),
        manifest_base_url_(ExtensionUrl()) { }
  virtual ~ExtensionManifest() { }

  virtual bool GetProgramURL(nacl::string* full_url,
                             ErrorInfo* error_info,
                             bool* pnacl_translate) const {
    // Does not contain program urls.
    UNREFERENCED_PARAMETER(full_url);
    UNREFERENCED_PARAMETER(error_info);
    UNREFERENCED_PARAMETER(pnacl_translate);
    PLUGIN_PRINTF(("ExtensionManifest does not contain a program\n"));
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
    PLUGIN_PRINTF(("ExtensionManifest does not support key enumeration\n"));
    UNREFERENCED_PARAMETER(keys);
    return false;
  }

  virtual bool ResolveKey(const nacl::string& key,
                          nacl::string* full_url,
                          ErrorInfo* error_info,
                          bool* pnacl_translate) const {
    // All of the extension files are native (do not require pnacl translate).
    *pnacl_translate = false;
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ExtensionManifest);

  const pp::URLUtil_Dev* url_util_;
  nacl::string manifest_base_url_;
};

// TEMPORARY: ld needs to look up dynamic libraries in the nexe's manifest
// until metadata is complete in pexes.  This manifest lookup allows looking
// for whether a resource requested by ld is in the nexe manifest first, and
// if not, then consults the extension manifest.
// TODO(sehr,jvoung,pdox): remove this when metadata is correct.
class PnaclLDManifest : public Manifest {
 public:
  PnaclLDManifest(const Manifest* nexe_manifest,
                  const Manifest* extension_manifest)
      : nexe_manifest_(nexe_manifest),
        extension_manifest_(extension_manifest) {
    CHECK(nexe_manifest != NULL);
    CHECK(extension_manifest != NULL);
  }
  virtual ~PnaclLDManifest() { }

  virtual bool GetProgramURL(nacl::string* full_url,
                             ErrorInfo* error_info,
                             bool* pnacl_translate) const {
    if (nexe_manifest_->GetProgramURL(full_url, error_info, pnacl_translate)) {
      return true;
    }
    return extension_manifest_->GetProgramURL(full_url, error_info,
                                              pnacl_translate);
  }

  virtual bool ResolveURL(const nacl::string& relative_url,
                          nacl::string* full_url,
                          ErrorInfo* error_info) const {
    if (nexe_manifest_->ResolveURL(relative_url, full_url, error_info)) {
      return true;
    }
    return extension_manifest_->ResolveURL(relative_url, full_url, error_info);
  }

  virtual bool GetFileKeys(std::set<nacl::string>* keys) const {
    if (nexe_manifest_->GetFileKeys(keys)) {
      return true;
    }
    return extension_manifest_->GetFileKeys(keys);
  }

  virtual bool ResolveKey(const nacl::string& key,
                          nacl::string* full_url,
                          ErrorInfo* error_info,
                          bool* pnacl_translate) const {
    if (nexe_manifest_->ResolveKey(key, full_url,
                                   error_info, pnacl_translate)) {
      return true;
    }
    return extension_manifest_->ResolveKey(key, full_url,
                                           error_info, pnacl_translate);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclLDManifest);

  const Manifest* nexe_manifest_;
  const Manifest* extension_manifest_;
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
                 reinterpret_cast<const void*>(coordinator->manifest_.get())));
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
                         coordinator->manifest_.get(),
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
    subprocesses_should_die_(false),
    file_system_(new pp::FileSystem(plugin, PP_FILESYSTEMTYPE_LOCALTEMPORARY)),
    manifest_(new ExtensionManifest(plugin->url_util())),
    pexe_url_(pexe_url),
    error_already_reported_(false) {
  PLUGIN_PRINTF(("PnaclCoordinator::PnaclCoordinator (this=%p, plugin=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(plugin)));
  callback_factory_.Initialize(this);
  NaClXMutexCtor(&subprocess_mu_);
  ld_manifest_.reset(new PnaclLDManifest(plugin_->manifest(), manifest_.get()));
}

PnaclCoordinator::~PnaclCoordinator() {
  PLUGIN_PRINTF(("PnaclCoordinator::~PnaclCoordinator (this=%p)\n",
                 static_cast<void*>(this)));
  // Stopping the translate thread will cause the translate thread to try to
  // run translation_complete_callback_ on the main thread.  This destructor is
  // running from the main thread, and by the time it exits, callback_factory_
  // will have been destroyed.  This will result in the cancellation of
  // translation_complete_callback_, so no notification will be delivered.
  if (translate_thread_.get() != NULL) {
    SetSubprocessesShouldDie(true);
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
    // TODO(sehr): Delete the object and nexe temporary files.
    return;
  }
  // Close the object temporary file.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::ObjectFileWasClosed);
  obj_file_->Close(cb);
}

void PnaclCoordinator::ObjectFileWasClosed(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ObjectFileWasClosed (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Delete the object temporary file.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::ObjectFileWasDeleted);
  obj_file_->Delete(cb);
}

void PnaclCoordinator::ObjectFileWasDeleted(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ObjectFileWasDeleted (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Close the nexe temporary file.
  pp::CompletionCallback cb =
     callback_factory_.NewCallback(&PnaclCoordinator::NexeFileWasClosed);
  nexe_file_->Close(cb);
}

void PnaclCoordinator::NexeFileWasClosed(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeFileWasClosed (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // TODO(sehr): enable renaming once cache ids are available.
  // Rename the nexe file to the cache id.
  // pp::CompletionCallback cb =
  //     callback_factory_.NewCallback(&PnaclCoordinator::NexeReadDidOpen);
  // nexe_file_->Rename(new_name, cb);
  NexeFileWasRenamed(PP_OK);
}

void PnaclCoordinator::NexeFileWasRenamed(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeFileWasRenamed (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Open the nexe temporary file for reading.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::NexeReadDidOpen);
  nexe_file_->OpenRead(cb);
}

void PnaclCoordinator::NexeReadDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeReadDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Transfer ownership of the nexe wrapper to the coordinator.
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
  core->CallOnMainThread(0, report_translate_finished_, PP_ERROR_FAILED);
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
  dir_ref_.reset(new pp::FileRef(*file_system_, kPnaclTempDir));
  dir_io_.reset(new pp::FileIO(plugin_));
  // Attempt to create the directory.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::DirectoryWasCreated);
  dir_ref_->MakeDirectory(cb);
}

void PnaclCoordinator::DirectoryWasCreated(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::DirectoryWasCreated (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_ERROR_FILEEXISTS && pp_error != PP_OK) {
    // Directory did not exist and could not be created.
    ReportPpapiError(pp_error, "directory creation/check failed.");
    return;
  }
  // Create the object file pair for connecting llc and ld.
  obj_file_.reset(new LocalTempFile(plugin_, file_system_.get(), this));
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::ObjectWriteDidOpen);
  obj_file_->OpenWrite(cb);
}

void PnaclCoordinator::ObjectWriteDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ObjectWriteDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::ObjectReadDidOpen);
  obj_file_->OpenRead(cb);
}

void PnaclCoordinator::ObjectReadDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ObjectReadDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Create the nexe file for connecting ld and sel_ldr.
  nexe_file_.reset(new LocalTempFile(plugin_, file_system_.get(), this));
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::NexeWriteDidOpen);
  nexe_file_->OpenWrite(cb);
}

void PnaclCoordinator::NexeWriteDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeWriteDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error);
    return;
  }
  // Load the pexe file and get the translation started.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::RunTranslate);

  if (!plugin_->StreamAsFile(pexe_url_, cb.pp_completion_callback())) {
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
  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  report_translate_finished_ =
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
  nacl::scoped_ptr<NaClSubprocess> subprocess(
      plugin_->LoadHelperNaClModule(wrapper, manifest, &error_info_));
  if (subprocess.get() == NULL) {
    PLUGIN_PRINTF((
        "PnaclCoordinator::StartSubprocess: subprocess creation failed\n"));
    return NULL;
  }
  return subprocess.release();
}

// TODO(sehr): the thread body should be in a class by itself with a delegate
// class for interfacing with the rest of the coordinator.
void WINAPI PnaclCoordinator::DoTranslateThread(void* arg) {
  PnaclCoordinator* coordinator = reinterpret_cast<PnaclCoordinator*>(arg);

  nacl::scoped_ptr<NaClSubprocess> llc_subprocess(
      coordinator->StartSubprocess(kLlcUrl, coordinator->manifest_.get()));
  if (llc_subprocess == NULL) {
    coordinator->TranslateFailed("Compile process could not be created.");
    return;
  }
  // Run LLC.
  SrpcParams params;
  nacl::DescWrapper* llc_out_file = coordinator->obj_file_->write_wrapper();
  PluginReverseInterface* llc_reverse =
      llc_subprocess->service_runtime()->rev_interface();
  llc_reverse->AddQuotaManagedFile(coordinator->obj_file_->identifier(),
                                   coordinator->obj_file_->write_file_io());
  if (!llc_subprocess->InvokeSrpcMethod("RunWithDefaultCommandLine",
                                        "hh",
                                        &params,
                                        coordinator->pexe_wrapper_->desc(),
                                        llc_out_file->desc())) {
    coordinator->TranslateFailed("compile failed.");
    return;
  }
  // LLC returns values that are used to determine how linking is done.
  int is_shared_library = (params.outs()[0]->u.ival != 0);
  nacl::string soname = params.outs()[1]->arrays.str;
  nacl::string lib_dependencies = params.outs()[2]->arrays.str;
  PLUGIN_PRINTF(("PnaclCoordinator: compile (coordinator=%p) succeeded"
                 " is_shared_library=%d, soname='%s', lib_dependencies='%s')\n",
                 arg, is_shared_library, soname.c_str(),
                 lib_dependencies.c_str()));
  // Shut down the llc subprocess.
  llc_subprocess.reset(NULL);
  if (coordinator->SubprocessesShouldDie()) {
    coordinator->TranslateFailed("stopped by coordinator.");
    return;
  }
  nacl::scoped_ptr<NaClSubprocess> ld_subprocess(
      coordinator->StartSubprocess(kLdUrl, coordinator->ld_manifest_.get()));
  if (ld_subprocess == NULL) {
    coordinator->TranslateFailed("Link process could not be created.");
    return;
  }
  // Run LD.
  nacl::DescWrapper* ld_in_file = coordinator->obj_file_->read_wrapper();
  nacl::DescWrapper* ld_out_file = coordinator->nexe_file_->write_wrapper();
  PluginReverseInterface* ld_reverse =
      ld_subprocess->service_runtime()->rev_interface();
  ld_reverse->AddQuotaManagedFile(coordinator->nexe_file_->identifier(),
                                  coordinator->nexe_file_->write_file_io());
  if (!ld_subprocess->InvokeSrpcMethod("RunWithDefaultCommandLine",
                                       "hhiCC",
                                       &params,
                                       ld_in_file->desc(),
                                       ld_out_file->desc(),
                                       is_shared_library,
                                       soname.c_str(),
                                       lib_dependencies.c_str())) {
    coordinator->TranslateFailed("link failed.");
    return;
  }
  PLUGIN_PRINTF(("PnaclCoordinator: link (coordinator=%p) succeeded\n", arg));
  // Shut down the ld subprocess.
  ld_subprocess.reset(NULL);
  if (coordinator->SubprocessesShouldDie()) {
    coordinator->TranslateFailed("stopped by coordinator.");
    return;
  }
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, coordinator->report_translate_finished_, PP_OK);
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
