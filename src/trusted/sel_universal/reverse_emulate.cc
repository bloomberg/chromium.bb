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
#include "native_client/src/public/nacl_file_info.h"
#include "native_client/src/public/secure_service.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/launcher_factory.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"


// Mock of ReverseInterface for use by nexes.
class ReverseEmulate : public nacl::ReverseInterface {
 public:
  ReverseEmulate(
      nacl::SelLdrLauncherStandaloneFactory* factory,
      const vector<nacl::string>& prefix,
      const vector<nacl::string>& sel_ldr_argv);
  virtual ~ReverseEmulate();

  // Startup handshake
  virtual void StartupInitializationComplete();

  // Name service use.
  virtual bool OpenManifestEntry(nacl::string url_key,
                                 struct NaClFileInfo* info);
  virtual void ReportCrash();

  // Request quota for a write to a file.
  virtual int64_t RequestQuotaForWrite(nacl::string file_id,
                                       int64_t offset,
                                       int64_t length);

  // covariant impl of Ref()
  ReverseEmulate* Ref() {  // down_cast
    return reinterpret_cast<ReverseEmulate*>(RefCountBase::Ref());
  }

 private:
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
      NaClSrpcInvokeBySignature(command_channel,
                                NACL_SECURE_SERVICE_REVERSE_SETUP,
                                &h);
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
}

ReverseEmulate::~ReverseEmulate() {
  NaClLog(1, "ReverseEmulate::~ReverseEmulate\n");
}

void ReverseEmulate::StartupInitializationComplete() {
  NaClLog(1, "ReverseEmulate::StartupInitializationComplete ()\n");
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

void ReverseEmulate::ReportCrash() {
  NaClLog(1, "ReverseEmulate::ReportCrash\n");
  nacl::MutexLocker take(&g_exit_mu);
  g_exited = true;
  NaClXCondVarBroadcast(&g_exit_cv);
}

int64_t ReverseEmulate::RequestQuotaForWrite(nacl::string file_id,
                                             int64_t offset,
                                             int64_t length) {
  NaClLog(1, "ReverseEmulate::RequestQuotaForWrite (file_id=%s, offset=%"
          NACL_PRId64 ", length=%" NACL_PRId64 ")\n", file_id.c_str(), offset,
          length);
  return length;
}
