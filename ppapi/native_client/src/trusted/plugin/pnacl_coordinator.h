// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_

#include <set>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/delayed_callback.h"
#include "native_client/src/trusted/plugin/local_temp_file.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"


namespace plugin {

class Manifest;
class Plugin;
class PnaclCoordinator;
class PnaclTranslateThread;

// A thread safe reference counting class Needed for CompletionCallbackFactory
// in PnaclCoordinator.
class PnaclRefCount {
 public:
  PnaclRefCount() : ref_(0) { NaClXMutexCtor(&mu_); }
  ~PnaclRefCount() { NaClMutexDtor(&mu_); }
  int32_t AddRef() {
    nacl::MutexLocker ml(&mu_);
    return ++ref_;
  }
  int32_t Release() {
    nacl::MutexLocker ml(&mu_);
    return --ref_;
  }

 private:
  int32_t ref_;
  struct NaClMutex mu_;
};

// A class invoked by Plugin to handle PNaCl client-side translation.
// Usage:
// (1) Invoke the factory method, e.g.,
//     PnaclCoordinator* coord = BitcodeToNative(plugin,
//                                               "http://foo.com/my.pexe",
//                                               TranslateNotifyCallback);
// (2) TranslateNotifyCallback gets invoked when translation is complete.
//     If the translation was successful, the pp_error argument is PP_OK.
//     Other values indicate errors.
// (3) After finish_callback runs, get the file descriptor of the translated
//     nexe, e.g.,
//     fd = coord->ReleaseTranslatedFD();
// (4) Load the nexe from "fd".
// (5) delete coord.
//
// Translation proceeds in two steps:
// (1) llc translates the bitcode in pexe_url_ to an object in obj_file_.
// (2) ld links the object code in obj_file_ and produces a nexe in nexe_file_.
//
// The coordinator proceeds through several states.  They are
// LOAD_TRANSLATOR_BINARIES
//     Complete when ResourcesDidLoad is invoked.
// OPEN_LOCAL_FILE_SYSTEM
//     Complete when FileSystemDidOpen is invoked.
// CREATED_PNACL_TEMP_DIRECTORY
//     Complete when DirectoryWasCreated is invoked.
// CACHED_FILE_OPEN
//     Complete with success if cached version is available and jump to end.
//     Otherwise, proceed with usual pipeline of translation.
// OPEN_TMP_WRITE_FOR_LLC_TO_LD_COMMUNICATION
//     Complete when ObjectWriteDidOpen is invoked.
// OPEN_TMP_READ_FOR_LLC_TO_LD_COMMUNICATION
//     Complete when ObjectReadDidOpen is invoked.
// OPEN_TMP_FOR_LD_WRITING
//     Complete when NexeWriteDidOpen is invoked.
// PREPARE_PEXE_FOR_STREAMING
//     Complete when RunTranslate is invoked.
// START_LD_AND_LLC_SUBPROCESS_AND_INITIATE_TRANSLATION
//     Complete when RunTranslate returns.
// TRANSLATION_COMPLETE
//     Complete when TranslateFinished is invoked.
// CLOSE_OBJECT_FILE
//     Complete when ObjectFileWasClosed is invoked.
// DELETE_OBJECT_FILE
//     Complete when ObjectFileWasDeleted is invoked.
// CLOSE_NEXE_FILE
//     Complete when NexeFileWasClosed is invoked.
// RENAME_NEXE_FILE
//     Complete when NexeFileWasRenamed is invoked.
// OPEN_NEXE_FOR_SEL_LDR
//     Complete when NexeReadDidOpen is invoked.
class PnaclCoordinator {
 public:
  virtual ~PnaclCoordinator();

  // The factory method for translations.
  static PnaclCoordinator* BitcodeToNative(
      Plugin* plugin,
      const nacl::string& pexe_url,
      const nacl::string& cache_identity,
      const pp::CompletionCallback& translate_notify_callback);

  // Call this to take ownership of the FD of the translated nexe after
  // BitcodeToNative has completed (and the finish_callback called).
  nacl::DescWrapper* ReleaseTranslatedFD() { return translated_fd_.release(); }

  // Looks up a file descriptor for an url that was already downloaded.
  // This is used for getting the descriptor for llc and ld nexes as well
  // as the libraries and object files used by the linker.
  int32_t GetLoadedFileDesc(int32_t pp_error,
                            const nacl::string& url,
                            const nacl::string& component);

  // Run |translate_notify_callback_| with an error condition that is not
  // PPAPI specific.
  void ReportNonPpapiError(const nacl::string& message);
  // Run when faced with a PPAPI error condition. Bring control back to the
  // plugin by invoking the |translate_notify_callback_|.
  void ReportPpapiError(int32_t pp_error, const nacl::string& message);
  void ReportPpapiError(int32_t pp_error);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclCoordinator);

