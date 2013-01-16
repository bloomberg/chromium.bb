// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_coordinator.h"

#include <utility>
#include <vector>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/plugin/local_temp_file.h"
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_translate_thread.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/temporary_file.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"

namespace {
const char kPnaclTempDir[] = "/.pnacl";
const uint32_t kCopyBufSize = 512 << 10;
}

namespace plugin {

//////////////////////////////////////////////////////////////////////
//  Pnacl-specific manifest support.
//////////////////////////////////////////////////////////////////////

class PnaclManifest : public Manifest {
 public:
  PnaclManifest(const pp::URLUtil_Dev* url_util, bool use_extension)
      : url_util_(url_util),
        manifest_base_url_(PnaclUrls::GetBaseUrl(use_extension)) {
    // TODO(jvoung): get rid of use_extension when we no longer rely
    // on the chrome webstore extension.  Most of this Manifest stuff
    // can also be simplified then.
  }
  virtual ~PnaclManifest() { }

  virtual bool GetProgramURL(nacl::string* full_url,
                             nacl::string* cache_identity,
                             ErrorInfo* error_info,
                             bool* pnacl_translate) const {
    // Does not contain program urls.
    UNREFERENCED_PARAMETER(full_url);
    UNREFERENCED_PARAMETER(cache_identity);
    UNREFERENCED_PARAMETER(error_info);
    UNREFERENCED_PARAMETER(pnacl_translate);
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
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclManifest);

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

// Out-of-line destructor to keep it from getting put in every .o where
// callback_source.h is included
template<>
CallbackSource<FileStreamData>::~CallbackSource() {}

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
  coordinator->off_the_record_ =
      plugin->nacl_interface()->IsOffTheRecord();
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (manifest=%p, "
                 "off_the_record=%b)\n",
                 reinterpret_cast<const void*>(coordinator->manifest_.get()),
                 coordinator->off_the_record_));

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
  coordinator->resources_->StartLoad();
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
      ReportPpapiError(ERROR_PNACL_RESOURCE_FETCH,
                       pp_error,
                       component + " load failed.");
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
  : translate_finish_error_(PP_OK),
    plugin_(plugin),
    translate_notify_callback_(translate_notify_callback),
    file_system_(new pp::FileSystem(plugin, PP_FILESYSTEMTYPE_LOCALTEMPORARY)),
    manifest_(new PnaclManifest(
        plugin->url_util(),
        plugin::PnaclUrls::UsePnaclExtension(plugin))),
    pexe_url_(pexe_url),
    cache_identity_(cache_identity),
    error_already_reported_(false),
    off_the_record_(false) {
  PLUGIN_PRINTF(("PnaclCoordinator::PnaclCoordinator (this=%p, plugin=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(plugin)));
  callback_factory_.Initialize(this);
  ld_manifest_.reset(new PnaclLDManifest(plugin_->manifest(), manifest_.get()));
}

PnaclCoordinator::~PnaclCoordinator() {
  PLUGIN_PRINTF(("PnaclCoordinator::~PnaclCoordinator (this=%p, "
                 "translate_thread=%p\n",
                 static_cast<void*>(this), translate_thread_.get()));
  // Stopping the translate thread will cause the translate thread to try to
  // run translation_complete_callback_ on the main thread.  This destructor is
  // running from the main thread, and by the time it exits, callback_factory_
  // will have been destroyed.  This will result in the cancellation of
  // translation_complete_callback_, so no notification will be delivered.
  if (translate_thread_.get() != NULL) {
    translate_thread_->AbortSubprocesses();
  }
}

void PnaclCoordinator::ReportNonPpapiError(enum PluginErrorCode err_code,
                                           const nacl::string& message) {
  error_info_.SetReport(err_code,
                        nacl::string("PnaclCoordinator: ") + message);
  ExitWithError();
}

void PnaclCoordinator::ReportPpapiError(enum PluginErrorCode err_code,
                                        int32_t pp_error,
                                        const nacl::string& message) {
  nacl::stringstream ss;
  ss << "PnaclCoordinator: " << message << " (pp_error=" << pp_error << ").";
  error_info_.SetReport(err_code, ss.str());
  ExitWithError();
}

void PnaclCoordinator::ExitWithError() {
  PLUGIN_PRINTF(("PnaclCoordinator::ExitWithError (error_code=%d, "
                 "message='%s')\n",
                 error_info_.error_code(),
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
    translate_notify_callback_.Run(PP_ERROR_FAILED);
  } else {
    PLUGIN_PRINTF(("PnaclCoordinator::ExitWithError an earlier error was "
                   "already reported -- Skipping.\n"));
  }
}

// Signal that Pnacl translation completed normally.
void PnaclCoordinator::TranslateFinished(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::TranslateFinished (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Bail out if there was an earlier error (e.g., pexe load failure).
  if (translate_finish_error_ != PP_OK) {
    ExitWithError();
    return;
  }
  // Bail out if there is an error from the translation thread.
  if (pp_error != PP_OK) {
    ExitWithError();
    return;
  }

  // The nexe is written to the temp_nexe_file_.  We must Reset() the file
  // pointer to be able to read it again from the beginning.
  temp_nexe_file_->Reset();

  if (cache_identity_ != "" && cached_nexe_file_ != NULL) {
    // We are using a cache, but had a cache miss, which is why we did the
    // translation.  Reset cached_nexe_file_ to have a random name,
    // for scratch purposes, before renaming to the final cache_identity_.
    cached_nexe_file_.reset(new LocalTempFile(plugin_, file_system_.get(),
                                              nacl::string(kPnaclTempDir)));
    pp::CompletionCallback cb = callback_factory_.NewCallback(
        &PnaclCoordinator::CachedNexeOpenedForWrite);
    cached_nexe_file_->OpenWrite(cb);
  } else {
    // For now, tolerate bitcode that is missing a cache identity, and
    // tolerate the lack of caching in incognito mode.
    PLUGIN_PRINTF(("PnaclCoordinator -- not caching.\n"));
    NexeReadDidOpen(PP_OK);
  }
}

void PnaclCoordinator::CachedNexeOpenedForWrite(int32_t pp_error) {
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_NOACCESS) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_FILEOPEN_NOACCESS,
          pp_error,
          "PNaCl translation cache failed to open file for write "
          "(no access).");
      return;
    }
    if (pp_error == PP_ERROR_NOQUOTA) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_FILEOPEN_NOQUOTA,
          pp_error,
          "PNaCl translation cache failed to open file for write "
          "(no quota).");
      return;
    }
    if (pp_error == PP_ERROR_NOSPACE) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_FILEOPEN_NOSPACE,
          pp_error,
          "PNaCl translation cache failed to open file for write "
          "(no space).");
      return;
    }
    if (pp_error == PP_ERROR_NOTAFILE) {
      ReportPpapiError(ERROR_PNACL_CACHE_FILEOPEN_NOTAFILE,
                       pp_error,
                       "PNaCl translation cache failed to open file for write."
                       "  File already exists as a directory.");
      return;
    }
    ReportPpapiError(ERROR_PNACL_CACHE_FILEOPEN_OTHER,
                     pp_error,
                     "PNaCl translation cache failed to open file for write.");
    return;
  }

  // Copy the contents from temp_nexe_file_ -> cached_nexe_file_,
  // then rename the cached_nexe_file_ file to the cache id.
  int64_t cur_offset = 0;
  nacl::DescWrapper* read_wrapper = temp_nexe_file_->read_wrapper();
  char buf[kCopyBufSize];
  int32_t num_read =
    nacl::assert_cast<int32_t>(read_wrapper->Read(buf, sizeof buf));
  // Hit EOF or something.
  if (num_read == 0) {
    NexeWasCopiedToCache(PP_OK);
    return;
  }
  if (num_read < 0) {
    PLUGIN_PRINTF(("PnaclCoordinator::CachedNexeOpenedForWrite read failed "
                   "(error=%"NACL_PRId32")\n", num_read));
    NexeWasCopiedToCache(PP_ERROR_FAILED);
    return;
  }
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &PnaclCoordinator::DidCopyNexeToCachePartial, num_read, cur_offset);
  cached_nexe_file_->write_file_io()->Write(cur_offset, buf, num_read, cb);
}

