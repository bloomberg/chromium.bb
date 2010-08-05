/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/service_runtime.h"

#include <map>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_log.h"
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

using std::vector;

namespace plugin {

ServiceRuntime::ServiceRuntime(
    BrowserInterface* browser_interface,
    Plugin* plugin) :
    async_receive_desc(NULL),
    async_send_desc(NULL),
    browser_interface_(browser_interface),
    default_socket_address_(NULL),
    default_socket_(NULL),
    plugin_(plugin),
    runtime_channel_(NULL),
    subprocess_(NULL) {
}

// shm is consumed (Delete invoked).
bool ServiceRuntime::InitCommunication(nacl::Handle bootstrap_socket,
                                       nacl::DescWrapper* shm) {
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
    const char* error = "service runtime: no valid socket address";
    PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (%s)\n", error));
    browser_interface_->Alert(plugin()->instance_id(), error);
    return false;
  }
  ScriptableHandle* raw_channel;
  // The first connect on the socket address returns the service
  // runtime command channel.  This channel is created before the NaCl
  // module runs, and is private to the service runtime.  This channel
  // is used for a secure plugin<->service runtime communication path


  // that can be used to forcibly shut down the sel_ldr.
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (connecting for SrtSocket)\n"));
  PortableHandle* portable_socket_address = default_socket_address_->handle();
  raw_channel = portable_socket_address->Connect();
  if (NULL == raw_channel) {
    const char* error = "service runtime connect failed";
    PLUGIN_PRINTF(("ServiceRuntime::InitCommuncation (%s)\n", error));
    browser_interface_->Alert(plugin()->instance_id(), error);
    return false;
  }
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (constructing runtime channel)\n"));
  runtime_channel_ = new(std::nothrow) SrtSocket(raw_channel,
                                                 browser_interface_);
  if (NULL == runtime_channel_) {
    // TODO(sehr): leaking raw_channel.
    return false;
  }

  // Set module's origin at the service runtime.  NB: we may end up
  // enforcing restrictions for network access to the origin of the
  // module only in the plugin.  Here's why:
  //
  // When a plugin using NPAPI invokes NPP_GetURLNotify (on the behalf
  // of a NaCl module), it may provide a relative URL; even if it is
  // not a relative URL, 3xx redirects may change the source domain of
  // the actual web content as a side effect -- the 3xx redirects
  // cause the browser to make new HTTP connections, and the browser
  // does not notify the plugin as this is occurring.  However, when
  // the browser invokes NPN_NewStream and then either
  // NPN_StreamAsFile or NPP_WriteReady/NPP_Write to deliver the
  // content, the NPStream object contains the fully-qualified URL of
  // the web content, after redirections (if any).  This means that we
  // should parse the FQ-URL at NPN_NewStream or NPN_StreamAsFile,
  // rather than try to canonicalize the URL before passing it to
  // NPP_GetURLNotify, since we won't know the final FQ-URL at that
  // point.
  //
  // This strategy also implies that we might allow any URL, e.g., a
  // tinyurl.com redirecting URL, to be used with NPP_GetURLNotify,
  // and allow the access if the final FQ-URL is the same domain.
  // This has the hazard that we still are doing fetches to other
  // domains (including sending cookies specific to those domains),
  // just not making the results available to the NaCl module; the
  // same thing occurs if IMG SRC tags were scripted.
  //
  // A perhaps more important downside is that this is an easy way for
  // NaCl modules to leak information: the target web server is always
  // contacted, since we don't know if
  // http://evil.org/leak?s3kr1t_inf0 might result in a 3xx redirect
  // to an acceptable origin.  If we want to prevent this, we could
  // require that all URLs given to NPP_GetURLNotify are either
  // relative or are absolute with the same origin as the module; this
  // would prevent the scenario where an evil module is hosted (e.g.,
  // in temporary upload directory that's visible, or as a gmail
  // attachment URL) at an innocent server to extract confidential
  // info and send it to third party servers.  An explict network ACL
  // a la /robots.txt might be useful to serve as an ingress filter.z

  PLUGIN_PRINTF(("ServiceRuntime::InitCommuncation (invoking set_origin)\n"));
  if (!runtime_channel_->SetOrigin(plugin_->nacl_module_origin())) {
    const char* error = "could not set origin";
    PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (%s)\n", error));
    browser_interface_->Alert(plugin()->instance_id(), error);
    // TODO(sehr): leaking raw_channel and runtime_channel_.
    return false;
  }

