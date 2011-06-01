/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/service_runtime.h"

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
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/shared_memory.h"
#include "native_client/src/trusted/plugin/socket_address.h"
#include "native_client/src/trusted/plugin/srt_socket.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/completion_callback.h"

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
void LogToJavaScriptConsole(nacl::WeakRef<LogToJavaScriptConsoleResource>* wr,
                            int32_t err) {
  UNREFERENCED_PARAMETER(err);
  nacl::scoped_ptr<LogToJavaScriptConsoleResource> p;
  wr->ReleaseAndUnref(&p);
  if (p == NULL) {
    NaClLog(0, "LogToJavaScriptConsole: Weak ref died\n");
    return;
  }
  NaClLog(0, "LogToJavaScriptConsole: Weak ref okay: %p\n",
          reinterpret_cast<void*>(p.get()));
  p->plugin->browser_interface()->AddToConsole(p->plugin->instance_id(),
                                               p->message);
}

void LogToJavaScriptConsoleInterface::Log(nacl::string message) {
  (void) WeakRefCompletionCallback(anchor_, 0,
                                   LogToJavaScriptConsole,
                                   new LogToJavaScriptConsoleResource(
                                       message, plugin_));
}

ServiceRuntime::ServiceRuntime(Plugin* plugin)
    : default_socket_address_(NULL),
      plugin_(plugin),
      browser_interface_(plugin->browser_interface()),
      runtime_channel_(NULL),
      reverse_service_(NULL),
      subprocess_(NULL),
      async_receive_desc_(NULL),
      async_send_desc_(NULL),
      anchor_(new nacl::WeakRefAnchor()),
      log_interface_(new LogToJavaScriptConsoleInterface(anchor_, plugin)) {
}

bool ServiceRuntime::InitCommunication(nacl::Handle bootstrap_socket,
                                       nacl::DescWrapper* nacl_desc,
                                       nacl::string* error_string) {
  // TODO(sehr): this should use the new
  // SelLdrLauncher::OpenSrpcChannels interface, which should be free
  // of resource leaks.
  // bootstrap_socket was opened to communicate with the sel_ldr instance.
  // Get the first IMC message and receive a socket address FD from it.
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (this=%p, subprocess=%p, bootstrap_socket=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(subprocess_),
                 reinterpret_cast<void*>(bootstrap_socket)));
  // GetSocketAddress implicitly invokes Close(bootstrap_socket).
  // Get service channel descriptor.
  default_socket_address_ = GetSocketAddress(plugin_, bootstrap_socket);
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (default_socket_address=%p)\n",
                 static_cast<void*>(default_socket_address_)));

  if (NULL == default_socket_address_) {
    *error_string = "sel_ldr: no valid socket address";
    return false;
  }
  // The first connect on the socket address returns the service
  // runtime command channel.  This channel is created before the NaCl
  // module runs, and is private to the service runtime.  This channel
  // is used for a secure plugin<->service runtime communication path
  // that can be used to forcibly shut down the sel_ldr.
  PortableHandle* portable_socket_address = default_socket_address_->handle();
  ScriptableHandle* raw_channel = portable_socket_address->Connect();
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (raw_channel=%p)\n",
                 static_cast<void*>(raw_channel)));
  if (NULL == raw_channel) {
    *error_string = "sel_ldr: connect failed";
    return false;
  }
  runtime_channel_ = new(std::nothrow) SrtSocket(raw_channel,
                                                 browser_interface_);
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (runtime_channel=%p)\n",
                 static_cast<void*>(runtime_channel_)));
  if (NULL == runtime_channel_) {
    raw_channel->Unref();
    *error_string = "sel_ldr: runtime channel creation failed";
    return false;
  }

  // Hook up the reverse service channel.  We are the IMC client, but
  // provide SRPC service.

  //  Get connection capability to service runtime where the IMC
  //  server/SRPC client is waiting for a rendezvous.
  NaClDesc* out_conn_cap;
  PLUGIN_PRINTF(("ServiceRuntime: invoking reverse setup\n"));
  if (!runtime_channel_->ReverseSetup(&out_conn_cap)) {
    *error_string = "sel_ldr: reverse service channel setup failure";
    return false;
  }
  PLUGIN_PRINTF(("ServiceRuntime: got 0x%"NACL_PRIxPTR"\n",
                 (uintptr_t) out_conn_cap));
  nacl::DescWrapper* conn_cap = plugin_->wrapper_factory()->MakeGenericCleanup(
      out_conn_cap);
  if (conn_cap == NULL) {
    *error_string = "ServiceRuntime: wrapper allocation failure";
    return false;
  }
  out_conn_cap = NULL;  // ownership passed
  reverse_service_ = new nacl::ReverseService(conn_cap, log_interface_->Ref());
  if (!reverse_service_->Start()) {
    *error_string = "ServiceRuntime: starting reverse services failed";
    return false;
  }

  if (nacl_desc != NULL) {
    // This is executed only if we already launched sel_ldr process and
    // now have an open communication channel to it, so we can send
    // the descriptor for nexe over.
    if (!runtime_channel_->LoadModule(nacl_desc->desc())) {
      // TODO(gregoryd): close communication channels
      *error_string = "sel_ldr: failed to send nexe";
      return false;
    }
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
    // Establish the communication for handle passing protocol
    struct NaClDesc* desc = NaClHandlePassBrowserGetSocketAddress();
    if (!runtime_channel_->InitHandlePassing(desc,
                                             subprocess_->child_process())) {
      *error_string = "sel_ldr: failed handle passing protocol";
      return false;
    }
#endif
  }

  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status;
  if (!runtime_channel_->StartModule(&load_status)) {
    *error_string = "sel_ldr: could not start nacl module";
    return false;
  }
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (load_status=%d)\n",
                 load_status));
  if (LOAD_OK != load_status) {
    *error_string =
        "sel_ldr: loading of module failed with status " + load_status;
    return false;
  }
  return true;
}

