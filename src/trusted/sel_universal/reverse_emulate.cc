// Copyright (c) 2012 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "native_client/src/trusted/sel_universal/reverse_emulate.h"
#include <stdio.h>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"
#include "native_client/src/trusted/validator/nacl_file_info.h"


// Mock of ReverseInterface for use by nexes.
class ReverseEmulate : public nacl::ReverseInterface {
 public:
  ReverseEmulate(
      nacl::SelLdrLauncherStandaloneFactory* factory,
      const vector<nacl::string>& prefix,
      const vector<nacl::string>& sel_ldr_argv);
  virtual ~ReverseEmulate();

  // debugging, messaging
  virtual void Log(nacl::string message);

  // Startup handshake
  virtual void StartupInitializationComplete();

  // Name service use.
  virtual bool EnumerateManifestKeys(std::set<nacl::string>* keys);
  virtual bool OpenManifestEntry(nacl::string url_key,
                                 struct NaClFileInfo* info);
  virtual bool CloseManifestEntry(int32_t desc);
  virtual void ReportCrash();

  // The low-order 8 bits of the |exit_status| should be reported to
  // any interested parties.
  virtual void ReportExitStatus(int exit_status);

  // Send a string as a PostMessage to the browser.
  virtual void DoPostMessage(nacl::string message);

  // Create new service runtime process and return secure command
  // channel and untrusted application channel socket addresses.
  virtual int CreateProcess(nacl::DescWrapper** out_sock_addr,
                            nacl::DescWrapper** out_app_addr);

  virtual void CreateProcessFunctorResult(
      nacl::CreateProcessFunctorInterface* functor);

  virtual void FinalizeProcess(int32_t pid);

  // Request quota for a write to a file.
  virtual int64_t RequestQuotaForWrite(nacl::string file_id,
                                       int64_t offset,
                                       int64_t length);

  // covariant impl of Ref()
  ReverseEmulate* Ref() {  // down_cast
    return reinterpret_cast<ReverseEmulate*>(RefCountBase::Ref());
  }

 private:
  int32_t ReserveProcessSlot();
  void SaveToProcessSlot(int32_t pid,
                         nacl::SelLdrLauncherStandalone *launcher);
  bool FreeProcessSlot(int32_t pid);

  NaClMutex mu_;
  std::vector<std::pair<bool, nacl::SelLdrLauncherStandalone*> > subprocesses_;

  nacl::SelLdrLauncherStandaloneFactory* factory_;

  std::vector<nacl::string> prefix_;
  std::vector<nacl::string> sel_ldr_argv_;

  NACL_DISALLOW_COPY_AND_ASSIGN(ReverseEmulate);
};

namespace {

typedef std::map<nacl::string, string> KeyToFileMap;

KeyToFileMap g_key_to_file;

nacl::scoped_ptr_refcount<nacl::ReverseService> g_reverse_service;

/*
 * TODO(phosek): These variables should be instance variables of Reverse
 * Emulate. However, we cannot make them such at the moment because they're
 * also being used by command handlers. This will require more significant
 * redesign/refactoring.
 */
int g_exited;
NaClMutex g_exit_mu;
NaClCondVar g_exit_cv;

}  // end namespace


