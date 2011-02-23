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

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Flash_NetConnector_Impl> connector(
      new PPB_Flash_NetConnector_Impl(instance));
  return connector->GetReference();
}

PP_Bool IsFlashNetConnector(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Flash_NetConnector_Impl>(resource));
}

int32_t ConnectTcp(PP_Resource connector_id,
                   const char* host,
                   uint16_t port,
                   PP_FileHandle* socket_out,
                   PP_Flash_NetAddress* local_addr_out,
                   PP_Flash_NetAddress* remote_addr_out,
                   PP_CompletionCallback callback) {
  scoped_refptr<PPB_Flash_NetConnector_Impl> connector(
      Resource::GetAs<PPB_Flash_NetConnector_Impl>(connector_id));
  if (!connector.get())
    return PP_ERROR_BADRESOURCE;

  return connector->ConnectTcp(
      host, port, socket_out, local_addr_out, remote_addr_out, callback);
}

int32_t ConnectTcpAddress(PP_Resource connector_id,
                          const PP_Flash_NetAddress* addr,
                          PP_FileHandle* socket_out,
                          PP_Flash_NetAddress* local_addr_out,
                          PP_Flash_NetAddress* remote_addr_out,
                          PP_CompletionCallback callback) {
  scoped_refptr<PPB_Flash_NetConnector_Impl> connector(
      Resource::GetAs<PPB_Flash_NetConnector_Impl>(connector_id));
  if (!connector.get())
    return PP_ERROR_BADRESOURCE;

  return connector->ConnectTcpAddress(
      addr, socket_out, local_addr_out, remote_addr_out, callback);
}

const PPB_Flash_NetConnector ppb_flash_netconnector = {
  &Create,
  &IsFlashNetConnector,
  &ConnectTcp,
  &ConnectTcpAddress,
};

}  // namespace

PPB_Flash_NetConnector_Impl::PPB_Flash_NetConnector_Impl(
    PluginInstance* instance)
    : Resource(instance) {
}

PPB_Flash_NetConnector_Impl::~PPB_Flash_NetConnector_Impl() {
}

// static
const PPB_Flash_NetConnector* PPB_Flash_NetConnector_Impl::GetInterface() {
  return &ppb_flash_netconnector;
}

PPB_Flash_NetConnector_Impl*
    PPB_Flash_NetConnector_Impl::AsPPB_Flash_NetConnector_Impl() {
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

  PP_Resource resource_id = GetReferenceNoAddRef();
  if (!resource_id) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  int32_t rv = instance()->delegate()->ConnectTcp(this, host, port);
  if (rv == PP_ERROR_WOULDBLOCK) {
    // Record callback and output buffers.
    callback_ = new TrackedCompletionCallback(
        instance()->module()->GetCallbackTracker(), resource_id, callback);
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

  PP_Resource resource_id = GetReferenceNoAddRef();
  if (!resource_id) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  int32_t rv = instance()->delegate()->ConnectTcpAddress(this, addr);
  if (rv == PP_ERROR_WOULDBLOCK) {
    // Record callback and output buffers.
    callback_ = new TrackedCompletionCallback(
        instance()->module()->GetCallbackTracker(), resource_id, callback);
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

