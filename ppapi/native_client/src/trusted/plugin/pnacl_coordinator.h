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

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"

#include "ppapi/native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/native_client/src/trusted/plugin/file_downloader.h"
#include "ppapi/native_client/src/trusted/plugin/local_temp_file.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_subprocess.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_options.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"


namespace plugin {

class Manifest;
class Plugin;
class PnaclCoordinator;
class PnaclTranslateThread;
class TempFile;

// A class invoked by Plugin to handle PNaCl client-side translation.
// Usage:
// (1) Invoke the factory method, e.g.,
//     PnaclCoordinator* coord = BitcodeToNative(plugin,
//                                               "http://foo.com/my.pexe",
//                                               pnacl_options,
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
// OPEN_BITCODE_STREAM
//       Complete when BitcodeStreamDidOpen is invoked
// GET_NEXE_FD
//       Get an FD which contains the cached nexe, or is writeable for
//       translation output. Complete when NexeFdDidOpen is called.
//
// If there was a cache hit, go to OPEN_NEXE_FOR_SEL_LDR, otherwise,
// continue streaming the bitcode, and:
// OPEN_TMP_FOR_LLC_TO_LD_COMMUNICATION
//     Complete when ObjectFileDidOpen is invoked.
// OPEN_NEXE_FD_FOR_WRITING
//     Complete when RunTranslate is invoked.
// START_LD_AND_LLC_SUBPROCESS_AND_INITIATE_TRANSLATION
//     Complete when RunTranslate returns.
// TRANSLATION_COMPLETE
//     Complete when TranslateFinished is invoked.
//
// OPEN_NEXE_FOR_SEL_LDR
//   Complete when NexeReadDidOpen is invoked.
class PnaclCoordinator: public CallbackSource<FileStreamData> {
 public:
  virtual ~PnaclCoordinator();

  // The factory method for translations.
  static PnaclCoordinator* BitcodeToNative(
      Plugin* plugin,
      const nacl::string& pexe_url,
      const PnaclOptions& pnacl_options,
      const pp::CompletionCallback& translate_notify_callback);

  // Call this to take ownership of the FD of the translated nexe after
  // BitcodeToNative has completed (and the finish_callback called).
  nacl::DescWrapper* ReleaseTranslatedFD() { return translated_fd_.release(); }

  // Run |translate_notify_callback_| with an error condition that is not
  // PPAPI specific.  Also set ErrorInfo report.
  void ReportNonPpapiError(PluginErrorCode err, const nacl::string& message);
  // Run when faced with a PPAPI error condition. Bring control back to the
  // plugin by invoking the |translate_notify_callback_|.
  // Also set ErrorInfo report.
  void ReportPpapiError(PluginErrorCode err,
                        int32_t pp_error, const nacl::string& message);
  // Bring control back to the plugin by invoking the
  // |translate_notify_callback_|.  This does not set the ErrorInfo report,
  // it is assumed that it was already set.
  void ExitWithError();

  // Implement FileDownloader's template of the CallbackSource interface.
  // This method returns a callback which will be called by the FileDownloader
  // to stream the bitcode data as it arrives. The callback
  // (BitcodeStreamGotData) passes it to llc over SRPC.
  StreamCallback GetCallback();

  // Return a callback that should be notified when |bytes_compiled| bytes
  // have been compiled.
  pp::CompletionCallback GetCompileProgressCallback(int64_t bytes_compiled);

  // Get the last known load progress.
  void GetCurrentProgress(int64_t* bytes_loaded, int64_t* bytes_total);

  // Return true if the total progress to report (w/ progress events) is known.
  bool ExpectedProgressKnown() { return expected_pexe_size_ != -1; }

  // Return true if we should delay the progress event reporting.
  // This delay approximates:
  // - the size of the buffer of bytes sent but not-yet-compiled by LLC.
  // - the linking time.
  bool ShouldDelayProgressEvent() {
    const uint32_t kProgressEventSlopPct = 5;
    return ((expected_pexe_size_ - pexe_bytes_compiled_) * 100 /
            expected_pexe_size_) < kProgressEventSlopPct;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclCoordinator);

  // BitcodeToNative is the factory method for PnaclCoordinators.
  // Therefore the constructor is private.
  PnaclCoordinator(Plugin* plugin,
                   const nacl::string& pexe_url,
                   const PnaclOptions& pnacl_options,
                   const pp::CompletionCallback& translate_notify_callback);

  // Callback for when we know PNaCl is installed.
  void DidCheckPnaclInstalled(int32_t pp_error);

  // Callback for when the resource info JSON file has been read.
  void ResourceInfoWasRead(int32_t pp_error);

  // Callback for when llc and ld have been downloaded.
  void ResourcesDidLoad(int32_t pp_error);