bool ReverseEmulateInit(NaClSrpcChannel* command_channel,
                        nacl::SelLdrLauncherStandalone* launcher,
                        nacl::SelLdrLauncherStandaloneFactory* factory,
                        const std::vector<nacl::string>& prefix,
                        const std::vector<nacl::string>& sel_ldr_argv) {
  // Do the SRPC to the command channel to set up the reverse channel.
  // This returns a NaClDesc* containing a socket address.
  NaClLog(1, "ReverseEmulateInit: launching reverse RPC service\n");
  NaClDesc* h;
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(command_channel, "reverse_setup::h", &h);
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    NaClLog(LOG_ERROR, "ReverseEmulateInit: reverse setup failed\n");
    return false;
  }
  // Make a nacl::DescWrapper* from the NaClDesc*
  nacl::scoped_ptr<nacl::DescWrapper> conn_cap(launcher->WrapCleanup(h));
  if (conn_cap == NULL) {
    NaClLog(LOG_ERROR, "ReverseEmulateInit: reverse desc wrap failed\n");
    return false;
  }
  // The implementation of the ReverseInterface is our emulator class.
  nacl::scoped_ptr<ReverseEmulate> reverse_interface(new ReverseEmulate(
        factory, prefix, sel_ldr_argv));
  // Construct locks guarding exit status.
  NaClXMutexCtor(&g_exit_mu);
  NaClXCondVarCtor(&g_exit_cv);
  g_exited = false;
  // Create an instance of ReverseService, which connects to the socket
  // address and exports the services from our emulator.
  g_reverse_service.reset(new nacl::ReverseService(conn_cap.get(),
                                                   reverse_interface->Ref()));
  if (g_reverse_service == NULL) {
    NaClLog(LOG_ERROR, "ReverseEmulateInit: reverse service ctor failed\n");
    return false;
  }
  // Successful creation of ReverseService took ownership of these.
  reverse_interface.release();
  conn_cap.release();
  // Starts the RPC handler for the reverse interface.
  if (!g_reverse_service->Start()) {
    NaClLog(LOG_ERROR, "ReverseEmulateInit: reverse service start failed\n");
    return false;
  }
  return true;
}

void ReverseEmulateFini() {
  CHECK(g_reverse_service != NULL);
  NaClLog(1, "Waiting for service threads to exit...\n");
  g_reverse_service->WaitForServiceThreadsToExit();
  NaClLog(1, "...service threads done\n");
  g_reverse_service.reset(NULL);
  NaClMutexDtor(&g_exit_mu);
  NaClCondVarDtor(&g_exit_cv);
}

bool HandlerReverseEmuAddManifestMapping(NaClCommandLoop* ncl,
                                         const std::vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }
  NaClLog(1, "HandlerReverseEmulateAddManifestMapping(%s) -> %s\n",
          args[1].c_str(), args[2].c_str());
  // Set the mapping for the key.
  g_key_to_file[args[1]] = args[2];
  return true;
}

bool HandlerReverseEmuDumpManifestMappings(NaClCommandLoop* ncl,
                                           const std::vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() != 1) {
    NaClLog(LOG_ERROR, "unexpected args\n");
    return false;
  }
  printf("ReverseEmulate manifest mappings:\n");
  for (KeyToFileMap::iterator i = g_key_to_file.begin();
       i != g_key_to_file.end();
       ++i) {
    printf("'%s': '%s'\n", i->first.c_str(), i->second.c_str());
  }
  return true;
}

bool HandlerWaitForExit(NaClCommandLoop* ncl,
                        const std::vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  UNREFERENCED_PARAMETER(args);

  nacl::MutexLocker take(&g_exit_mu);
  while (!g_exited) {
    NaClXCondVarWait(&g_exit_cv, &g_exit_mu);
  }

  return true;
}

ReverseEmulate::ReverseEmulate(
    nacl::SelLdrLauncherStandaloneFactory* factory,
    const vector<nacl::string>& prefix,
    const vector<nacl::string>& sel_ldr_argv)
  : factory_(factory),
    prefix_(prefix),
    sel_ldr_argv_(sel_ldr_argv) {
  NaClLog(1, "ReverseEmulate::ReverseEmulate\n");
  NaClXMutexCtor(&mu_);
}

ReverseEmulate::~ReverseEmulate() {
  NaClLog(1, "ReverseEmulate::~ReverseEmulate\n");
  for (size_t ix = 0; ix < subprocesses_.size(); ++ix) {
    if (subprocesses_[ix].first) {
      delete subprocesses_[ix].second;
      subprocesses_[ix].second = NULL;
      subprocesses_[ix].first = false;
    }
  }
  NaClMutexDtor(&mu_);
}

void ReverseEmulate::Log(nacl::string message) {
  NaClLog(1, "ReverseEmulate::Log (message=%s)\n", message.c_str());
}

void ReverseEmulate::StartupInitializationComplete() {
  NaClLog(1, "ReverseEmulate::StartupInitializationComplete ()\n");
}

