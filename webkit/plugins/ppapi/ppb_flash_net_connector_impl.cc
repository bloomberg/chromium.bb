// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ::ppapi::thunk::PPB_Flash_NetConnector_API;

namespace webkit {
namespace ppapi {

PPB_Flash_NetConnector_Impl::PPB_Flash_NetConnector_Impl(PP_Instance instance)
    : Resource(instance),
      socket_out_(NULL),
      local_addr_out_(NULL),
      remote_addr_out_(NULL) {
}

PPB_Flash_NetConnector_Impl::~PPB_Flash_NetConnector_Impl() {
}

PPB_Flash_NetConnector_API*
    PPB_Flash_NetConnector_Impl::AsPPB_Flash_NetConnector_API() {
  return this;
}

int32_t PPB_Flash_NetConnector_Impl::ConnectTcp(
    const char* host,
    uint16_t port,
    PP_FileHandle* socket_out,
    PP_Flash_NetAddress* local_addr_out,
    PP_Flash_NetAddress* remote_addr_out,
    PP_CompletionCallback callback) {
  // |socket_out| is not optional.
  if (!socket_out)
    return PP_ERROR_BADARGUMENT;

  if (!callback.func) {
    NOTIMPLEMENTED();
    return PP_ERROR_BADARGUMENT;
  }

  if (callback_.get() && !callback_->completed())
    return PP_ERROR_INPROGRESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return false;
  int32_t rv = plugin_instance->delegate()->ConnectTcp(this, host, port);
  if (rv == PP_OK_COMPLETIONPENDING) {
    // Record callback and output buffers.
    callback_ = new TrackedCompletionCallback(
        plugin_instance->module()->GetCallbackTracker(),
        pp_resource(), callback);
    socket_out_ = socket_out;
    local_addr_out_ = local_addr_out;
    remote_addr_out_ = remote_addr_out;
  } else {
    // This should never be completed synchronously successfully.
    DCHECK_NE(rv, PP_OK);
  }
  return rv;
}

int32_t PPB_Flash_NetConnector_Impl::ConnectTcpAddress(
    const PP_Flash_NetAddress* addr,
    PP_FileHandle* socket_out,
    PP_Flash_NetAddress* local_addr_out,
    PP_Flash_NetAddress* remote_addr_out,
    PP_CompletionCallback callback) {
  // |socket_out| is not optional.
  if (!socket_out)
    return PP_ERROR_BADARGUMENT;

  if (!callback.func) {
    NOTIMPLEMENTED();
    return PP_ERROR_BADARGUMENT;
  }

  if (callback_.get() && !callback_->completed())
    return PP_ERROR_INPROGRESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return false;
  int32_t rv = plugin_instance->delegate()->ConnectTcpAddress(this, addr);
  if (rv == PP_OK_COMPLETIONPENDING) {
    // Record callback and output buffers.
    callback_ = new TrackedCompletionCallback(
        plugin_instance->module()->GetCallbackTracker(),
        pp_resource(), callback);
    socket_out_ = socket_out;
    local_addr_out_ = local_addr_out;
    remote_addr_out_ = remote_addr_out;
  } else {
    // This should never be completed synchronously successfully.
    DCHECK_NE(rv, PP_OK);
  }
  return rv;
}

void PPB_Flash_NetConnector_Impl::CompleteConnectTcp(
    PP_FileHandle socket,
    const PP_Flash_NetAddress& local_addr,
    const PP_Flash_NetAddress& remote_addr) {
  int32_t rv = PP_ERROR_ABORTED;
  if (!callback_->aborted()) {
    CHECK(!callback_->completed());

    // Write output data.
    *socket_out_ = socket;
    if (socket != PP_kInvalidFileHandle) {
      if (local_addr_out_)
        *local_addr_out_ = local_addr;
      if (remote_addr_out_)
        *remote_addr_out_ = remote_addr;
      rv = PP_OK;
    } else {
      rv = PP_ERROR_FAILED;
    }
  }

  // Theoretically, the plugin should be allowed to try another |ConnectTcp()|
  // from the callback.
  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(callback_);
  // Wipe everything else out for safety.
  socket_out_ = NULL;
  local_addr_out_ = NULL;
  remote_addr_out_ = NULL;

  callback->Run(rv);  // Will complete abortively if necessary.
}

}  // namespace ppapi
}  // namespace webkit

