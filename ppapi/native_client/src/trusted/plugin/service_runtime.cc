/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NACL_LOG_MODULE_NAME "Plugin::ServiceRuntime"

#include "native_client/src/trusted/plugin/service_runtime.h"

#include <string.h>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "native_client/src/trusted/weak_ref/call_on_main_thread.h"

#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/completion_callback.h"

using std::vector;

namespace plugin {

PluginReverseInterface::PluginReverseInterface(
    nacl::WeakRefAnchor* anchor,
    Plugin* plugin,
    pp::CompletionCallback init_done_cb,
    pp::CompletionCallback crash_cb)
      : anchor_(anchor),
        plugin_(plugin),
        shutting_down_(false),
        init_done_cb_(init_done_cb),
        crash_cb_(crash_cb) {
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cv_);
}

PluginReverseInterface::~PluginReverseInterface() {
  NaClCondVarDtor(&cv_);
  NaClMutexDtor(&mu_);
}

void PluginReverseInterface::ShutDown() {
  NaClLog(4, "PluginReverseInterface::Shutdown: entered\n");
  nacl::MutexLocker take(&mu_);
  shutting_down_ = true;
  NaClXCondVarBroadcast(&cv_);
  NaClLog(4, "PluginReverseInterface::Shutdown: broadcasted, exiting\n");
}

void PluginReverseInterface::Log(nacl::string message) {
  LogToJavaScriptConsoleResource* continuation =
      new LogToJavaScriptConsoleResource(message);
  CHECK(continuation != NULL);
  NaClLog(4, "PluginReverseInterface::Log(%s)\n", message.c_str());
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,  /* delay in ms */
      this,
      &plugin::PluginReverseInterface::Log_MainThreadContinuation,
      continuation);
}

void PluginReverseInterface::StartupInitializationComplete() {
  NaClLog(0, "PluginReverseInterface::StartupInitializationComplete\n");
  if (init_done_cb_.pp_completion_callback().func != NULL) {
    NaClLog(0,
            "PluginReverseInterface::StartupInitializationComplete:"
            " invoking CB\n");
    pp::Module::Get()->core()->CallOnMainThread(0, init_done_cb_, PP_OK);
  } else {
    NaClLog(0,
            "PluginReverseInterface::StartupInitializationComplete:"
            " init_done_cb_ not valid, skipping.\n");
  }
}

void PluginReverseInterface::Log_MainThreadContinuation(
    LogToJavaScriptConsoleResource* p,
    int32_t err) {
  UNREFERENCED_PARAMETER(err);
  NaClLog(4,
          "PluginReverseInterface::Log_MainThreadContinuation(%s)\n",
          p->message.c_str());
  plugin_->browser_interface()->AddToConsole(static_cast<Plugin*>(plugin_),
                                             p->message);
}

bool PluginReverseInterface::EnumerateManifestKeys(
    std::set<nacl::string>* out_keys) {
  Manifest const* mp = plugin_->manifest();

  if (!mp->GetFileKeys(out_keys)) {
    return false;
  }

  return true;
}

// TODO(bsy): OpenManifestEntry should use the manifest to ResolveKey
// and invoke StreamAsFile with a completion callback that invokes
// GetPOSIXFileDesc.
bool PluginReverseInterface::OpenManifestEntry(nacl::string url_key,
                                               int32_t* out_desc) {
  ErrorInfo error_info;
  bool is_portable = false;
  bool op_complete = false;  // NB: mu_ and cv_ also controls access to this!
  OpenManifestEntryResource* to_open =
      new OpenManifestEntryResource(url_key, out_desc,
                                    &error_info, &is_portable, &op_complete);
  CHECK(to_open != NULL);
  NaClLog(4, "PluginReverseInterface::OpenManifestEntry: %s\n",
          url_key.c_str());
  // This assumes we are not on the main thread.  If false, we deadlock.
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,
      this,
      &plugin::PluginReverseInterface::OpenManifestEntry_MainThreadContinuation,
      to_open);
  NaClLog(4,
          "PluginReverseInterface::OpenManifestEntry:"
          " waiting on main thread\n");
  bool shutting_down;
  do {
    nacl::MutexLocker take(&mu_);
    for (;;) {
      NaClLog(4,
              "PluginReverseInterface::OpenManifestEntry:"
              " got lock, checking shutdown and completion: (%s, %s)\n",
              shutting_down_ ? "yes" : "no",
              op_complete ? "yes" : "no");
      shutting_down = shutting_down_;
      if (op_complete || shutting_down) {
        NaClLog(4,
                "PluginReverseInterface::OpenManifestEntry:"
                " done!\n");
        break;
      }
      NaClXCondVarWait(&cv_, &mu_);
    }
  } while (0);
  if (shutting_down) {
    NaClLog(4,
            "PluginReverseInterface::OpenManifestEntry:"
            " plugin is shutting down\n");
    return false;
  }
  // out_desc has the returned descriptor if successful, else -1.

  // The caller is responsible for not closing *out_desc.  If it is
  // closed prematurely, then another open could re-use the OS
  // descriptor, confusing the opened_ map.  If the caller is going to
  // want to make a NaClDesc object and transfer it etc., then the
  // caller should DUP the descriptor (but remember the original
  // value) for use by the NaClDesc object, which closes when the
  // object is destroyed.
  NaClLog(4,
          "PluginReverseInterface::OpenManifestEntry:"
          " *out_desc = %d\n",
          *out_desc);
  if (*out_desc == -1) {
    // TODO(bsy,ncbray): what else should we do with the error?  This
    // is a runtime error that may simply be a programming error in
    // the untrusted code, or it may be something else wrong w/ the
    // manifest.
    NaClLog(4,
            "OpenManifestEntry: failed for key %s, code %d (%s)\n",
            url_key.c_str(),
            error_info.error_code(),
            error_info.message().c_str());
  }
  return true;
}