  if (shm != NULL) {
    // This code is executed only if NaCl is running as a built-in plugin
    // in Chrome.
    // We now have an open communication channel to the sel_ldr process,
    // so we can send the nexe bits over
    if (!runtime_channel_->LoadModule(shm->desc())) {
      const char* error = "failed to send nexe";
      PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (%s)\n", error));
      browser_interface_->Alert(plugin()->instance_id(), error);
      shm->Delete();
      // TODO(gregoryd): close communication channels
      delete subprocess_;
      subprocess_ = NULL;
      return false;
    }
    /* LoadModule succeeded, proceed normally */
    shm->Delete();
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
    // Establish the communication for handle passing protocol
    struct NaClDesc* desc = NaClHandlePassBrowserGetSocketAddress();
    if (!runtime_channel_->InitHandlePassing(desc, subprocess_->child())) {
      delete subprocess_;
      subprocess_ = NULL;
      return false;
    }
#endif
  }

  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status;

  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (invoking start_module RPC)\n"));
  if (!runtime_channel_->StartModule(&load_status)) {
    const char* error = "could not start nacl module";
    PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (%s)\n", error));
    browser_interface_->Alert(plugin()->instance_id(), error);
    // TODO(sehr): leaking raw_channel and runtime_channel_.
    return false;
  }
  PLUGIN_PRINTF((" start_module returned %d\n", load_status));
  if (LOAD_OK != load_status) {
    nacl::string error = "loading of module failed with status " + load_status;
    PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (%s)\n", error.c_str()));
    browser_interface_->Alert(plugin()->instance_id(), error.c_str());
    // TODO(sehr): leaking raw_channel and runtime_channel_.
    return false;
  }

  // The second connect on the socket address is to the untrusted side
  // of sel_ldr.
  default_socket_ = portable_socket_address->Connect();
  if (NULL == default_socket_) {
    PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                   " (srpc channel connect failed)\n"));
    // TODO(sehr): leaking raw_channel and runtime_channel_.
    return false;
  }
  default_socket_->handle()->StartJSObjectProxy(plugin_);
  plugin_->EnableVideo();
  // Create the listener thread and initialize the nacl module.
  if (!plugin_->InitializeModuleMultimedia(default_socket_, this)) {
    // TODO(sehr): leaking raw_channel, runtime_channel_, and default_socket_.
    return false;
  }
  return true;
}

