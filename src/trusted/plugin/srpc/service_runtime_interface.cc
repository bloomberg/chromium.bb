/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <map>

#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/multimedia_socket.h"
#include "native_client/src/trusted/plugin/srpc/service_runtime_interface.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"
#include "native_client/src/trusted/plugin/srpc/socket_address.h"
#include "native_client/src/trusted/plugin/srpc/srt_socket.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

namespace nacl_srpc {

int ServiceRuntimeInterface::number_alive_counter = 0;

ServiceRuntimeInterface::ServiceRuntimeInterface(
    PortablePluginInterface* plugin_interface,
    Plugin* plugin) :
    plugin_interface_(plugin_interface),
    default_socket_address_(NULL),
    default_socket_(NULL),
    plugin_(plugin),
    runtime_channel_(NULL),
    multimedia_channel_(NULL),
    subprocess_(NULL) {
}

bool ServiceRuntimeInterface::InitCommunication(const void* buffer,
                                                int32_t size) {
  // TODO(sehr): this should use the new
  // SelLdrLauncher::OpenSrpcChannels interface, which should be free
  // of resource leaks.
  // Channel5 was opened to communicate with the sel_ldr instance.
  // Get the first IMC message and create a socket address from it.
  NaClHandle channel5 = subprocess_->channel();
  dprintf(("ServiceRuntimeInterface(%p): opened %p 0x%p\n",
           static_cast<void *>(this), static_cast<void *>(subprocess_),
           reinterpret_cast<void *>(channel5)));
  // GetSocketAddress implicitly invokes Close(channel5).
  default_socket_address_ = GetSocketAddress(plugin_, channel5);
  dprintf(("ServiceRuntimeInterface::Start: "
           "Got service channel descriptor %p\n",
           static_cast<void *>(default_socket_address_)));

  ScriptableHandle<ConnectedSocket> *raw_channel;
  // The first connect on the socket address returns the service
  // runtime command channel.  This channel is created before the NaCl
  // module runs, and is private to the service runtime.  This channel
  // is used for a secure plugin<->service runtime communication path
  // that can be used to forcibly shut down the sel_ldr.
  dprintf((" connecting for SrtSocket\n"));
  SocketAddress *real_socket_address =
      static_cast<SocketAddress*>(default_socket_address_->get_handle());
  raw_channel = real_socket_address->Connect(NULL);
  if (NULL == raw_channel) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "service runtime Connect failed.\n"));
    return false;
  }
  dprintf((" constructing SrtSocket\n"));
  runtime_channel_ = new(std::nothrow) SrtSocket(raw_channel,
                                                 plugin_interface_);
  if (NULL == runtime_channel_) {
    // BUG: leaking raw_channel.
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

  dprintf(("invoking set_origin\n"));
  if (!runtime_channel_->SetOrigin(plugin_->nacl_module_origin())) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "set_orign RPC failed.\n"));
    // BUG: leaking raw_channel and runtime_channel_.
    return false;
  }

  if (buffer != NULL) {
    // We now have an open communication channel to the sel_ldr process,
    // so we can send the nexe bits over
    SharedMemoryInitializer init_info(plugin_interface_, plugin_, size);
    ScriptableHandle<SharedMemory> *shared_memory =
      ScriptableHandle<SharedMemory>::New(&init_info);

    SharedMemory *real_shared_memory =
      static_cast<SharedMemory*>(shared_memory->get_handle());
    // TODO(gregoryd): another option is to export a Write() function
    // from SharedMemory
    memcpy(real_shared_memory->buffer(), buffer, size);


    if (!runtime_channel_->LoadModule(real_shared_memory->desc())) {
      dprintf(("ServiceRuntimeInterface::Start failed to send nexe\n"));
      shared_memory->Unref();
      // TODO(gregoryd): close communication channels
      delete subprocess_;
      subprocess_ = NULL;
      return false;
    }

    shared_memory->Unref();
  }

  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status;

  dprintf((" invoking start_module RPC\n"));
  if (!runtime_channel_->StartModule(&load_status)) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "module_start RPC failed.\n"));
    // BUG: leaking raw_channel and runtime_channel_.
    return false;
  }
  dprintf((" start_module returned %d\n", load_status));
  if (LOAD_OK != load_status) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "module load status %d",
            load_status));
    // BUG: leaking raw_channel and runtime_channel_.
    return false;
  }

  // The second connect on the socket address returns the "untrusted
  // command channel" to the NaCl app, usually libsrpc -- see
  // accept_loop's srpc_init.  This channel is "privileged" in that
  // the srpc_default_acceptor knows that this is from the plugin, and
  // allows some RPCs that might otherwise not be, e.g., to
  // (gracefully) shut down the NaCl module.  NB: the default
  // srpc_shutdown_request handler is not (yet) very graceful.
  dprintf((" connecting for MultiMediaSocket\n"));
  raw_channel = real_socket_address->Connect(NULL);
  if (NULL == raw_channel) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "command channel Connect failed.\n"));
    // BUG: leaking runtime_channel_.
    return false;
  }
  dprintf((" constructing MultiMediaSocket\n"));
  multimedia_channel_ = new(std::nothrow) MultimediaSocket(raw_channel,
                                                           plugin_interface_,
                                                           this);
  if (NULL == multimedia_channel_) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "MultimediaSocket channel construction failed.\n"));
    // BUG: leaking raw_channel and runtime_channel_.
    return false;
  }

  dprintf((" connecting for javascript SRPC use\n"));
  // The third connect on the socket address returns the channel used to
  // perform SRPC calls.
  default_socket_ = real_socket_address->Connect(this);
  if (NULL == default_socket_) {
    dprintf(("ServiceRuntimeInterface::Start: "
            "SRPC channel Connect failed.\n"));
    // BUG: leaking raw_channel, runtime_channel_, and multimedia_channel_.
    return false;
  }

  // Initialize the multimedia system, if necessary.
  dprintf((" initializing multimedia subsystem\n"));
  if (!multimedia_channel_->InitializeModuleMultimedia(plugin_)) {
    // BUG: leaking raw_channel, runtime_channel_, multimedia_channel_,
    // and default_socket_.
    dprintf(("ServiceRuntimeInterface::Start: "
            "InitializeModuleMultimedia failed.\n"));
    return false;
  }
  return true;
}