void PluginReverseInterface::OpenManifestEntry_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t err) {
  OpenManifestEntryResource *open_cont;
  UNREFERENCED_PARAMETER(err);
  // CallOnMainThread continuations always called with err == PP_OK.

  NaClLog(4, "Entered OpenManifestEntry_MainThreadContinuation\n");

  std::string mapped_url;
  if (!plugin_->manifest()->ResolveKey(p->url, &mapped_url,
                                       p->error_info, p->is_portable)) {
    NaClLog(4, "OpenManifestEntry_MainThreadContinuation: ResolveKey failed\n");
    // Failed, and error_info has the details on what happened.  Wake
    // up requesting thread -- we are done.
    nacl::MutexLocker take(&mu_);
    *p->op_complete_ptr = true;  // done...
    *p->out_desc = -1;       // but failed.
    NaClXCondVarBroadcast(&cv_);
    return;
  }
  NaClLog(4,
          "OpenManifestEntry_MainThreadContinuation: ResolveKey: %s -> %s\n",
          p->url.c_str(), mapped_url.c_str());

  open_cont = new OpenManifestEntryResource(*p);  // copy ctor!
  CHECK(open_cont != NULL);
  open_cont->url = mapped_url;
  pp::CompletionCallback stream_cc = WeakRefNewCallback(
      anchor_,
      this,
      &PluginReverseInterface::StreamAsFile_MainThreadContinuation,
      open_cont);
  if (!plugin_->StreamAsFile(mapped_url, stream_cc.pp_completion_callback())) {
    NaClLog(4,
            "OpenManifestEntry_MainThreadContinuation: StreamAsFile failed\n");
    nacl::MutexLocker take(&mu_);
    *p->op_complete_ptr = true;  // done...
    *p->out_desc = -1;       // but failed.
    p->error_info->SetReport(ERROR_MANIFEST_OPEN,
                             "ServiceRuntime: StreamAsFile failed");
    NaClXCondVarBroadcast(&cv_);
    return;
  }
  NaClLog(4,
          "OpenManifestEntry_MainThreadContinuation: StreamAsFile okay\n");
  // p is deleted automatically
}

void PluginReverseInterface::StreamAsFile_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t result) {
  NaClLog(4,
          "Entered StreamAsFile_MainThreadContinuation\n");

  nacl::MutexLocker take(&mu_);
  if (result == PP_OK) {
    NaClLog(4, "StreamAsFile_MainThreadContinuation: GetPOSIXFileDesc(%s)\n",
            p->url.c_str());
    *p->out_desc = plugin_->GetPOSIXFileDesc(p->url);
    NaClLog(4,
            "StreamAsFile_MainThreadContinuation: PP_OK, desc %d\n",
            *p->out_desc);
  } else {
    NaClLog(4,
            "StreamAsFile_MainThreadContinuation: !PP_OK, setting desc -1\n");
    *p->out_desc = -1;
    p->error_info->SetReport(ERROR_MANIFEST_OPEN,
                             "Plugin StreamAsFile failed at callback");
  }
  *p->op_complete_ptr = true;
  NaClXCondVarBroadcast(&cv_);
}

bool PluginReverseInterface::CloseManifestEntry(int32_t desc) {
  bool op_complete;
  bool op_result;
  CloseManifestEntryResource* to_close =
      new CloseManifestEntryResource(desc, &op_complete, &op_result);

  bool shutting_down;
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,
      this,
      &plugin::PluginReverseInterface::
        CloseManifestEntry_MainThreadContinuation,
      to_close);
  // wait for completion or surf-away.
  do {
    nacl::MutexLocker take(&mu_);
    for (;;) {
      shutting_down = shutting_down_;
      if (op_complete || shutting_down) {
        break;
      }
      NaClXCondVarWait(&cv_, &mu_);
    }
  } while (0);

  if (shutting_down) return false;
  // op_result true if close was successful; false otherwise (e.g., bad desc).
  return op_result;
}