  // Callbacks for temporary file related stages.
  // They are invoked from ResourcesDidLoad and proceed in declaration order.
  // Invoked when the temporary file system is successfully opened in PPAPI.
  void FileSystemDidOpen(int32_t pp_error);
  // Invoked after we are sure the PNaCl temporary directory exists.
  void DirectoryWasCreated(int32_t pp_error);
  // Invoke to issue a GET request for bitcode.
  void OpenBitcodeStream();
  // Invoked when we've started an URL fetch for the pexe to check for
  // caching metadata.
  void BitcodeStreamDidOpen(int32_t pp_error);
  // Invoked when we've gotten a temp FD for the nexe, either with the nexe
  // data, or a writeable fd to save to.
  void NexeFdDidOpen(int32_t pp_error);
  // Invoked after we have checked the PNaCl cache for a translated version.
  void CachedFileDidOpen(int32_t pp_error);
  // Invoked when a pexe data chunk arrives (when using streaming translation)
  void BitcodeStreamGotData(int32_t pp_error, FileStreamData data);
  // Invoked when a pexe data chunk is compiled.
  void BitcodeGotCompiled(int32_t pp_error, int64_t bytes_compiled);
  // Invoked when the pexe download finishes (using streaming translation)
  void BitcodeStreamDidFinish(int32_t pp_error);
  // Invoked when the write descriptor for obj_file_ is created.
  void ObjectFileDidOpen(int32_t pp_error);
  // Once llc and ld nexes have been loaded and the two temporary files have
  // been created, this starts the translation.  Translation starts two
  // subprocesses, one for llc and one for ld.
  void RunTranslate(int32_t pp_error);

  // Invoked when translation is finished.
  void TranslateFinished(int32_t pp_error);

  // If the cache is enabled, open a cache file for write, then copy
  // the nexe data from temp_nexe_file_ to> cached_nexe_file_.
  // Once the copy is done, we commit it to the cache by renaming the
  // cache file to the final name.
  void CachedNexeOpenedForWrite(int32_t pp_error);
  void DidCopyNexeToCachePartial(int32_t pp_error, int32_t num_read_prev,
                                 int64_t cur_offset);
  void NexeWasCopiedToCache(int32_t pp_error);
  // If the copy of the nexe to the not-yet-committed-to-cache file
  // failed after partial writes, we attempt to delete the partially written
  // file. This callback is invoked when the delete is completed.
  void CorruptCacheFileWasDeleted(int32_t delete_pp_error,
                                  int32_t orig_pp_error);
  // Invoked when the nexe_file_ temporary has been renamed to the nexe name.
  void NexeFileWasRenamed(int32_t pp_error);
  // Invoked when the read descriptor for nexe_file_ is created.
  void NexeReadDidOpen(int32_t pp_error);

  // Keeps track of the pp_error upon entry to TranslateFinished,
  // for inspection after cleanup.
  int32_t translate_finish_error_;

  // The plugin owning the nexe for which we are doing translation.
  Plugin* plugin_;

  pp::CompletionCallback translate_notify_callback_;
  // Threadsafety is required to support file lookups.
  pp::CompletionCallbackFactory<PnaclCoordinator,
                                pp::ThreadSafeThreadTraits> callback_factory_;

  // Nexe from the final native Link.
  nacl::scoped_ptr<nacl::DescWrapper> translated_fd_;

  // Translation creates local temporary files.
  nacl::scoped_ptr<pp::FileSystem> file_system_;
  // The manifest used by resource loading and ld + llc's reverse service
  // to look up objects and libraries.
  nacl::scoped_ptr<const Manifest> manifest_;
  // An auxiliary class that manages downloaded resources (llc and ld nexes).
  nacl::scoped_ptr<PnaclResources> resources_;

  // State used for querying the temporary directory.
  nacl::scoped_ptr<pp::FileRef> dir_ref_;

  // The URL for the pexe file.
  nacl::string pexe_url_;
  // Options for translation.
  PnaclOptions pnacl_options_;

  // Object file, produced by the translator and consumed by the linker.
  nacl::scoped_ptr<TempFile> obj_file_;
  // Translated nexe file, produced by the linker.
  nacl::scoped_ptr<TempFile> temp_nexe_file_;
  // Cached nexe file, consumed by sel_ldr.  This will be NULL if we do
  // not have a writeable cache file.  That is currently the case when
  // off_the_record_ is true.
  nacl::scoped_ptr<LocalTempFile> cached_nexe_file_;
  // True if the new cache flow is enabled. Currently set by an environment
  // variable on construction. TODO(dschuff): remove old cache stuff.
  bool use_new_cache_;
  // Passed to the browser, which sets it to true if there is a translation
  // cache hit.
  PP_Bool is_cache_hit_;
  // Passed to the browser, which sets it to the handle for the nexe file
  // (either the translated nexe from the cache, or a temp file to write to).
  PP_FileHandle nexe_handle_;

  // Downloader for streaming translation
  nacl::scoped_ptr<FileDownloader> streaming_downloader_;

  // Used to report information when errors (PPAPI or otherwise) are reported.
  ErrorInfo error_info_;

  // True if an error was already reported, and translate_notify_callback_
  // was already run/consumed.
  bool error_already_reported_;

  // True if compilation is off_the_record.
  bool off_the_record_;

  // State for timing and size information for UMA stats.
  int64_t pnacl_init_time_;
  int64_t pexe_size_;  // Count as we stream -- will converge to pexe size.
  int64_t pexe_bytes_compiled_;  // Count as we compile.
  int64_t expected_pexe_size_;   // Expected download total (-1 if unknown).

  // The helper thread used to do translations via SRPC.
  // Keep this last in declaration order to ensure the other variables
  // haven't been destroyed yet when its destructor runs.
  nacl::scoped_ptr<PnaclTranslateThread> translate_thread_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
