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


#include <signal.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/service_runtime_interface.h"
#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/trusted/plugin/srpc/video.h"

namespace nacl_srpc {


// Define all the static variables.
int ConnectedSocket::number_alive_counter = 0;
PLUGIN_JMPBUF ConnectedSocket::socket_env;

// ConnectedSocket implements a method for each method exported from
// the NaCl service runtime
bool ConnectedSocket::InvokeEx(uintptr_t method_id,
                               CallType call_type,
                               SrpcParams *params) {
  // All ConnectedSocket does for dynamic calls
  // is forward it to the SrpcClient object
  dprintf(("ConnectedSocket::InvokeEx()\n"));
  if (srpc_client_)
    return srpc_client_->Invoke(method_id, params);
  return PortableHandle::InvokeEx(method_id, call_type, params);
}

bool ConnectedSocket::HasMethodEx(uintptr_t method_id, CallType call_type) {
  if (srpc_client_ && (METHOD_CALL == call_type))
    return srpc_client_->HasMethod(method_id);
  return PortableHandle::HasMethodEx(method_id, call_type);
}

bool ConnectedSocket::InitParamsEx(uintptr_t method_id,
                                   CallType call_type,
                                   SrpcParams *params) {
  UNREFERENCED_PARAMETER(call_type);
  if (srpc_client_) {
    return srpc_client_->InitParams(method_id, params);
  }
  return false;
}

void ConnectedSocket::SignalHandler(int value) {
  dprintf(("ConnectedSocket::SignalHandler()\n"));

  PLUGIN_LONGJMP(socket_env, value);
}

NaClSrpcArg* ConnectedSocket::GetSignatureObject() {
  dprintf(("ConnectedSocket::GetSignatureObject(%p)\n",
           static_cast<void *>(this)));
  // Get the exported methods.
  return srpc_client_->GetSignatureObject();
}


bool ConnectedSocket::SignaturesGetProperty(void *obj, SrpcParams *params) {
  // TODO(gregoryd): is this still required?
  UNREFERENCED_PARAMETER(obj);
  UNREFERENCED_PARAMETER(params);
  return true;
}

void ConnectedSocket::LoadMethods() {
  // ConnectedSocket exports the signature property
  // TODO(sehr) - keep the following comment for documentation
  // - see previous version of this file
  // Safari apparently wants us to return false here.  This means programmers
  // will have to use a JavaScript function object to wrap NativeClient
  // methods they intend to invoke indirectly.

  PortableHandle::AddMethodToMap(SignaturesGetProperty,
                                 "__signatures",
                                 PROPERTY_GET, "", "");
}


bool ConnectedSocket::Init(
    struct PortableHandleInitializer* init_info) {
  nacl::VideoScopedGlobalLock video_lock;

  if (!DescBasedHandle::Init(init_info)) {
    dprintf(("ConnectedSocket::Init - DescBasedHandle::Init failed\n"));
    return false;
  }

  ConnectedSocketInitializer *socket_init_info =
      static_cast<ConnectedSocketInitializer*>(init_info);

  dprintf(("ConnectedSocket::Init(%p, %p, %d, %d, %p)\n",
           static_cast<void *>(socket_init_info->plugin_),
           static_cast<void *>(socket_init_info->wrapper_),
           socket_init_info->is_srpc_client_,
           socket_init_info->is_command_channel_,
           static_cast<void *>(socket_init_info->serv_rtm_info_)));

  service_runtime_info_ = socket_init_info->serv_rtm_info_;


  if (socket_init_info->is_srpc_client_) {
    // Get SRPC client interface going over socket.  Only the JavaScript main
    // channel may use proxied NPAPI (not the command channels).
    srpc_client_
        = new(std::nothrow) SrpcClient(!socket_init_info->is_command_channel_);
    if (NULL == srpc_client_) {
      // Return an error.
      // TODO(sehr): make sure that clients check for this as well.
      // BUG: This leaks socket.
      dprintf(("ConnectedSocket::Init -- new failed.\n"));
      return false;
    }
    if (!srpc_client_->Init(GetPortablePluginInterface(), this)) {
      delete srpc_client_;
      srpc_client_ = NULL;
      // BUG: This leaks socket.
      dprintf(("ConnectedSocket::Init -- SrpcClient::Init failed.\n"));
      return false;
    }

    // Prefetch the signatures for use by clients.
    signatures_ = GetSignatureObject();
    // only enable display on socket with service_runtime_info
    if (NULL != service_runtime_info_) {
      GetPortablePluginInterface()->video()->Enable();
    }
  } else {
    signatures_ = NULL;
  }
  LoadMethods();
  return true;
}

ConnectedSocket::ConnectedSocket()
  : signatures_(NULL),
    service_runtime_info_(NULL),
    srpc_client_(NULL) {
  dprintf(("ConnectedSocket::ConnectedSocket(%p, %d)\n",
           static_cast<void *>(this),
           ++number_alive_counter));
}

ConnectedSocket::~ConnectedSocket() {
  dprintf(("ConnectedSocket::~ConnectedSocket(%p, %d)\n",
           static_cast<void *>(this),
           --number_alive_counter));

  // Release the other NPAPI objects.
  if (signatures_) {
    FreeSrpcArg(signatures_);
    signatures_ = NULL;
  }

  // Free the SRPC connection.
  delete srpc_client_;
  //  Free the rpc method descriptors and terminate the connection to
  //  the service runtime instance.
  dprintf(("ConnectedSocket(%p): deleting SRI %p\n",
           static_cast<void *>(this),
           static_cast<void *>(service_runtime_info_)));

  if (service_runtime_info_) {
    delete service_runtime_info_;
  }
}

}  // namespace nacl_srpc