bool ReverseEmulate::EnumerateManifestKeys(std::set<nacl::string>* keys) {
  NaClLog(1, "ReverseEmulate::StartupInitializationComplete ()\n");
  // Enumerate the keys.
  std::set<nacl::string> manifest_keys;
  for (KeyToFileMap::iterator i = g_key_to_file.begin();
       i != g_key_to_file.end();
       ++i) {
    manifest_keys.insert(i->first);
  }
  *keys = manifest_keys;
  return true;
}

bool ReverseEmulate::OpenManifestEntry(nacl::string url_key,
                                       struct NaClFileInfo* info) {
  NaClLog(1, "ReverseEmulate::OpenManifestEntry (url_key=%s)\n",
          url_key.c_str());
  memset(info, 0, sizeof(*info));
  info->desc = -1;
  // Find the pathname for the key.
  if (g_key_to_file.find(url_key) == g_key_to_file.end()) {
    NaClLog(1, "ReverseEmulate::OpenManifestEntry: no pathname for key.\n");
    return false;
  }
  nacl::string pathname = g_key_to_file[url_key];
  NaClLog(1, "ReverseEmulate::OpenManifestEntry: pathname is %s.\n",
          pathname.c_str());
  // TODO(ncbray): provide more information so that fast validation caching and
  // mmaping can be enabled.
  info->desc = OPEN(pathname.c_str(), O_RDONLY);
  return info->desc >= 0;
}

bool ReverseEmulate::CloseManifestEntry(int32_t desc) {
  NaClLog(1, "ReverseEmulate::CloseManifestEntry (desc=%d)\n", desc);
  CLOSE(desc);
  return true;
}

void ReverseEmulate::ReportCrash() {
  NaClLog(1, "ReverseEmulate::ReportCrash\n");
  nacl::MutexLocker take(&g_exit_mu);
  g_exited = true;
  NaClXCondVarBroadcast(&g_exit_cv);
}

void ReverseEmulate::ReportExitStatus(int exit_status) {
  NaClLog(1, "ReverseEmulate::ReportExitStatus (exit_status=%d)\n",
          exit_status);
  nacl::MutexLocker take(&g_exit_mu);
  g_exited = true;
  NaClXCondVarBroadcast(&g_exit_cv);
}

void ReverseEmulate::DoPostMessage(nacl::string message) {
  NaClLog(1, "ReverseEmulate::DoPostMessage (message=%s)\n", message.c_str());
}

class CreateProcessBinder : public nacl::CreateProcessFunctorInterface {
 public:
  CreateProcessBinder(nacl::DescWrapper** out_sock_addr,
                      nacl::DescWrapper** out_app_addr,
                      int32_t* out_pid)
      : sock_addr_(out_sock_addr)
      , app_addr_(out_app_addr)
      , pid_(out_pid) {}
  void Results(nacl::DescWrapper* res_sock_addr,
               nacl::DescWrapper* res_app_addr,
               int32_t pid) {
    if (pid >= 0) {
      *sock_addr_ = res_sock_addr;
      *app_addr_ = res_app_addr;
    } else {
      *sock_addr_ = NULL;
      *app_addr_ = NULL;
    }
    *pid_ = pid;
  }
 private:
  nacl::DescWrapper** sock_addr_;
  nacl::DescWrapper** app_addr_;
  int32_t* pid_;
};

// DEPRECATED
int ReverseEmulate::CreateProcess(nacl::DescWrapper** out_sock_addr,
                                  nacl::DescWrapper** out_app_addr) {
  NaClLog(1, "ReverseEmulate::CreateProcess)\n");

  int32_t pid;

  CreateProcessBinder binder(out_sock_addr, out_app_addr, &pid);
  CreateProcessFunctorResult(&binder);
  // race condition here, since we did not take a ref on *out_sock_addr etc
  // so until the response is sent, some other thread might unref it.

  return (pid < 0) ? pid : 0;
}