void PnaclCoordinator::DidCopyNexeToCachePartial(int32_t pp_error,
                                                 int32_t num_read_prev,
                                                 int64_t cur_offset) {
  PLUGIN_PRINTF(("PnaclCoordinator::DidCopyNexeToCachePartial "
                 "(pp_error=%"NACL_PRId32", num_read_prev=%"NACL_PRId32""
                 ", cur_offset=%"NACL_PRId64").\n",
                 pp_error, num_read_prev, cur_offset));
  // Assume we are done.
  if (pp_error == PP_OK) {
    NexeWasCopiedToCache(PP_OK);
    return;
  }
  if (pp_error < PP_OK) {
    PLUGIN_PRINTF(("PnaclCoordinator::DidCopyNexeToCachePartial failed (err=%"
                   NACL_PRId32")\n", pp_error));
    NexeWasCopiedToCache(pp_error);
    return;
  }

  // Check if we wrote as much as we read.
  nacl::DescWrapper* read_wrapper = temp_nexe_file_->read_wrapper();
  if (pp_error != num_read_prev) {
    PLUGIN_PRINTF(("PnaclCoordinator::DidCopyNexeToCachePartial partial "
                   "write (bytes_written=%"NACL_PRId32" vs "
                   "read=%"NACL_PRId32")\n", pp_error, num_read_prev));
    CHECK(pp_error < num_read_prev);
    // Seek back to re-read the bytes that were not written.
    nacl_off64_t seek_result =
        read_wrapper->Seek(pp_error - num_read_prev, SEEK_CUR);
    if (seek_result < 0) {
      PLUGIN_PRINTF(("PnaclCoordinator::DidCopyNexeToCachePartial seek failed "
                     "(err=%"NACL_PRId64")\n", seek_result));
      NexeWasCopiedToCache(PP_ERROR_FAILED);
      return;
    }
  }

  int64_t next_offset = cur_offset + pp_error;
  char buf[kCopyBufSize];
  int32_t num_read =
    nacl::assert_cast<int32_t>(read_wrapper->Read(buf, sizeof buf));
  PLUGIN_PRINTF(("PnaclCoordinator::DidCopyNexeToCachePartial read (bytes=%"
                 NACL_PRId32")\n", num_read));
  // Hit EOF or something.
  if (num_read == 0) {
    NexeWasCopiedToCache(PP_OK);
    return;
  }
  if (num_read < 0) {
    PLUGIN_PRINTF(("PnaclCoordinator::DidCopyNexeToCachePartial read failed "
                   "(error=%"NACL_PRId32")\n", num_read));
    NexeWasCopiedToCache(PP_ERROR_FAILED);
    return;
  }
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &PnaclCoordinator::DidCopyNexeToCachePartial, num_read, next_offset);
  PLUGIN_PRINTF(("PnaclCoordinator::CopyNexeToCache Writing ("
                 "bytes=%"NACL_PRId32", buf=%p, file_io=%p)\n", num_read, buf,
                 cached_nexe_file_->write_file_io()));
  cached_nexe_file_->write_file_io()->Write(next_offset, buf, num_read, cb);
}