  // BitcodeToNative is the factory method for PnaclCoordinators.
  // Therefore the constructor is private.
  PnaclCoordinator(Plugin* plugin,
                   const nacl::string& pexe_url,
                   const nacl::string& cache_identity,
                   const pp::CompletionCallback& translate_notify_callback);

  // Callback for when llc and ld have been downloaded.
  // This is the first callback invoked in response to BitcodeToNative.
  void ResourcesDidLoad(int32_t pp_error);

  // Callbacks for temporary file related stages.
  // They are invoked from ResourcesDidLoad and proceed in declaration order.
  // Invoked when the temporary file system is successfully opened in PPAPI.
  void FileSystemDidOpen(int32_t pp_error);
  // Invoked after we are sure the PNaCl temporary directory exists.
  void DirectoryWasCreated(int32_t pp_error);
  // Invoked after we have checked the PNaCl cache for a translated version.
  void CachedFileDidOpen(int32_t pp_error);
  // Invoked after we have started pulling down the bitcode file.
  void BitcodeFileDidOpen(int32_t pp_error);
  // Invoked when the write descriptor for obj_file_ is created.
  void ObjectWriteDidOpen(int32_t pp_error);
  // Invoked when the read descriptor for obj_file_ is created.
  void ObjectReadDidOpen(int32_t pp_error);
  // Invoked when the descriptors for obj_file_ have been closed.
  void ObjectFileWasClosed(int32_t pp_error);
  // Invoked when the obj_file_ temporary has been deleted.
  void ObjectFileWasDeleted(int32_t pp_error);
  // Invoked when the descriptors for nexe_file_ have been closed.
  void NexeFileWasClosed(int32_t pp_error);
  // Invoked when the nexe_file_ temporary has been renamed to the nexe name.
  void NexeFileWasRenamed(int32_t pp_error);
  // Invoked when the read descriptor for nexe_file_ is created.
  void NexeReadDidOpen(int32_t pp_error);
  // Invoked if there was an error and we've cleaned up the nexe_file_ temp.
  void NexeFileWasDeleted(int32_t pp_error);

  // Once llc and ld nexes have been loaded and the two temporary files have
  // been created, this starts the translation.  Translation starts two
  // subprocesses, one for llc and one for ld.
  void RunTranslate(int32_t pp_error);

  void TranslateFinished(int32_t pp_error);
  // Keeps track of the pp_error upon entry to TranslateFinished,
  // for inspection after cleanup.
  int32_t translate_finish_error_;

  // The plugin owning the nexe for which we are doing translation.
  Plugin* plugin_;

  pp::CompletionCallback translate_notify_callback_;
  // PnaclRefCount is only needed to support file lookups.
  // TODO(sehr): remove this when file lookup is through ReverseService.
  pp::CompletionCallbackFactory<PnaclCoordinator,
                                PnaclRefCount> callback_factory_;

  // Nexe from the final native Link.
  nacl::scoped_ptr<nacl::DescWrapper> translated_fd_;

  // The helper thread used to do translations via SRPC.
  nacl::scoped_ptr<PnaclTranslateThread> translate_thread_;
  // Translation creates local temporary files.
  nacl::scoped_ptr<pp::FileSystem> file_system_;
  // The manifest used by resource loading and llc's reverse service to look up
  // objects and libraries.
  nacl::scoped_ptr<const Manifest> manifest_;
  // TEMPORARY: ld needs to look up dynamic libraries in the nexe's manifest
  // until metadata is complete in pexes.  This manifest lookup allows looking
  // for whether a resource requested by ld is in the nexe manifest first, and
  // if not, then consults the extension manifest.
  // TODO(sehr,jvoung,pdox): remove this when metadata is correct.
  // The manifest used by ld's reverse service to look up objects and libraries.
  nacl::scoped_ptr<const Manifest> ld_manifest_;
  // An auxiliary class that manages downloaded resources (llc and ld nexes).
  nacl::scoped_ptr<PnaclResources> resources_;

  // State used for querying the temporary directory.
  nacl::scoped_ptr<pp::FileRef> dir_ref_;
  nacl::scoped_ptr<pp::FileIO> dir_io_;
  PP_FileInfo dir_info_;

  // The URL for the pexe file.
  nacl::string pexe_url_;
  // Optional cache identity for translation caching.
  nacl::string cache_identity_;
  // Borrowed reference which must outlive the thread.
  nacl::scoped_ptr<nacl::DescWrapper> pexe_wrapper_;
  // Object file, produced by the translator and consumed by the linker.
  nacl::scoped_ptr<LocalTempFile> obj_file_;
  // Translated nexe file, produced by the linker and consumed by sel_ldr.
  nacl::scoped_ptr<LocalTempFile> nexe_file_;

  // Used to report information when errors (PPAPI or otherwise) are reported.
  ErrorInfo error_info_;
  // True if an error was already reported, and translate_notify_callback_
  // was already run/consumed.
  bool error_already_reported_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
