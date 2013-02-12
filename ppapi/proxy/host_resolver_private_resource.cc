// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_resolver_private_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

HostResolverPrivateResource::HostResolverPrivateResource(Connection connection,
                                                         PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_HostResolverPrivate_Create());
}

HostResolverPrivateResource::~HostResolverPrivateResource() {
}

thunk::PPB_HostResolver_Private_API*
HostResolverPrivateResource::AsPPB_HostResolver_Private_API() {
  return this;
}

int32_t HostResolverPrivateResource::Resolve(
    const char* host,
    uint16_t port,
    const PP_HostResolver_Private_Hint* hint,
    scoped_refptr<TrackedCallback> callback) {
  if (!host)
    return PP_ERROR_BADARGUMENT;
  if (ResolveInProgress())
    return PP_ERROR_INPROGRESS;

  resolve_callback_ = callback;

  HostPortPair host_port;
  host_port.host = host;
  host_port.port = port;

  SendResolve(host_port, hint);
  return PP_OK_COMPLETIONPENDING;
}

PP_Var HostResolverPrivateResource::GetCanonicalName() {
  return StringVar::StringToPPVar(canonical_name_);
}

uint32_t HostResolverPrivateResource::GetSize() {
  if (ResolveInProgress())
    return 0;
  return static_cast<uint32_t>(net_address_list_.size());
}

bool HostResolverPrivateResource::GetNetAddress(
    uint32 index,
    PP_NetAddress_Private* address) {
  if (ResolveInProgress() || index >= GetSize())
    return false;
  *address = net_address_list_[index];
  return true;
}

void HostResolverPrivateResource::OnPluginMsgResolveReply(
    const ResourceMessageReplyParams& params,
    const std::string& canonical_name,
    const std::vector<PP_NetAddress_Private>& net_address_list) {
  if (params.result() == PP_OK) {
    canonical_name_ = canonical_name;
    net_address_list_ = net_address_list;
  } else {
    canonical_name_.clear();
    net_address_list_.clear();
  }
  resolve_callback_->Run(params.result());
}

void HostResolverPrivateResource::SendResolve(
    const HostPortPair& host_port,
    const PP_HostResolver_Private_Hint* hint) {
  PpapiHostMsg_HostResolverPrivate_Resolve msg(host_port, *hint);
  Call<PpapiPluginMsg_HostResolverPrivate_ResolveReply>(
      BROWSER,
      msg,
      base::Bind(&HostResolverPrivateResource::OnPluginMsgResolveReply,
                 base::Unretained(this)));
}

bool HostResolverPrivateResource::ResolveInProgress() const {
  return TrackedCallback::IsPending(resolve_callback_);
}

}  // namespace proxy
}  // namespace ppapi