void PnaclCoordinator::NexeWasCopiedToCache(int32_t pp_error) {
  if (pp_error != PP_OK) {
    // TODO(jvoung): This should try to delete the partially written
    // cache file before returning...
    if (pp_error == PP_ERROR_NOQUOTA) {
      ReportPpapiError(ERROR_PNACL_CACHE_FINALIZE_COPY_NOQUOTA,
                       pp_error,
                       "Failed to copy translated nexe to cache (no quota).");
      return;
    }
    if (pp_error == PP_ERROR_NOSPACE) {
      ReportPpapiError(ERROR_PNACL_CACHE_FINALIZE_COPY_NOSPACE,
                       pp_error,
                       "Failed to copy translated nexe to cache (no space).");
      return;
    }
    ReportPpapiError(ERROR_PNACL_CACHE_FINALIZE_COPY_OTHER,
                     pp_error,
                     "Failed to copy translated nexe to cache.");
    return;
  }
  // Rename the cached_nexe_file_ file to the cache id, to finalize.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::NexeFileWasRenamed);
  cached_nexe_file_->Rename(cache_identity_, cb);
}

void PnaclCoordinator::NexeFileWasRenamed(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeFileWasRenamed (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_NOACCESS) {
      ReportPpapiError(ERROR_PNACL_CACHE_FINALIZE_RENAME_NOACCESS,
                       pp_error,
                       "Failed to finalize cached translation (no access).");
      return;
    } else if (pp_error != PP_ERROR_FILEEXISTS) {
      ReportPpapiError(ERROR_PNACL_CACHE_FINALIZE_RENAME_OTHER,
                       pp_error,
                       "Failed to finalize cached translation.");
      return;
    } else { // pp_error == PP_ERROR_FILEEXISTS.
      // NOTE: if the file already existed, it looks like the rename will
      // happily succeed.  However, we should add a test for this.
      // Could be a hash collision, or it could also be two tabs racing to
      // translate the same pexe.  The file could also be a corrupt left-over,
      // but that case can be removed by doing the TODO for cleanup.
      // We may want UMA stats to know if this happens.
      // For now, assume that it is a race and try to continue.
      // If there is truly a corrupted file, then sel_ldr should prevent the
      // file from loading due to the file size not matching the ELF header.
      PLUGIN_PRINTF(("PnaclCoordinator::NexeFileWasRenamed file existed\n"));
    }
  }

  cached_nexe_file_->FinishRename();

  // TODO(dschuff,jvoung): We could have a UMA stat for the size of
  // the cached nexes, to know how much cache we are using.

  // Open the cache file for reading.
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::NexeReadDidOpen);
  cached_nexe_file_->OpenRead(cb);
}