bool ServiceRuntime::StartFromCommandLine(nacl::string nacl_file,
                                          nacl::DescWrapper* nacl_file_desc,
                                          nacl::string* error_string) {
  PLUGIN_PRINTF(("ServiceRuntime::StartFromCommandLine "
                 "(nacl_file='%s', nacl_file_desc='%p')\n",
                 nacl_file.c_str(), reinterpret_cast<void*>(nacl_file_desc)));
  CHECK(nacl_file != NACL_NO_FILE_PATH || nacl_file_desc != NULL);

  nacl::scoped_ptr<nacl::SelLdrLauncher>
      tmp_subprocess(new(std::nothrow) nacl::SelLdrLauncher());
  if (NULL == tmp_subprocess.get()) {
    *error_string = "sel_ldr: failed to create sel_ldr launcher";
    return false;
  }

  // The arguments we want to pass to the service runtime are
  // "-X 5" causes the service runtime to create a bound socket and socket
  //      address at descriptors 3 and 4.  The socket address is transferred as
  //      the first IMC message on descriptor 5.  This is used when connecting
  //      to socket addresses.
  const char* kSelLdrArgs[] = { "-X", "5" };
  const int kSelLdrArgLength = NACL_ARRAY_SIZE(kSelLdrArgs);
  vector<nacl::string> args_for_sel_ldr(kSelLdrArgs,
                                        kSelLdrArgs + kSelLdrArgLength);
  vector<nacl::string> args_for_nexe;
  const char* irt_library_path = getenv("NACL_IRT_LIBRARY");
  if (NULL != irt_library_path) {
    args_for_sel_ldr.push_back("-B");
    args_for_sel_ldr.push_back(irt_library_path);
  }
  tmp_subprocess->InitCommandLine(nacl_file,
                                  -1,
                                  args_for_sel_ldr,
                                  args_for_nexe);

  nacl::Handle recv_handle = tmp_subprocess->ExportImcFD(6);
  if (recv_handle == nacl::kInvalidHandle) {
    *error_string = "sel_ldr: failed to create async receive handle";
    return false;
  }
  async_receive_desc_ = plugin()->wrapper_factory()->MakeImcSock(recv_handle);

  nacl::Handle send_handle = tmp_subprocess->ExportImcFD(7);
  if (send_handle == nacl::kInvalidHandle) {
    *error_string = "sel_ldr: failed to create async send handle";
    return false;
  }
  async_send_desc_ = plugin()->wrapper_factory()->MakeImcSock(send_handle);

  nacl::Handle bootstrap_socket = tmp_subprocess->ExportImcFD(5);
  if (bootstrap_socket == nacl::kInvalidHandle) {
    *error_string = "sel_ldr: failed to create socket handle";
    return false;
  }

  if (!tmp_subprocess->LaunchFromCommandLine()) {
    *error_string = "sel_ldr: failed to launch";
    return false;
  }

  if (!InitCommunication(bootstrap_socket, nacl_file_desc, error_string)) {
    return false;
  }

  PLUGIN_PRINTF(("ServiceRuntime::Start (return 1)\n"));
  subprocess_ = tmp_subprocess.release();
  return true;
}