bool ServiceRuntimeInterface::Start(const char* nacl_file) {
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

  // NB: number_alive is intentionally modified only when
  // SRPC_PLUGIN_DEBUG is enabled.
  dprintf(("ServiceRuntimeInterface::ServiceRuntimeInterface(%p, %p, %s, %d)\n",
           static_cast<void *>(this),
           static_cast<void *>(plugin()),
           nacl_file,
           ++number_alive_counter));

  subprocess_ = new(std::nothrow) nacl::SelLdrLauncher();
  if (NULL == subprocess_) {
    return false;
  }
  if (!subprocess_->Start(const_cast<char*>(nacl_file),
                          5,
                          kSelLdrArgLength,
                          kSelLdrArgs,
                          0,
                          NULL)) {
    delete subprocess_;
    subprocess_ = NULL;
    return false;
  }

  // TODO(gregoryd) - this should deal with buffer and size correctly
  // - do we need to send the load command from here?
  if (!InitCommunication(NULL, 0)) {
    return false;
  }

  dprintf(("ServiceRuntimeInterface::Start was successful\n"));
  return true;
}

bool ServiceRuntimeInterface::Start(const char* url,
                                    const void* buffer,
                                    int32_t size) {
  subprocess_ = new(std::nothrow) nacl::SelLdrLauncher();
  if (NULL == subprocess_) {
    return false;
  }
  if (!subprocess_->Start(url, 5)) {
      delete subprocess_;
      subprocess_ = NULL;
      return false;
  }

  if (!InitCommunication(buffer, size)) {
    return false;
  }

  dprintf(("ServiceRuntimeInterface::Start was successful\n"));
  return true;
}

bool ServiceRuntimeInterface::Kill() {
  return subprocess_->KillChild();
}

bool ServiceRuntimeInterface::LogAtServiceRuntime(int severity,
                                                  std::string msg) {
  return runtime_channel_->Log(severity, msg);
}

ServiceRuntimeInterface::~ServiceRuntimeInterface() {
  dprintf(("ServiceRuntimeInterface::~ServiceRuntimeInterface(%p, %d)\n",
           static_cast<void *>(this), --number_alive_counter));

  dprintf(("ServiceRuntimeInterface::~ServiceRuntimeInterface:"
    " deleting multimedia_channel_\n"));
  delete multimedia_channel_;
  delete runtime_channel_;

  // ServiceRuntimeInterfaces are always shut down from the default
  // Connected socket, so we don't delete that memory.
  dprintf(("ServiceRuntimeInterface::~ServiceRuntimeInterface:"
           " deleting subprocess_\n"));
  delete subprocess_;
  dprintf(("ServiceRuntimeInterface: shut down sel_ldr.\n"));
}

ScriptableHandle<SocketAddress>*
    ServiceRuntimeInterface::default_socket_address() const {
  dprintf(("ServiceRuntimeInterface::default_socket_address(%p) = %p\n",
           static_cast<void *>(
               const_cast<ServiceRuntimeInterface *>(this)),
           static_cast<void *>(const_cast<ScriptableHandle<SocketAddress>*>(
               default_socket_address_))));

  return default_socket_address_;
}

