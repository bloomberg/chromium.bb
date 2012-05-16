// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_coordinator.h"

#include <utility>
#include <vector>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/plugin/local_temp_file.h"
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_translate_thread.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"

namespace {
const char kPnaclTempDir[] = "/.pnacl";
}

namespace plugin {

//////////////////////////////////////////////////////////////////////
//  Pnacl-specific manifest support.
//////////////////////////////////////////////////////////////////////
class ExtensionManifest : public Manifest {
 public:
  explicit ExtensionManifest(const pp::URLUtil_Dev* url_util)
      : url_util_(url_util),
        manifest_base_url_(PnaclUrls::GetExtensionUrl()) { }
  virtual ~ExtensionManifest() { }

  virtual bool GetProgramURL(nacl::string* full_url,
                             nacl::string* cache_identity,
                             ErrorInfo* error_info,
                             bool* pnacl_translate) const {
    // Does not contain program urls.
    UNREFERENCED_PARAMETER(full_url);
    UNREFERENCED_PARAMETER(cache_identity);
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
                          nacl::string* cache_identity,
                          ErrorInfo* error_info,
                          bool* pnacl_translate) const {
    // All of the extension files are native (do not require pnacl translate).
    *pnacl_translate = false;
    // Do not cache these entries.
    *cache_identity = "";
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
                             nacl::string* cache_identity,
                             ErrorInfo* error_info,
                             bool* pnacl_translate) const {
    if (nexe_manifest_->GetProgramURL(full_url, cache_identity,
                                      error_info, pnacl_translate)) {
      return true;
    }
    return extension_manifest_->GetProgramURL(full_url, cache_identity,
                                              error_info, pnacl_translate);
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
                          nacl::string* cache_identity,
                          ErrorInfo* error_info,
                          bool* pnacl_translate) const {
    if (nexe_manifest_->ResolveKey(key, full_url, cache_identity,
                                   error_info, pnacl_translate)) {
      return true;
    }
    return extension_manifest_->ResolveKey(key, full_url, cache_identity,
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
    const nacl::string& cache_identity,
    const pp::CompletionCallback& translate_notify_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (plugin=%p, pexe=%s)\n",
                 static_cast<void*>(plugin), pexe_url.c_str()));
  PnaclCoordinator* coordinator =
      new PnaclCoordinator(plugin, pexe_url,
                           cache_identity, translate_notify_callback);
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (manifest=%p)\n",
                 reinterpret_cast<const void*>(coordinator->manifest_.get())));
  // Load llc and ld.
  std::vector<nacl::string> resource_urls;
  resource_urls.push_back(PnaclUrls::GetLlcUrl());
  resource_urls.push_back(PnaclUrls::GetLdUrl());
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
  ErrorInfo error_info;
  int32_t file_desc_ok_to_close = plugin_->GetPOSIXFileDesc(url);
  if (pp_error != PP_OK || file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      plugin_->ReportLoadAbort();
    } else {
      ReportPpapiError(pp_error, component + " load failed.");
    }
    return NACL_NO_FILE_DESC;
  }
  return file_desc_ok_to_close;
}