void PnaclCoordinator::NexeReadDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeReadDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_FILENOTFOUND) {
      ReportPpapiError(ERROR_PNACL_CACHE_FETCH_NOTFOUND,
                       pp_error,
                       "Failed to open translated nexe (not found).");
      return;
    }
    if (pp_error == PP_ERROR_NOACCESS) {
      ReportPpapiError(ERROR_PNACL_CACHE_FETCH_NOACCESS,
                       pp_error,
                       "Failed to open translated nexe (no access).");
      return;
    }
    ReportPpapiError(ERROR_PNACL_CACHE_FETCH_OTHER,
                     pp_error,
                     "Failed to open translated nexe.");
    return;
  }

  // Transfer ownership of cache/temp file's wrapper to the coordinator.
  if (cached_nexe_file_ != NULL) {
    translated_fd_.reset(cached_nexe_file_->release_read_wrapper());
  } else {
    translated_fd_.reset(temp_nexe_file_->release_read_wrapper());
  }
  plugin_->EnqueueProgressEvent(Plugin::kProgressEventProgress);
  translate_notify_callback_.Run(pp_error);
}

void PnaclCoordinator::ResourcesDidLoad(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ResourcesDidLoad (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    // Finer-grained error code should have already been reported by
    // the PnaclResources class.
    return;
  }

  if (!off_the_record_) {
    // Open the local temporary FS to see if we get a hit in the cache.
    pp::CompletionCallback cb =
        callback_factory_.NewCallback(&PnaclCoordinator::FileSystemDidOpen);
    int32_t open_error = file_system_->Open(0, cb);
    if (open_error != PP_OK_COMPLETIONPENDING) {
      // At this point, no async request has kicked off to check for
      // permissions, space, etc., so the only error that can be detected
      // now is that an open() is already in progress (or a really terrible
      // error).
      if (pp_error == PP_ERROR_INPROGRESS) {
        ReportPpapiError(
            ERROR_PNACL_CACHE_OPEN_INPROGRESS,
            pp_error,
            "File system for PNaCl translation cache failed to open "
            "(in progress).");
        return;
      }
      ReportPpapiError(
          ERROR_PNACL_CACHE_OPEN_OTHER,
          pp_error,
          "File system for PNaCl translation cache failed to open.");
    }
  } else {
    // We don't have a cache, so do the non-cached codepath.
    CachedFileDidOpen(PP_ERROR_FAILED);
  }
}