bool ServiceRuntime::Start(const char* nacl_file) {
  PLUGIN_PRINTF(("ServiceRuntime::Start (nacl_file='%s')\n", nacl_file));
  // The arguments we want to pass to the service runtime are
  // "-P 5" sets the default SRPC channel to be over descriptor 5.  The 5 needs
  //      to match the 5 in the Launcher invocation below.
  // "-X 5" causes the service runtime to create a bound socket and socket
  //      address at descriptors 3 and 4.  The socket address is transferred as
  //      the first IMC message on descriptor 5.  This is used when connecting
  //      to socket addresses.
  // "-d" (not default) invokes the service runtime in debug mode.
  // const char* kSelLdrArgs[] = { "-P", "5", "-X", "5" };
  const char* kSelLdrArgs[] = { "-P", "5", "-X", "5" };
  // TODO(sehr): remove -P support and default channels.
  const int kSelLdrArgLength = NACL_ARRAY_SIZE(kSelLdrArgs);
  vector<nacl::string> kArgv(kSelLdrArgs, kSelLdrArgs + kSelLdrArgLength);
  vector<nacl::string> kEmpty;

  PLUGIN_PRINTF(("ServiceRuntime::Start (this=%p, plugin=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(plugin())));

  subprocess_ = new(std::nothrow) nacl::SelLdrLauncher();
  if (NULL == subprocess_) {
    const char* error = "could not create SelLdrLauncher";
    PLUGIN_PRINTF(("ServiceRuntime::Start (%s)\n", error));
    browser_interface_->Alert(plugin()->instance_id(), error);
    return false;
  }
  subprocess_->Init(nacl_file, -1, kArgv, kEmpty);

  nacl::Handle receive_handle = subprocess_->ExportImcFD(6);
  if (receive_handle == nacl::kInvalidHandle) {
    browser_interface_->Alert(plugin()->instance_id(),
                              "Failed to create async receive handle");
    return false;
  }
  async_receive_desc = plugin()->wrapper_factory()->MakeImcSock(receive_handle);

  nacl::Handle send_handle = subprocess_->ExportImcFD(7);
  if (send_handle == nacl::kInvalidHandle) {
    browser_interface_->Alert(plugin()->instance_id(),
                              "Failed to create async send handle");
    return false;
  }
  async_send_desc = plugin()->wrapper_factory()->MakeImcSock(send_handle);

  nacl::Handle bootstrap_socket = subprocess_->ExportImcFD(5);
  if (bootstrap_socket == nacl::kInvalidHandle) {
    browser_interface_->Alert(plugin()->instance_id(),
                              "Failed to create socket handle");
    return false;
  }

  if (!subprocess_->Launch()) {
    const char* error = "could not start SelLdrLauncher";
    PLUGIN_PRINTF(("ServiceRuntime::Start (%s)\n", error));
    browser_interface_->Alert(plugin()->instance_id(), error);
    delete subprocess_;
    subprocess_ = NULL;
    return false;
  }

  // TODO(gregoryd) - this should deal with buffer and size correctly
  // - do we need to send the load command from here?
  if (!InitCommunication(bootstrap_socket, NULL)) {
    return false;
  }

  PLUGIN_PRINTF(("ServiceRuntime::Start (return 1)\n"));
  return true;
}

bool ServiceRuntime::StartUnderChromium(const char* url,
                                        nacl::DescWrapper* shm) {
  subprocess_ = new(std::nothrow) nacl::SelLdrLauncher();
  if (NULL == subprocess_) {
    return false;
  }
  nacl::Handle sockets[3];
  if (!subprocess_->StartUnderChromium(url, NACL_ARRAY_SIZE(sockets),
                                       sockets)) {
    delete subprocess_;
    subprocess_ = NULL;
    return false;
  }

  nacl::Handle bootstrap_socket = sockets[0];
  async_receive_desc = plugin()->wrapper_factory()->MakeImcSock(sockets[1]);
  async_send_desc = plugin()->wrapper_factory()->MakeImcSock(sockets[2]);

  if (!InitCommunication(bootstrap_socket, shm)) {
    return false;
  }

  PLUGIN_PRINTF(("ServiceRuntime::Start (return 1)\n"));
  return true;
}

bool ServiceRuntime::Kill() {
  return subprocess_->KillChild();
}

bool ServiceRuntime::Log(int severity, nacl::string msg) {
  return runtime_channel_->Log(severity, msg);
}

void ServiceRuntime::Shutdown() {
  if (subprocess_ != NULL) {
    Kill();
  }
  // This waits for the upcall thread to exit so it must come after we
  // terminate the subprocess.
  plugin_->ShutdownMultimedia();

  // Note that this does waitpid() to get rid of any zombie subprocess.
  delete subprocess_;
  subprocess_ = NULL;

  delete runtime_channel_;
  runtime_channel_ = NULL;
}

ServiceRuntime::~ServiceRuntime() {
  PLUGIN_PRINTF(("ServiceRuntime::~ServiceRuntime (this=%p)\n",
                 static_cast<void*>(this)));

  // We do this just in case Terminate() was not called.
  delete subprocess_;
  delete runtime_channel_;

  if (async_receive_desc != NULL) {
    async_receive_desc->Delete();
  }
  if (async_send_desc != NULL) {
    async_send_desc->Delete();
  }
}

ScriptableHandle* ServiceRuntime::default_socket_address() const {
  PLUGIN_PRINTF(("ServiceRuntime::default_socket_address"
                 " (this=%p, default_socket_address=%p)\n",
                 static_cast<void*>(const_cast<ServiceRuntime*>(this)),
                 static_cast<void*>(const_cast<ScriptableHandle*>(
                     default_socket_address_))));

  return default_socket_address_;
}

ScriptableHandle* ServiceRuntime::default_socket() const {
  PLUGIN_PRINTF(("ServiceRuntime::default_socket"
                 " (this=%p, default_socket=%p)\n",
                 static_cast<void*>(const_cast<ServiceRuntime*>(this)),
                 static_cast<void*>(const_cast<ScriptableHandle*>(
                     default_socket_))));

  return default_socket_;
}

ScriptableHandle* ServiceRuntime::GetSocketAddress(
    Plugin* plugin,
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
  imc_desc->Delete();
  PLUGIN_PRINTF(("ServiceRuntime::GetSocketAddress (return %p)\n",
                 static_cast<void*>(retval)));
  return retval;
}

}  // namespace plugin
