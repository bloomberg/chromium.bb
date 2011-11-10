// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/delayed_callback.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"

#include "ppapi/cpp/completion_callback.h"

struct NaClMutex;

namespace plugin {

class Plugin;
class PnaclCoordinator;

struct PnaclTranslationUnit {
  PnaclTranslationUnit(PnaclCoordinator* coord)
    : coordinator(coord),
      obj_len(-1),
      is_shared_library(false),
      soname(""),
      lib_dependencies("") {
  }
  // Punch hole in abstraction.
  PnaclCoordinator* coordinator;

  // Borrowed reference which must outlive the thread.
  nacl::scoped_ptr<nacl::DescWrapper> pexe_wrapper;

  // Object file produced by translator and consumed by the linker.
  nacl::scoped_ptr<nacl::DescWrapper> obj_wrapper;
  int32_t obj_len;

  // Information extracted from the pexe that is needed by the linker.
  bool is_shared_library;
  nacl::string soname;
  nacl::string lib_dependencies;

  // The translated user nexe file.
  nacl::scoped_ptr<nacl::DescWrapper> nexe_wrapper;

  // Callbacks to run when tasks or completed or an error has occurred.
  pp::CompletionCallback translate_done_cb;
  pp::CompletionCallback link_done_cb;

  ErrorInfo error_info;
};


typedef std::pair<nacl::string, pp::CompletionCallback> url_callback_pair;

// A class that handles PNaCl client-side translation.
// Usage:
// (1) Initialize();
// (2) BitcodeToNative(bitcode, ..., finish_callback);
// (3) After finish_callback runs, do:
//       fd = ReleaseTranslatedFD();
// (4) go ahead and load the nexe from "fd"
// (5) delete
class PnaclCoordinator {
 public:
  PnaclCoordinator()
    : plugin_(NULL),
      translate_notify_callback_(pp::BlockUntilComplete()),
      llc_subprocess_(NULL),
      ld_subprocess_(NULL),
      subprocesses_should_die_(false) {
        NaClXMutexCtor(&subprocess_mu_);
  }

  virtual ~PnaclCoordinator();

  // Initialize() can only be called once during the lifetime of this instance.
  void Initialize(Plugin* instance);

  void BitcodeToNative(const nacl::string& pexe_url,
                       const nacl::string& llc_url,
                       const nacl::string& ld_url,
                       const pp::CompletionCallback& finish_callback);

  // Call this to take ownership of the FD of the translated nexe after
  // BitcodeToNative has completed (and the finish_callback called).
  nacl::DescWrapper* ReleaseTranslatedFD() {
    return translated_fd_.release();
  }

  int32_t GetLoadedFileDesc(int32_t pp_error,
                            const nacl::string& url,
                            const nacl::string& component);

  // Run when faced with a PPAPI error condition. It brings control back to the
  // plugin by invoking the |translate_notify_callback_|.
  void PnaclPpapiError(int32_t pp_error);
  // Run |translate_notify_callback_| with an error condition that is not
  // PPAPI specific.
  void PnaclNonPpapiError();
  // Wrapper for Plugin ReportLoadAbort.
  void ReportLoadAbort();
  // Wrapper for Plugin ReportLoadError.
  void ReportLoadError(const ErrorInfo& error);



  // Accessors for use by helper threads.
  Plugin* plugin() const { return plugin_; }
  nacl::string llc_url() const { return llc_url_; }
  NaClSubprocess* llc_subprocess() const { return llc_subprocess_; }
  bool StartLlcSubProcess();
  nacl::string ld_url() const { return ld_url_; }
  NaClSubprocess* ld_subprocess() const { return ld_subprocess_; }
  bool StartLdSubProcess();
  bool SubprocessesShouldDie();
  void SetSubprocessesShouldDie(bool subprocesses_should_die);
  PnaclResources* resources() const { return resources_.get(); }

 protected:

  // Callbacks for when various files, etc. have been downloaded.
  void ResourcesDidLoad(int32_t pp_error,
                        const nacl::string& url,
                        PnaclTranslationUnit* translation_unit);

  // Callbacks for compute-based translation steps.
  void RunTranslate(int32_t pp_error,
                    const nacl::string& url,
                    PnaclTranslationUnit* translation_unit);
  void RunLink(int32_t pp_error, PnaclTranslationUnit* translation_unit);

  // Pnacl translation completed normally.
  void PnaclDidFinish(int32_t pp_error, PnaclTranslationUnit* translation_unit);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclCoordinator);

  Plugin* plugin_;
  pp::CompletionCallback translate_notify_callback_;
  pp::CompletionCallbackFactory<PnaclCoordinator> callback_factory_;

  // URLs used to lookup downloaded resources.
  nacl::string llc_url_;
  nacl::string ld_url_;

  // Helper subprocesses loaded by the plugin (deleted by the plugin).
  // We may want to do cleanup ourselves when we are in the
  // business of compiling multiple bitcode objects / libraries, and
  // if we truly cannot reuse existing loaded subprocesses.
  NaClSubprocess* llc_subprocess_;
  NaClSubprocess* ld_subprocess_;
  bool subprocesses_should_die_;
  struct NaClMutex subprocess_mu_;

  // Nexe from the final native Link.
  nacl::scoped_ptr<nacl::DescWrapper> translated_fd_;

  // Perhaps make this a single thread that invokes (S)RPCs followed by
  // callbacks based on a Queue of requests. A generic mechanism would make
  // it easier to add steps later (the mechanism could look like PostMessage?).
  nacl::scoped_ptr<PnaclTranslationUnit> translation_unit_;
  nacl::scoped_ptr<NaClThread> translate_thread_;
  nacl::scoped_ptr<NaClThread> link_thread_;

  // An auxiliary class that manages downloaded resources.
  nacl::scoped_ptr<PnaclResources> resources_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