void PluginReverseInterface::CloseManifestEntry_MainThreadContinuation(
    CloseManifestEntryResource* cls,
    int32_t err) {
  UNREFERENCED_PARAMETER(err);

  nacl::MutexLocker take(&mu_);
  // TODO(bsy): once the plugin has a reliable way to report that the
  // file usage is done -- and sel_ldr uses this RPC call -- we should
  // tell the plugin that the associated resources can be freed.
  *cls->op_result_ptr = true;
  *cls->op_complete_ptr = true;
  NaClXCondVarBroadcast(&cv_);
  // cls automatically deleted
}

void PluginReverseInterface::ReportCrash() {
  NaClLog(0, "PluginReverseInterface::ReportCrash\n");
  if (crash_cb_.pp_completion_callback().func != NULL) {
    NaClLog(0, "PluginReverseInterface::ReportCrash: invoking CB\n");
    pp::Module::Get()->core()->CallOnMainThread(0, crash_cb_, PP_OK);
  } else {
    NaClLog(0,
            "PluginReverseInterface::ReportCrash:"
            " crash_cb_ not valid, skipping\n");
  }
}

ServiceRuntime::ServiceRuntime(Plugin* plugin,
                               pp::CompletionCallback init_done_cb,
                               pp::CompletionCallback crash_cb)
    : plugin_(plugin),
      browser_interface_(plugin->browser_interface()),
      reverse_service_(NULL),
      subprocess_(NULL),
      async_receive_desc_(NULL),
      async_send_desc_(NULL),
      anchor_(new nacl::WeakRefAnchor()),
      rev_interface_(new PluginReverseInterface(anchor_, plugin,
                                                init_done_cb, crash_cb)) {
  NaClSrpcChannelInitialize(&command_channel_);
}

bool ServiceRuntime::InitCommunication(nacl::DescWrapper* nacl_desc,
                                       ErrorInfo* error_info) {
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (this=%p, subprocess=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(subprocess_.get())));
  // Create the command channel to the sel_ldr and load the nexe from nacl_desc.
  if (!subprocess_->SetupCommandAndLoad(&command_channel_, nacl_desc)) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                          "ServiceRuntime: command channel creation failed");
    return false;
  }
  // Hook up the reverse service channel.  We are the IMC client, but
  // provide SRPC service.
  NaClDesc* out_conn_cap;
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "reverse_setup::h",
                                &out_conn_cap);

  if (NACL_SRPC_RESULT_OK != rpc_result) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_REV_SETUP,
                          "ServiceRuntime: reverse setup rpc failed");
    return false;
  }
  //  Get connection capability to service runtime where the IMC
  //  server/SRPC client is waiting for a rendezvous.
  PLUGIN_PRINTF(("ServiceRuntime: got 0x%"NACL_PRIxPTR"\n",
                 (uintptr_t) out_conn_cap));
  nacl::DescWrapper* conn_cap = plugin_->wrapper_factory()->MakeGenericCleanup(
      out_conn_cap);
  if (conn_cap == NULL) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_WRAPPER,
                          "ServiceRuntime: wrapper allocation failure");
    return false;
  }
  out_conn_cap = NULL;  // ownership passed
  reverse_service_ = new nacl::ReverseService(conn_cap, rev_interface_->Ref());
  if (!reverse_service_->Start()) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_REV_SERVICE,
                          "ServiceRuntime: starting reverse services failed");
    return false;
  }

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  // Establish the communication for handle passing protocol
  struct NaClDesc* desc = NaClHandlePassBrowserGetSocketAddress();

  DWORD my_pid = GetCurrentProcessId();
  nacl::Handle my_handle = GetCurrentProcess();
  nacl::Handle my_handle_in_selldr;

  if (!DuplicateHandle(GetCurrentProcess(),
                       my_handle,
                       subprocess_->child_process(),
                       &my_handle_in_selldr,
                       PROCESS_DUP_HANDLE,
                       FALSE,
                       0)) {
    error_info->SetReport(ERROR_SEL_LDR_HANDLE_PASSING,
                          "ServiceRuntime: failed handle passing protocol");
    return false;
  }

  rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "init_handle_passing:hii:",
                                desc,
                                my_pid,
                                reinterpret_cast<int>(my_handle_in_selldr));

  if (NACL_SRPC_RESULT_OK != rpc_result) {
    error_info->SetReport(ERROR_SEL_LDR_HANDLE_PASSING,
                          "ServiceRuntime: failed handle passing protocol");
    return false;
  }