bool ServiceRuntime::StartFromBrowser(nacl::string nacl_url,
                                      nacl::DescWrapper* nacl_desc,
                                      nacl::string* error_string) {
  PLUGIN_PRINTF(("ServiceRuntime::StartFromBrowser (nacl_url=%s)\n",
                 nacl_url.c_str()));
  PLUGIN_PRINTF(("ServiceRuntime::StartFromBrowser (nacl_desc=%p)\n",
                 reinterpret_cast<void*>(nacl_desc)));

  nacl::scoped_ptr<nacl::SelLdrLauncher>
      tmp_subprocess(new(std::nothrow) nacl::SelLdrLauncher());
  if (NULL == tmp_subprocess.get()) {
    *error_string = "sel_ldr: failed to create launcher";
    return false;
  }
  nacl::Handle sockets[3];
  if (!tmp_subprocess->StartFromBrowser(nacl_url.c_str(),
                                        NACL_ARRAY_SIZE(sockets),
                                        sockets)) {
    *error_string = "sel_ldr: failed to start";
    return false;
  }

  nacl::Handle bootstrap_socket = sockets[0];
  async_receive_desc_ = plugin()->wrapper_factory()->MakeImcSock(sockets[1]);
  async_send_desc_ = plugin()->wrapper_factory()->MakeImcSock(sockets[2]);

  subprocess_ = tmp_subprocess.release();
  if (!InitCommunication(bootstrap_socket, nacl_desc, error_string)) {
    delete subprocess_;
    subprocess_ = NULL;
    return false;
  }

  PLUGIN_PRINTF(("ServiceRuntime::StartFromBrowser (return 1)\n"));
  return true;
}

bool ServiceRuntime::Kill() {
  return subprocess_->KillChildProcess();
}

bool ServiceRuntime::Log(int severity, nacl::string msg) {
  return runtime_channel_->Log(severity, msg);
}

void ServiceRuntime::Shutdown() {
  if (subprocess_ != NULL) {
    Kill();
  }

  // Note that this does waitpid() to get rid of any zombie subprocess.
  delete subprocess_;
  subprocess_ = NULL;

  delete runtime_channel_;
  runtime_channel_ = NULL;

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
  delete subprocess_;
  delete runtime_channel_;
  if (reverse_service_ != NULL) {
    reverse_service_->Unref();
  }

  log_interface_->Unref();

  // TODO(sehr,mseaborn): use scoped_ptr for management of DescWrappers.
  delete async_receive_desc_;
  delete async_send_desc_;

  anchor_->Abandon();
  anchor_->Unref();
}

ScriptableHandle* ServiceRuntime::default_socket_address() const {
  PLUGIN_PRINTF(("ServiceRuntime::default_socket_address"
                 " (this=%p, default_socket_address=%p)\n",
                 static_cast<void*>(const_cast<ServiceRuntime*>(this)),
                 static_cast<void*>(const_cast<ScriptableHandle*>(
                     default_socket_address_))));

  return default_socket_address_;
}

ScriptableHandle* ServiceRuntime::GetSocketAddress(Plugin* plugin,
                                                   nacl::Handle channel) {
  nacl::DescWrapper::MsgHeader header;
  nacl::DescWrapper::MsgIoVec iovec[1];
  unsigned char bytes[NACL_ABI_IMC_USER_BYTES_MAX];
  nacl::DescWrapper* descs[NACL_ABI_IMC_USER_DESC_MAX];

  PLUGIN_PRINTF(("ServiceRuntime::GetSocketAddress (plugin=%p, channel=%p)\n",
                 static_cast<void*>(plugin),
                 reinterpret_cast<void*>(channel)));

  ScriptableHandle* retval = NULL;
  nacl::DescWrapper* imc_desc = plugin->wrapper_factory()->MakeImcSock(channel);
  // Set up to receive a message.
  iovec[0].base = bytes;
  iovec[0].length = NACL_ABI_IMC_USER_BYTES_MAX;
  header.iov = iovec;
  header.iov_length = NACL_ARRAY_SIZE(iovec);
  header.ndescv = descs;
  header.ndescv_length = NACL_ARRAY_SIZE(descs);
  header.flags = 0;
  // Receive the message.
  ssize_t ret = imc_desc->RecvMsg(&header, 0);
  // Check that there was exactly one descriptor passed.
  if (0 > ret || 1 != header.ndescv_length) {
    PLUGIN_PRINTF(("ServiceRuntime::GetSocketAddress"
                   " (message receive failed %" NACL_PRIdS " %"
                   NACL_PRIdNACL_SIZE " %" NACL_PRIdNACL_SIZE ")\n", ret,
                   header.ndescv_length,
                   header.iov_length));
    goto cleanup;
  }
  PLUGIN_PRINTF(("ServiceRuntime::GetSocketAddress"
                 " (-X result descriptor descs[0]=%p)\n",
                 static_cast<void*>(descs[0])));
  retval = browser_interface_->NewScriptableHandle(
      SocketAddress::New(plugin, descs[0]));
 cleanup:
  // TODO(sehr,mseaborn): use scoped_ptr for management of DescWrappers.
  delete imc_desc;
  PLUGIN_PRINTF(("ServiceRuntime::GetSocketAddress (return %p)\n",
                 static_cast<void*>(retval)));
  return retval;
}

}  // namespace plugin