ScriptableHandle<ConnectedSocket>*
    ServiceRuntimeInterface::default_socket() const {
  dprintf(("ServiceRuntimeInterface::default_socket(%p) = %p\n",
           static_cast<void *>(
               const_cast<ServiceRuntimeInterface *>(this)),
           static_cast<void *>(const_cast<ScriptableHandle<ConnectedSocket>*>(
               default_socket_))));

  return default_socket_;
}

ScriptableHandle<SocketAddress>* ServiceRuntimeInterface::GetSocketAddress(
    Plugin* plugin,
    NaClHandle channel) {
  NaClImcTypedMsgHdr  header;
  NaClImcMsgIoVec     iovec[1];
  unsigned char       bytes[NACL_ABI_IMC_USER_BYTES_MAX];
  NaClDesc*           descs[NACL_ABI_IMC_USER_DESC_MAX];
  NaClNrdXferEffector eff;

  dprintf(("ServiceRuntimeInterface::GetSocketAddress(%p, %p)\n",
           static_cast<void *>(plugin),
           reinterpret_cast<void *>(channel)));

  ScriptableHandle<SocketAddress>* retval = NULL;
  // Create a NaClDesc to use for message passing over channel.
  struct NaClDesc* desc(reinterpret_cast<struct NaClDesc*>(
                            malloc(sizeof(struct NaClDescImcDesc))));

  if (NULL == desc) {
    dprintf(("ServiceRuntimeInterface::GetSocketAddress: malloc failed\n"));
    goto cleanup;
  }
  // desc takes ownership of channel
  if (!NaClDescImcDescCtor(reinterpret_cast<struct NaClDescImcDesc*>(desc),
                           channel)) {
    dprintf(("ServiceRuntimeInterface::GetSocketAddress: "
             "NaClDescImcDescCtor failed\n"));
    free(desc);
    desc = NULL;
    goto cleanup;
  }
  channel = NACL_INVALID_HANDLE;
  // Set up the effector to read the message.
  // TODO(bsy): using desc here is wrong, but doesn't really matter.
  if (!NaClNrdXferEffectorCtor(&eff, desc)) {
    dprintf(("ServiceRuntimeInterface::GetSocketAddress: "
             "NaClNrdXferEffectorCtor failed\n"));
    goto cleanup;
  }
  // Set up to receive a message.
  iovec[0].base = bytes;
  iovec[0].length = NACL_ABI_IMC_USER_BYTES_MAX;
  header.iov = iovec;
  header.iov_length = NACL_ARRAY_SIZE(iovec);
  header.ndescv = descs;
  header.ndesc_length = NACL_ARRAY_SIZE(descs);
  header.flags = 0;
  // Receive the message.
  NaClImcRecvTypedMessage(desc,
                          reinterpret_cast<NaClDescEffector*>(&eff),
                          &header,
                          0);
  // Check that there was exactly one descriptor passed.
  if (1 != header.ndesc_length) {
    dprintf(("ServiceRuntimeInterface::GetSocketAddress: "
             "message receive failed %u %u\n",
             (unsigned) header.ndesc_length, (unsigned) header.iov_length));
    goto cleanup_eff;
  }
  dprintf(("ServiceRuntimeInterface::GetSocketAddress: "
           "-X result descriptor (NaClDesc*) = %p\n",
           static_cast<void *>(descs[0])));
  {
    SocketAddressInitializer init_info(plugin_interface_, descs[0], plugin);
    retval = ScriptableHandle<SocketAddress>::New(
        static_cast<PortableHandleInitializer*>(&init_info));
  }
  // TODO(bsy): use NaClDescObj and scoped_ptr when NaClDescObj is
  // ready, and get rid of the gotos.
 cleanup_eff:
  dprintf((" dtoring the effector\n"));
  (*eff.base.vtbl->Dtor)(reinterpret_cast<struct NaClDescEffector *>(&eff));
 cleanup:
  if (NULL != desc) {
    NaClDescUnref(desc);
  }
  if (NACL_INVALID_HANDLE != channel) {
    dprintf((" NaClClose(handle = %p\n",
             reinterpret_cast<void *>(channel)));
    NaClClose(channel);
  }
  dprintf((" returning %p\n", static_cast<void *>(retval)));
  return retval;
}

bool ServiceRuntimeInterface::Shutdown() {
  if (NULL == getenv("NACLTEST_DISABLE_SHUTDOWN")) {
    dprintf(("ServiceRuntimeInterface::Shutdown: shutting down\n"));
    if (NULL != runtime_channel_) {
      return runtime_channel_->HardShutdown();
    } else {
      dprintf((" NULL runtime_channel_\n"));
      return true;
    }
  } else {
    dprintf(("ServiceRuntimeInterface::Shutdown: shutdown disabled\n"));
    return true;  // lie.  this is for testing.
  }
}

}  // namespace nacl_srpc