#endif
  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status = -1;
  rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "start_module::i",
                                &load_status);

  if (NACL_SRPC_RESULT_OK != rpc_result) {
    error_info->SetReport(ERROR_SEL_LDR_START_MODULE,
                          "ServiceRuntime: could not start nacl module");
    return false;
  }
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (load_status=%d)\n",
                 load_status));
  plugin_->ReportSelLdrLoadStatus(load_status);
  if (LOAD_OK != load_status) {
    error_info->SetReport(
        ERROR_SEL_LDR_START_STATUS,
        NaClErrorString(static_cast<NaClErrorCode>(load_status)));
    return false;
  }
  return true;
}

bool ServiceRuntime::Start(nacl::DescWrapper* nacl_desc,
                           ErrorInfo* error_info) {
  PLUGIN_PRINTF(("ServiceRuntime::Start (nacl_desc=%p)\n",
                 reinterpret_cast<void*>(nacl_desc)));

  nacl::scoped_ptr<nacl::SelLdrLauncher>
      tmp_subprocess(new(std::nothrow) nacl::SelLdrLauncher());
  if (NULL == tmp_subprocess.get()) {
    PLUGIN_PRINTF(("ServiceRuntime::Start (subprocess create failed)\n"));
    error_info->SetReport(ERROR_SEL_LDR_CREATE_LAUNCHER,
                          "ServiceRuntime: failed to create sel_ldr launcher");
    return false;
  }
  nacl::Handle sockets[3];
  if (!tmp_subprocess->Start(NACL_ARRAY_SIZE(sockets), sockets)) {
    PLUGIN_PRINTF(("ServiceRuntime::Start (start failed)\n"));
    error_info->SetReport(ERROR_SEL_LDR_LAUNCH,
                          "ServiceRuntime: failed to start");
    return false;
  }

  async_receive_desc_.reset(
      plugin()->wrapper_factory()->MakeImcSock(sockets[1]));
  async_send_desc_.reset(plugin()->wrapper_factory()->MakeImcSock(sockets[2]));

  subprocess_.reset(tmp_subprocess.release());
  if (!InitCommunication(nacl_desc, error_info)) {
    subprocess_.reset(NULL);
    return false;
  }

  PLUGIN_PRINTF(("ServiceRuntime::Start (return 1)\n"));
  return true;
}

SrpcClient* ServiceRuntime::SetupAppChannel() {
  PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (subprocess_=%p)\n",
                 reinterpret_cast<void*>(subprocess_.get())));
  nacl::DescWrapper* connect_desc = subprocess_->socket_addr()->Connect();
  if (NULL == connect_desc) {
    PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (connect failed)\n"));
    return NULL;
  } else {
    PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (conect_desc=%p)\n",
                   static_cast<void*>(connect_desc)));
    SrpcClient* srpc_client = SrpcClient::New(plugin(), connect_desc);
    PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (srpc_client=%p)\n",
                   static_cast<void*>(srpc_client)));
    return srpc_client;
  }
}

bool ServiceRuntime::Kill() {
  return subprocess_->KillChildProcess();
}

bool ServiceRuntime::Log(int severity, nacl::string msg) {
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "log:is:",
                                severity,
                                strdup(msg.c_str()));
  return (NACL_SRPC_RESULT_OK == rpc_result);
}

void ServiceRuntime::Shutdown() {
  if (subprocess_ != NULL) {
    Kill();
  }
  rev_interface_->ShutDown();
  anchor_->Abandon();
  // Abandon callbacks, tell service threads to quit if they were
  // blocked waiting for main thread operations to finish.  Note that
  // some callbacks must still await their completion event, e.g.,
  // CallOnMainThread must still wait for the time out, or I/O events
  // must finish, so resources associated with pending events cannot
  // be deallocated.

  // Note that this does waitpid() to get rid of any zombie subprocess.
  subprocess_.reset(NULL);

  NaClSrpcDtor(&command_channel_);

  // subprocess_ killed, but threads waiting on messages from the
  // service runtime may not have noticed yet.  The low-level
  // NaClSimpleRevService code takes care to refcount the data objects
  // that it needs, and reverse_service_ is also refcounted.  We wait
  // for the service threads to get their EOF indications.
  if (reverse_service_ != NULL) {
    reverse_service_->WaitForServiceThreadsToExit();
    reverse_service_->Unref();
    reverse_service_ = NULL;
  }
}

ServiceRuntime::~ServiceRuntime() {
  PLUGIN_PRINTF(("ServiceRuntime::~ServiceRuntime (this=%p)\n",
                 static_cast<void*>(this)));
  // We do this just in case Shutdown() was not called.
  subprocess_.reset(NULL);
  if (reverse_service_ != NULL) {
    reverse_service_->Unref();
  }

  rev_interface_->Unref();

  anchor_->Unref();
}

}  // namespace plugin