void ReverseEmulate::CreateProcessFunctorResult(
    nacl::CreateProcessFunctorInterface* functor) {
  NaClLog(1, "ReverseEmulate::CreateProcessFunctorResult)\n");
  // We are passing in empty list of application arguments as the real
  // arguments should be provided over the command channel.
  vector<nacl::string> app_argv;

  nacl::scoped_ptr<nacl::SelLdrLauncherStandalone> launcher(
      factory_->MakeSelLdrLauncherStandalone());
  if (!launcher->StartViaCommandLine(prefix_, sel_ldr_argv_, app_argv)) {
    NaClLog(LOG_FATAL,
            "ReverseEmulate::CreateProcess: failed to launch sel_ldr\n");
  }
  if (!launcher->ConnectBootstrapSocket()) {
    NaClLog(LOG_ERROR,
            "ReverseEmulate::CreateProcess:"
            " failed to connect boostrap socket\n");
    functor->Results(NULL, NULL, -NACL_ABI_EAGAIN);
    return;
  }

  if (!launcher->RetrieveSockAddr()) {
    NaClLog(LOG_ERROR,
            "ReverseEmulate::CreateProcess: failed to obtain socket addr\n");
    functor->Results(NULL, NULL, -NACL_ABI_EAGAIN);
    return;
  }
  // We use a 2-phase allocate-then-store scheme so that the process
  // slot does not actually hold a copy of the launcher object pointer
  // while we might need to use launcher->secure_sock_addr() or
  // launcher->socket_addr().  This is because otherwise the untrusted
  // code, by guessing pid values, could invoke FinalizeProcess to
  // cause the launcher to be deleted, causing the
  // launcher->socket_addr() etc expressions to use deallocated
  // memory.
  //
  // This race condition is not currently a real threat.  We use
  // ReverseEmulate with sel_universal, under which we run tests.  All
  // tests are written by the NaCl team and are not malicious.
  // However, this may change in the future, e.g., use sel_universal
  // to analyze in-the-wild NaCl modules.
  int32_t pid = ReserveProcessSlot();
  if (pid < 0) {
    functor->Results(NULL, NULL, -NACL_ABI_EAGAIN);
  }

  functor->Results(launcher->secure_socket_addr(),
                   launcher->socket_addr(), pid);
  SaveToProcessSlot(pid, launcher.release());
}

void ReverseEmulate::FinalizeProcess(int32_t pid) {
  if (!FreeProcessSlot(pid)) {
    NaClLog(LOG_WARNING, "FinalizeProcess(%d) failed\n", pid);
  }
}

int64_t ReverseEmulate::RequestQuotaForWrite(nacl::string file_id,
                                             int64_t offset,
                                             int64_t length) {
  NaClLog(1, "ReverseEmulate::RequestQuotaForWrite (file_id=%s, offset=%"
          NACL_PRId64", length=%"NACL_PRId64")\n", file_id.c_str(), offset,
          length);
  return length;
}

int32_t ReverseEmulate::ReserveProcessSlot() {
  nacl::MutexLocker take(&mu_);

  if (subprocesses_.size() > INT32_MAX) {
    return -NACL_ABI_EAGAIN;
  }
  int32_t pid;
  int32_t container_size = static_cast<int32_t>(subprocesses_.size());
  for (pid = 0; pid < container_size; ++pid) {
    if (!subprocesses_[pid].first) {
      break;
    }
  }
  if (pid == container_size) {
    // need to grow
    if (pid == INT32_MAX) {
      // but cannot!
      return -NACL_ABI_EAGAIN;
    }
    subprocesses_.resize(container_size + 1);
  }
  subprocesses_[pid].first = true;   // allocated/reserved...
  subprocesses_[pid].second = NULL;  // ... but still not yet in use
  return pid;
}

void ReverseEmulate::SaveToProcessSlot(
    int32_t pid,
    nacl::SelLdrLauncherStandalone *launcher) {
  nacl::MutexLocker take(&mu_);

  CHECK(subprocesses_[pid].first);
  CHECK(subprocesses_[pid].second == NULL);

  subprocesses_[pid].second = launcher;
}

bool ReverseEmulate::FreeProcessSlot(int32_t pid) {
  if (pid < 0) {
    return false;
  }
  nacl::MutexLocker take(&mu_);
  CHECK(subprocesses_.size() <= INT32_MAX);
  int32_t container_size = static_cast<int32_t>(subprocesses_.size());
  if (pid > container_size) {
    return false;
  }
  if (!subprocesses_[pid].first || subprocesses_[pid].second == NULL) {
    return false;
  }
  subprocesses_[pid].first = false;
  delete subprocesses_[pid].second;
  subprocesses_[pid].second = NULL;
  return true;
}
