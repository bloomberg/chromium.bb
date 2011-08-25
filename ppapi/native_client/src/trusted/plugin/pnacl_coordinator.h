// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_

#include <set>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/delayed_callback.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_thread_args.h"

#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class Plugin;
struct DoTranslateArgs;
struct DoLinkArgs;

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
    : instance_(NULL),
      translate_notify_callback_(pp::BlockUntilComplete()),
      llc_subprocess_(NULL),
      ld_subprocess_(NULL),
      obj_len_(-1),
      translate_args_(NULL),
      link_args_(NULL) {
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

  // This method should really be private, but it is used to
  // communicate with the linker thread.
  NaClSrpcImcDescType GetLinkerResourceFD(const nacl::string& url) {
    return linker_resource_fds_[url]->desc();
  }

 protected:

  void SetObjectFile(NaClSrpcImcDescType fd, int32_t len);
  void SetTranslatedFile(NaClSrpcImcDescType fd);

  // Delay a callback until |num_dependencies| are met.
  DelayedCallback* MakeDelayedCallback(pp::CompletionCallback cb,
                                       uint32_t num_dependencies);

  // Helper function for generating callbacks that will be run when a
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

  bool ScheduleDownload(const nacl::string& url,
                        const pp::CompletionCallback& cb);

  // Callbacks for when various files, etc. have been downloaded.
  void PexeReady(int32_t pp_error,
                 const nacl::string& url,
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
  void RunTranslate(int32_t pp_error, DelayedCallback* delayed_callback);
  void RunTranslateDidFinish(int32_t pp_error,
                             DelayedCallback* delayed_callback);
  void RunLink(int32_t pp_error);
  void RunLinkDidFinish(int32_t pp_error);

  // Pnacl translation completed normally.
  void PnaclDidFinish(int32_t pp_error);

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

  Plugin* instance_;
  pp::CompletionCallback translate_notify_callback_;
  pp::CompletionCallbackFactory<PnaclCoordinator> callback_factory_;

  // State for a single translation.
  // TODO(jvoung): see if we can manage this state better, especially when we
  // start having to translate multiple bitcode files for the same application
  // (for DSOs).

  std::set<DelayedCallback*> delayed_callbacks;

  // Helper subprocess loaded by the plugin (deleted by the plugin).
  // We may want to do cleanup ourselves when we are in the
  // business of compiling multiple bitcode objects / libraries, and
  // if we truly cannot reuse existing loaded subprocesses.
  NaClSubprocess* llc_subprocess_;
  NaClSubprocess* ld_subprocess_;

  // Bitcode file pulled from the Net.
  nacl::scoped_ptr<nacl::DescWrapper> pexe_fd_;

  // Object "file" obtained after compiling with LLVM, along with the length
  // of the "file".
  nacl::scoped_ptr<nacl::DescWrapper> obj_fd_;
  int32_t obj_len_;

  // Nexe from the final native Link.
  nacl::scoped_ptr<nacl::DescWrapper> translated_fd_;

  // Perhaps make this a single thread that invokes (S)RPCs followed by
  // callbacks based on a Queue of requests. A generic mechanism would make
  // it easier to add steps later (the mechanism could look like PostMessage?).
  nacl::scoped_ptr<DoTranslateArgs> translate_args_;
  nacl::scoped_ptr<NaClThread> translate_thread_;

  nacl::scoped_ptr<DoLinkArgs> link_args_;
  nacl::scoped_ptr<NaClThread> link_thread_;

  std::map<nacl::string, nacl::DescWrapper*> linker_resource_fds_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
