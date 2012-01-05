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
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_rng.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/delayed_callback.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"

struct NaClMutex;

namespace plugin {

class Manifest;
class Plugin;
class PnaclCoordinator;

// Translation creates two temporary files.  The first temporary file holds
// the object file created by llc.  The second holds the nexe produced by
// the linker.  Both of these temporary files are used to both write and
// read according to the following matrix:
//
// PnaclCoordinator::obj_file_:
//     written by: llc     (passed in explicitly through SRPC)
//     read by:    ld      (returned via lookup service from SRPC)
// PnaclCoordinator::nexe_file_:
//     written by: lc      (passed in explicitly through SRPC)
//     read by:    sel_ldr (passed in explicitly to command channel)
//

// PnaclFileDescPair represents a file used as a temporary between stages in
// translation.  It is created in the local temporary file system of the page
// being processed.  The name of the temporary file is a random 32-character
// hex string.  Because both reading and writing are necessary, two I/O objects
// for the file are opened.
class PnaclFileDescPair {
 public:
  PnaclFileDescPair(Plugin* plugin,
                    pp::FileSystem* file_system,
                    PnaclCoordinator* coordinator);
  ~PnaclFileDescPair();
  // Opens a pair of file IO objects referring to a randomly named file in
  // file_system_.  One IO is for writing the file and another for reading it.
  void Open(const pp::CompletionCallback& cb);
  // Accessors.
  // The nacl::DescWrapper* for the writeable version of the file.
  nacl::DescWrapper* write_wrapper() { return write_wrapper_.get(); }
  // The nacl::DescWrapper* for the read-only version of the file.
  nacl::DescWrapper* read_wrapper() { return read_wrapper_.get(); }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclFileDescPair);

  // Gets the POSIX file descriptor for a resource.
  int32_t GetFD(int32_t pp_error,
                const pp::Resource& resource,
                bool is_writable);
  // Called when the writable file IO was opened.
  void WriteFileDidOpen(int32_t pp_error);
  // Called when the readable file IO was opened.
  void ReadFileDidOpen(int32_t pp_error);

  Plugin* plugin_;
  pp::FileSystem* file_system_;
  PnaclCoordinator* coordinator_;
  const PPB_FileIOTrusted* file_io_trusted_;
  pp::CompletionCallbackFactory<PnaclFileDescPair> callback_factory_;
  nacl::string filename_;
  // The PPAPI and wrapper state for the writeable file.
  nacl::scoped_ptr<pp::FileRef> write_ref_;
  nacl::scoped_ptr<pp::FileIO> write_io_;
  nacl::scoped_ptr<nacl::DescWrapper> write_wrapper_;
  // The PPAPI and wrapper state for the read-only file.
  nacl::scoped_ptr<pp::FileRef> read_ref_;
  nacl::scoped_ptr<pp::FileIO> read_io_;
  nacl::scoped_ptr<nacl::DescWrapper> read_wrapper_;
  // The callback invoked when both file I/O objects are created.
  pp::CompletionCallback done_callback_;
  // Random number generator used to create filenames.
  struct NaClDescRng rng_desc_;
};

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
// OPEN_TMP_FOP_LLC_TO_LD_COMMUNICATION
//     Complete when ObjectPairDidOpen is invoked.
// OPEN_TMP_FOR_LD_TO_SEL_LDR_COMMUNICATION
//     Complete when NexePairDidOpen is invoked.
// PREPARE_PEXE_FOR_STREAMING
//     Complete when RunTranslate is invoked.
// START_LD_AND_LLC_SUBPROCESS_AND_INITIATE_TRANSLATION
//     Complete when RunTranslate returns.
// TRANSLATION_COMPLETE
//     Complete when TranslateFinished is invoked.
//
// It should be noted that at the moment we are not properly freeing the
// PPAPI resources used for the temporary files used in translation. Until
// that is fixed, (4) and (5) should be done in that order.
// TODO(sehr): Fix freeing of temporary files.
class PnaclCoordinator {
 public:
  virtual ~PnaclCoordinator();

