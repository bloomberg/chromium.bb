/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/service_runtime.h"

#include <string.h>
#include <utility>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "native_client/src/trusted/weak_ref/call_on_main_thread.h"

#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/cpp/core.h"
#include "native_client/src/third_party/ppapi/cpp/completion_callback.h"

using std::vector;

namespace plugin {

typedef std::pair<Plugin*, nacl::string> LogCallbackData;

struct LogToJavaScriptConsoleResource {
 public:
  LogToJavaScriptConsoleResource(std::string msg, Plugin* plugin_ptr)
      : message(msg),
        plugin(plugin_ptr) {}
  std::string message;
  Plugin* plugin;
};

// Must be called on the main thread.
void LogToJavaScriptConsole(LogToJavaScriptConsoleResource* p,
                            int32_t err) {
  UNREFERENCED_PARAMETER(err);
  p->plugin->browser_interface()->AddToConsole(p->plugin->instance_id(),
                                               p->message);
}

void PluginReverseInterface::Log(nacl::string message) {
  (void) plugin::WeakRefCallOnMainThread(
      anchor_,
      0,  /* delay in ms */
      LogToJavaScriptConsole,
      new LogToJavaScriptConsoleResource(
          message, plugin_));
}

ServiceRuntime::ServiceRuntime(Plugin* plugin)
    : plugin_(plugin),
      browser_interface_(plugin->browser_interface()),
      reverse_service_(NULL),
      subprocess_(NULL),
      async_receive_desc_(NULL),
      async_send_desc_(NULL),
      anchor_(new nacl::WeakRefAnchor()),
      rev_interface_(new PluginReverseInterface(anchor_, plugin)) {
}

bool ServiceRuntime::InitCommunication(nacl::DescWrapper* nacl_desc,
                                       ErrorInfo* error_info) {
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (this=%p, subprocess=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(subprocess_.get())));
  // Create the command channel to the sel_ldr and load the nexe from nacl_desc.
  if (!subprocess_->SetupCommandAndLoad(&command_channel_, nacl_desc)) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION,
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
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION,
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
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION,
                          "ServiceRuntime: wrapper allocation failure");
    return false;
  }
  out_conn_cap = NULL;  // ownership passed
  reverse_service_ = new nacl::ReverseService(conn_cap, rev_interface_->Ref());
  if (!reverse_service_->Start()) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION,
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
  }
  reverse_service_ = NULL;
}

ServiceRuntime::~ServiceRuntime() {
  PLUGIN_PRINTF(("ServiceRuntime::~ServiceRuntime (this=%p)\n",
                 static_cast<void*>(this)));

  // We do this just in case Terminate() was not called.
  subprocess_.reset(NULL);
  if (reverse_service_ != NULL) {
    reverse_service_->Unref();
  }

  rev_interface_->Unref();

  anchor_->Abandon();
  anchor_->Unref();
}

}  // namespace plugin