PnaclCoordinator::PnaclCoordinator(
    Plugin* plugin,
    const nacl::string& pexe_url,
    const nacl::string& cache_identity,
    const pp::CompletionCallback& translate_notify_callback)
  : plugin_(plugin),
    translate_notify_callback_(translate_notify_callback),
    file_system_(new pp::FileSystem(plugin, PP_FILESYSTEMTYPE_LOCALTEMPORARY)),
    manifest_(new ExtensionManifest(plugin->url_util())),
    pexe_url_(pexe_url),
    cache_identity_(cache_identity),
    error_already_reported_(false) {
  PLUGIN_PRINTF(("PnaclCoordinator::PnaclCoordinator (this=%p, plugin=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(plugin)));
  callback_factory_.Initialize(this);
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
    translate_thread_->SetSubprocessesShouldDie(true);
  }
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
                 NACL_PRId32", error_code=%d, message='%s')\n",
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

// Signal that Pnacl translation completed normally.
void PnaclCoordinator::TranslateFinished(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::TranslateFinished (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Save the translate error code, and inspect after cleaning up junk files.
  // Note: If there was a surfaway and the file objects were actually
  // destroyed, then we are in trouble since the obj_file_, nexe_file_,
  // etc. may have been destroyed.
  // TODO(jvoung,sehr): Fix.
  translate_finish_error_ = pp_error;

  // Close the object temporary file (regardless of error code).
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
  // Now that cleanup of the obj file is done, check the old TranslateFinished
  // error code to see if we should proceed normally or not.
  if (translate_finish_error_ != PP_OK) {
    pp::CompletionCallback cb =
        callback_factory_.NewCallback(&PnaclCoordinator::NexeFileWasDeleted);
    nexe_file_->Delete(cb);
    return;
  }

  // Rename the nexe file to the cache id.
  if (cache_identity_ != "") {
    pp::CompletionCallback cb =
         callback_factory_.NewCallback(&PnaclCoordinator::NexeFileWasRenamed);
    nexe_file_->Rename(cache_identity_, cb);
  } else {
    // For now tolerate bitcode that is missing a cache identity.
    PLUGIN_PRINTF(("PnaclCoordinator -- WARNING: missing cache identity,"
                   " not caching.\n"));
    NexeFileWasRenamed(PP_OK);
  }
}

void PnaclCoordinator::NexeFileWasRenamed(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeFileWasRenamed (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // NOTE: if the file already existed, it looks like the rename will
  // happily succeed. However, we should add a test for this.
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error, "Failed to place cached bitcode translation.");
    return;
  }
  nexe_file_->FinishRename();
  // Open the nexe temporary file for reading.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::NexeReadDidOpen);
  nexe_file_->OpenRead(cb);
}

void PnaclCoordinator::NexeReadDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeReadDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(pp_error, "Failed to open translated nexe.");
    return;
  }
  // Transfer ownership of the nexe wrapper to the coordinator.
  translated_fd_.reset(nexe_file_->release_read_wrapper());
  plugin_->EnqueueProgressEvent(Plugin::kProgressEventProgress);
  translate_notify_callback_.Run(pp_error);
}

void PnaclCoordinator::NexeFileWasDeleted(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeFileWasDeleted (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  ReportPpapiError(translate_finish_error_);
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
  dir_ref_.reset(new pp::FileRef(*file_system_,
                                 kPnaclTempDir));
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
  if (cache_identity_ != "") {
    nexe_file_.reset(new LocalTempFile(plugin_, file_system_.get(),
                                       nacl::string(kPnaclTempDir),
                                       cache_identity_));
    pp::CompletionCallback cb =
        callback_factory_.NewCallback(&PnaclCoordinator::CachedFileDidOpen);
    nexe_file_->OpenRead(cb);
  } else {
    // For now, tolerate lack of cache identity...
    CachedFileDidOpen(PP_ERROR_FAILED);
  }
}

void PnaclCoordinator::CachedFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::CachedFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error == PP_OK) {
    NexeReadDidOpen(PP_OK);
    return;
  }
  // Otherwise, load the pexe and set up temp files for translation.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::BitcodeFileDidOpen);
  if (!plugin_->StreamAsFile(pexe_url_, cb.pp_completion_callback())) {
    ReportNonPpapiError(nacl::string("failed to download ") + pexe_url_ + ".");
  }
}

void PnaclCoordinator::BitcodeFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // We have to get the fd immediately after streaming, otherwise it
  // seems like the temp file will get GC'ed.
  int32_t fd = GetLoadedFileDesc(pp_error, pexe_url_, "pexe");
  if (fd < 0) {
    // Error already reported by GetLoadedFileDesc().
    return;
  }
  pexe_wrapper_.reset(plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));

  obj_file_.reset(new LocalTempFile(plugin_, file_system_.get(),
                                    nacl::string(kPnaclTempDir)));
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
  // Start translation when done with this last step of setup!
  nexe_file_.reset(new LocalTempFile(plugin_, file_system_.get(),
                                     nacl::string(kPnaclTempDir)));
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::RunTranslate);
  nexe_file_->OpenWrite(cb);
}

void PnaclCoordinator::RunTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunTranslate (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  pp::CompletionCallback report_translate_finished =
      callback_factory_.NewCallback(&PnaclCoordinator::TranslateFinished);
  translate_thread_.reset(new PnaclTranslateThread());
  if (translate_thread_ == NULL) {
    ReportNonPpapiError("could not allocate translation thread.");
    return;
  }
  translate_thread_->RunTranslate(report_translate_finished,
                                  manifest_.get(),
                                  ld_manifest_.get(),
                                  obj_file_.get(),
                                  nexe_file_.get(),
                                  pexe_wrapper_.get(),
                                  &error_info_,
                                  resources_.get(),
                                  plugin_);
}

}  // namespace plugin