void PnaclCoordinator::FileSystemDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::FileSystemDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_NOACCESS) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_OPEN_NOACCESS,
          pp_error,
          "File system for PNaCl translation cache failed to open "
          "(no access).");
      return;
    }
    if (pp_error == PP_ERROR_NOQUOTA) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_OPEN_NOQUOTA,
          pp_error,
          "File system for PNaCl translation cache failed to open "
          "(no quota).");
      return;
    }
    if (pp_error == PP_ERROR_NOSPACE) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_OPEN_NOSPACE,
          pp_error,
          "File system for PNaCl translation cache failed to open "
          "(no space).");
      return;
    }
    ReportPpapiError(ERROR_PNACL_CACHE_OPEN_OTHER,
                     pp_error,
                     "File system for PNaCl translation cache failed to open.");
  }
  dir_ref_.reset(new pp::FileRef(*file_system_, kPnaclTempDir));
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
    if (pp_error == PP_ERROR_NOACCESS) {
      ReportPpapiError(
          ERROR_PNACL_CACHE_DIRECTORY_CREATE,
          pp_error,
          "PNaCl translation cache directory creation/check failed "
          "(no access).");
      return;
    }
    ReportPpapiError(
        ERROR_PNACL_CACHE_DIRECTORY_CREATE,
        pp_error,
        "PNaCl translation cache directory creation/check failed.");
    return;
  }
  if (cache_identity_ != "") {
    cached_nexe_file_.reset(new LocalTempFile(plugin_, file_system_.get(),
                                              nacl::string(kPnaclTempDir),
                                              cache_identity_));
    pp::CompletionCallback cb =
        callback_factory_.NewCallback(&PnaclCoordinator::CachedFileDidOpen);
    cached_nexe_file_->OpenRead(cb);
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
  // Otherwise, the cache file is missing, or the cache simply
  // cannot be created (e.g., incognito mode), so we must translate.

  // Create the translation thread object immediately. This ensures that any
  // pieces of the file that get downloaded before the compilation thread
  // is accepting SRPCs won't get dropped.
  translate_thread_.reset(new PnaclTranslateThread());
  if (translate_thread_ == NULL) {
    ReportNonPpapiError(ERROR_PNACL_THREAD_CREATE,
                        "could not allocate translation thread.");
    return;
  }
  // We also want to open the object file now so the
  // translator can start writing to it during streaming translation.
  obj_file_.reset(new TempFile(plugin_));
  pp::CompletionCallback obj_cb =
    callback_factory_.NewCallback(&PnaclCoordinator::ObjectFileDidOpen);
  obj_file_->Open(obj_cb);

  streaming_downloader_.reset(new FileDownloader());
  streaming_downloader_->Initialize(plugin_);
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &PnaclCoordinator::BitcodeStreamDidFinish);

  if (!streaming_downloader_->OpenStream(pexe_url_, cb, this)) {
    ReportNonPpapiError(ERROR_PNACL_PEXE_FETCH_OTHER,
                        nacl::string("failed to open stream ") + pexe_url_);
  }
}

void PnaclCoordinator::BitcodeStreamDidFinish(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeStreamDidFinish (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    // Defer reporting the error and cleanup until after the translation
    // thread returns, because it may be accessing the coordinator's
    // objects or writing to the files.
    translate_finish_error_ = pp_error;
    if (pp_error == PP_ERROR_ABORTED) {
      error_info_.SetReport(ERROR_PNACL_PEXE_FETCH_ABORTED,
                            "PnaclCoordinator: pexe load failed (aborted).");
    }
    if (pp_error == PP_ERROR_NOACCESS) {
      error_info_.SetReport(ERROR_PNACL_PEXE_FETCH_NOACCESS,
                            "PnaclCoordinator: pexe load failed (no access).");
    } else {
      nacl::stringstream ss;
      ss << "PnaclCoordinator: pexe load failed (pp_error=" << pp_error << ").";
      error_info_.SetReport(ERROR_PNACL_PEXE_FETCH_OTHER, ss.str());
    }
    translate_thread_->AbortSubprocesses();
  }
}

void PnaclCoordinator::BitcodeStreamGotData(int32_t pp_error,
                                            FileStreamData data) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeStreamGotData (pp_error=%"
                 NACL_PRId32", data=%p)\n", pp_error, data ? &(*data)[0] : 0));
  DCHECK(translate_thread_.get());
  translate_thread_->PutBytes(data, pp_error);
}

StreamCallback PnaclCoordinator::GetCallback() {
  return callback_factory_.NewCallbackWithOutput(
      &PnaclCoordinator::BitcodeStreamGotData);
}

void PnaclCoordinator::ObjectFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::ObjectFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    ReportPpapiError(ERROR_PNACL_CREATE_TEMP,
                     pp_error,
                     "Failed to open scratch object file.");
    return;
  }
  // Create the nexe file for connecting ld and sel_ldr.
  // Start translation when done with this last step of setup!
  temp_nexe_file_.reset(new TempFile(plugin_));
  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&PnaclCoordinator::RunTranslate);
  temp_nexe_file_->Open(cb);
}

void PnaclCoordinator::RunTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunTranslate (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  pp::CompletionCallback report_translate_finished =
      callback_factory_.NewCallback(&PnaclCoordinator::TranslateFinished);

  CHECK(translate_thread_ != NULL);
  translate_thread_->RunTranslate(report_translate_finished,
                                  manifest_.get(),
                                  ld_manifest_.get(),
                                  obj_file_.get(),
                                  temp_nexe_file_.get(),
                                  &error_info_,
                                  resources_.get(),
                                  plugin_);
}

}  // namespace plugin