  // The factory method for translations.
  static PnaclCoordinator* BitcodeToNative(
      Plugin* plugin,
      const nacl::string& pexe_url,
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
                   const pp::CompletionCallback& translate_notify_callback);

  // Callback for when llc and ld have been downloaded.
  // This is the first callback invoked in response to BitcodeToNative.
  void ResourcesDidLoad(int32_t pp_error);

  // Callbacks for temporary file related stages.
  // They are invoked from ResourcesDidLoad and proceed in declaration order.
  // Invoked when the temporary file system is successfully opened in PPAPI.
  void FileSystemDidOpen(int32_t pp_error);
  // Invoked when the obj_file_ temporary file I/O pair is created.
  void ObjectPairDidOpen(int32_t pp_error);
  // Invoked when the nexe_file_ temporary file I/O pair is created.
  void NexePairDidOpen(int32_t pp_error);

  // Once llc and ld nexes have been loaded and the two temporary files have
  // been created, this starts the translation.  Translation starts two
  // subprocesses, one for llc and one for ld.
  void RunTranslate(int32_t pp_error);
  // Starts an individual llc or ld subprocess used for translation.
  NaClSubprocess* StartSubprocess(const nacl::string& url,
                                  const Manifest* manifest);
  // PnaclCoordinator creates a helper thread to allow translations to be
  // invoked via SRPC.  This is the helper thread function for translation.
  static void WINAPI DoTranslateThread(void* arg);
  // Returns true if a the translate thread and subprocesses should stop.
  bool SubprocessesShouldDie();
  // Signal the translate thread and subprocesses that they should stop.
  void SetSubprocessesShouldDie(bool subprocesses_should_die);
  // Signal that Pnacl translation completed normally.
  void TranslateFinished(int32_t pp_error);
  // Signal that Pnacl translation failed, from the translation thread only.
  void TranslateFailed(const nacl::string& error_string);

  // The plugin owning the nexe for which we are doing translation.
  Plugin* plugin_;

  pp::CompletionCallback translate_notify_callback_;
  // PnaclRefCount is only needed to support file lookups.
  // TODO(sehr): remove this when file lookup is through ReverseService.
  pp::CompletionCallbackFactory<PnaclCoordinator,
                                PnaclRefCount> callback_factory_;

  // Helper subprocesses loaded by the plugin (deleted by the plugin).
  // A nacl sandbox running the llc nexe.
  NaClSubprocess* llc_subprocess_;
  // A nacl sandbox running the ld nexe.
  NaClSubprocess* ld_subprocess_;
  // True if the translation thread and subprocesses should exit.
  bool subprocesses_should_die_;
  // Used to guard and publish subprocesses_should_die_.
  struct NaClMutex subprocess_mu_;

  // Nexe from the final native Link.
  nacl::scoped_ptr<nacl::DescWrapper> translated_fd_;

  // The helper thread used to do translations via SRPC.
  nacl::scoped_ptr<NaClThread> translate_thread_;
  // Translation creates local temporary files.
  nacl::scoped_ptr<pp::FileSystem> file_system_;
  // The manifest used by the reverse service to look up objects and libraries.
  const Manifest* manifest_;
  // An auxiliary class that manages downloaded resources (llc and ld nexes).
  nacl::scoped_ptr<PnaclResources> resources_;

  // The URL for the pexe file.
  nacl::string pexe_url_;
  // Borrowed reference which must outlive the thread.
  nacl::scoped_ptr<nacl::DescWrapper> pexe_wrapper_;
  // Object file, produced by the translator and consumed by the linker.
  nacl::scoped_ptr<PnaclFileDescPair> obj_file_;
  // Translated nexe file, produced by the linker and consumed by sel_ldr.
  nacl::scoped_ptr<PnaclFileDescPair> nexe_file_;
  // Callbacks to run when tasks or completed or an error has occurred.
  pp::CompletionCallback translate_done_cb_;

  // Used to report information when errors (PPAPI or otherwise) are reported.
  ErrorInfo error_info_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
