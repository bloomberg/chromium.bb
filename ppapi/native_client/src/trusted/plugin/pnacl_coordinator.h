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

#include "ppapi/cpp/completion_callback.h"

struct NaClMutex;

namespace plugin {

class Plugin;
class PnaclCoordinator;
class PnaclResources;

class PnaclResources {
 public:
  PnaclResources(Plugin* plugin)
      : plugin_(plugin),
        llc_desc_(kNaClSrpcInvalidImcDesc),
        ld_desc_(kNaClSrpcInvalidImcDesc) { }

  ~PnaclResources() {
    for (std::map<nacl::string, nacl::DescWrapper*>::iterator
             i = resource_wrappers_.begin(), e = resource_wrappers_.end();
         i != e;
         ++i) {
      delete i->second;
    }
    resource_wrappers_.clear();
  }

  // The file descriptor for the llc nexe.
  NaClSrpcImcDescType llc_desc() const { return llc_desc_; }
  void set_llc_desc(NaClSrpcImcDescType llc_desc) { llc_desc_ = llc_desc; }
  // The file descriptor for the ld nexe.
  NaClSrpcImcDescType ld_desc() const { return ld_desc_; }
  void set_ld_desc(NaClSrpcImcDescType ld_desc) { ld_desc_ = ld_desc; }

  NaClSrpcImcDescType DescForUrl(const nacl::string& url) {
    return resource_wrappers_[url]->desc();
  }
  void AddFDForUrl(const nacl::string& url, int32_t fd);

 private:
  Plugin* plugin_;
  NaClSrpcImcDescType llc_desc_;
  NaClSrpcImcDescType ld_desc_;
  std::map<nacl::string, nacl::DescWrapper*> resource_wrappers_;
};

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

  // Accessors for use by helper threads.
  Plugin* plugin() const { return plugin_; }
  NaClSubprocess* llc_subprocess() const { return llc_subprocess_; }
  NaClSubprocess* ld_subprocess() const { return ld_subprocess_; }
  bool SubprocessesShouldDie() {
    NaClXMutexLock(&subprocess_mu_);
    bool retval =  subprocesses_should_die_;
    NaClXMutexUnlock(&subprocess_mu_);
    return retval;
  }
  void SetSubprocessesShouldDie(bool subprocesses_should_die) {
    NaClXMutexLock(&subprocess_mu_);
    subprocesses_should_die_ = subprocesses_should_die;
    NaClXMutexUnlock(&subprocess_mu_);
  }
  PnaclResources* resources() const { return resources_.get(); }

 protected:

  // Delay a callback until |num_dependencies| are met.
  DelayedCallback* MakeDelayedCallback(pp::CompletionCallback cb,
                                       uint32_t num_dependencies);

  // Helper functions for generating callbacks that will be run when a
  // download of |url| has completed. The generated callback will
  // run |handler|. The |handler| itself is given a pp_error code,
  // the url of the download, and another callback in the
  // form of the supplied |delayed_callback|.
  void AddDownloadToDelayedCallback(
      void (PnaclCoordinator::*handler)(int32_t,
                                        const nacl::string&,
                                        DelayedCallback*),
      DelayedCallback* delayed_callback,
      const nacl::string& url,
      std::vector<url_callback_pair>& queue);
  void AddDownloadToDelayedCallback(
      void (PnaclCoordinator::*handler)(int32_t,
                                        const nacl::string&,
                                        PnaclTranslationUnit*,
                                        DelayedCallback*),
      DelayedCallback* delayed_callback,
      const nacl::string& url,
      PnaclTranslationUnit* translation_unit,
      std::vector<url_callback_pair>& queue);

  bool ScheduleDownload(const nacl::string& url,
                        const pp::CompletionCallback& cb);

  // Callbacks for when various files, etc. have been downloaded.
  void PexeReady(int32_t pp_error,
                 const nacl::string& url,
                 PnaclTranslationUnit* translation_unit,
                 DelayedCallback* delayed_callback);
  void LLCReady(int32_t pp_error,
                const nacl::string& url,
                DelayedCallback* delayed_callback);
  void LDReady(int32_t pp_error,
               const nacl::string& url,
               DelayedCallback* delayed_callback);
  void LinkResourceReady(int32_t pp_error,
                         const nacl::string& url,
                         DelayedCallback* delayed_callback);

  int32_t GetLoadedFileDesc(int32_t pp_error,
                            const nacl::string& url,
                            const nacl::string& component);

  // Helper for starting helper nexes after they are downloaded.
  NaClSubprocessId HelperNexeDidLoad(int32_t fd, ErrorInfo* error_info);

  // Callbacks for compute-based translation steps.
  void RunTranslate(int32_t pp_error,
                    PnaclTranslationUnit* translation_unit,
                    DelayedCallback* delayed_callback);
  void RunTranslateDidFinish(int32_t pp_error,
                             PnaclTranslationUnit* translation_unit,
                             DelayedCallback* delayed_callback);
  void RunLink(int32_t pp_error, PnaclTranslationUnit* translation_unit);
  void RunLinkDidFinish(int32_t pp_error,
                        PnaclTranslationUnit* translation_unit);

  // Pnacl translation completed normally.
  void PnaclDidFinish(int32_t pp_error, PnaclTranslationUnit* translation_unit);

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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclCoordinator);

  Plugin* plugin_;
  pp::CompletionCallback translate_notify_callback_;
  pp::CompletionCallbackFactory<PnaclCoordinator> callback_factory_;

  // State for a single translation.
  // TODO(jvoung): see if we can manage this state better, especially when we
  // start having to translate multiple bitcode files for the same application
  // (for DSOs).

  std::set<DelayedCallback*> delayed_callbacks;

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
