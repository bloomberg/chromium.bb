/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <signal.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/trusted/plugin/npapi/video.h"

namespace {

PLUGIN_JMPBUF socket_env;

void SignalHandler(int value) {
  PLUGIN_PRINTF(("ConnectedSocket::SignalHandler()\n"));

  PLUGIN_LONGJMP(socket_env, value);
}

}  // namespace

namespace plugin {

// ConnectedSocket implements a method for each method exported from
// the NaCl service runtime
bool ConnectedSocket::InvokeEx(uintptr_t method_id,
                               CallType call_type,
                               SrpcParams *params) {
  // All ConnectedSocket does for dynamic calls
  // is forward it to the SrpcClient object
  PLUGIN_PRINTF(("ConnectedSocket::InvokeEx()\n"));
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

ConnectedSocket* ConnectedSocket::New(Plugin* plugin,
                                      nacl::DescWrapper* desc,
                                      bool can_use_proxied_npapi) {
  PLUGIN_PRINTF(("ConnectedSocket::New()\n"));

  ConnectedSocket* connected_socket = new(std::nothrow) ConnectedSocket();

  if (connected_socket == NULL ||
      !connected_socket->Init(plugin, desc, can_use_proxied_npapi)) {
    // Ok to delete if NULL.
    delete connected_socket;
    return NULL;
  }

  return connected_socket;
}

bool ConnectedSocket::Init(Plugin* plugin,
                           nacl::DescWrapper* wrapper,
                           bool can_use_proxied_npapi) {
  // TODO(sehr): this lock seems like it should be movable to PluginNpapi.
  VideoScopedGlobalLock video_lock;

  if (!DescBasedHandle::Init(plugin, wrapper)) {
    PLUGIN_PRINTF(("ConnectedSocket::Init - DescBasedHandle::Init failed\n"));
    return false;
  }

  PLUGIN_PRINTF(("ConnectedSocket::Init(%p, %p, %d)\n",
                 static_cast<void*>(plugin),
                 static_cast<void*>(wrapper),
                 can_use_proxied_npapi));

  // Get SRPC client interface going over socket.  Only the JavaScript main
  // channel may use proxied NPAPI (not the command channels).
  srpc_client_ = new(std::nothrow) SrpcClient(can_use_proxied_npapi);
  if (NULL == srpc_client_) {
    // Return an error.
    // TODO(sehr): make sure that clients check for this as well.
    // BUG: This leaks socket.
    PLUGIN_PRINTF(("ConnectedSocket::Init -- new failed.\n"));
    return false;
  }
  if (!srpc_client_->Init(browser_interface(), this)) {
    delete srpc_client_;
    srpc_client_ = NULL;
    // BUG: This leaks socket.
    PLUGIN_PRINTF(("ConnectedSocket::Init -- SrpcClient::Init failed.\n"));
    return false;
  }

  // Only enable video on socket with service_runtime.
  if (can_use_proxied_npapi) {
    plugin->EnableVideo();
  }
  return true;
}

ConnectedSocket::ConnectedSocket()
  : srpc_client_(NULL) {
  PLUGIN_PRINTF(("ConnectedSocket::ConnectedSocket(%p)\n",
                 static_cast<void*>(this)));
}

ConnectedSocket::~ConnectedSocket() {
  PLUGIN_PRINTF(("ConnectedSocket::~ConnectedSocket(%p)\n",
                 static_cast<void*>(this)));

  // Free the SRPC connection.
  delete srpc_client_;
}

}  // namespace plugin
