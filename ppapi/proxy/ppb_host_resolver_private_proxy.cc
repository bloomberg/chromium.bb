// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_host_resolver_private_proxy.h"

#include <cstddef>
#include <map>

#include "base/logging.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

namespace {

typedef std::map<uint32, PPB_HostResolver_Shared*> IDToHostResolverMap;
IDToHostResolverMap* g_id_to_host_resolver = NULL;

class HostResolver : public PPB_HostResolver_Shared {
 public:
  HostResolver(const HostResource& resource,
               uint32 plugin_dispatcher_id);
  virtual ~HostResolver();

  virtual void SendResolve(const HostPortPair& host_port,
                           const PP_HostResolver_Private_Hint* hint) OVERRIDE;

 private:
  void SendToBrowser(IPC::Message* msg);

  const uint32 plugin_dispatcher_id_;

  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

HostResolver::HostResolver(const HostResource& resource,
                           uint32 plugin_dispatcher_id)
    : PPB_HostResolver_Shared(resource),
      plugin_dispatcher_id_(plugin_dispatcher_id) {
  if (!g_id_to_host_resolver)
    g_id_to_host_resolver = new IDToHostResolverMap();
  DCHECK(g_id_to_host_resolver->find(host_resolver_id_) ==
         g_id_to_host_resolver->end());
  (*g_id_to_host_resolver)[host_resolver_id_] = this;
}

HostResolver::~HostResolver() {
  g_id_to_host_resolver->erase(host_resolver_id_);
}

void HostResolver::SendResolve(const HostPortPair& host_port,
                               const PP_HostResolver_Private_Hint* hint) {
  SendToBrowser(new PpapiHostMsg_PPBHostResolver_Resolve(
      API_ID_PPB_HOSTRESOLVER_PRIVATE,
      plugin_dispatcher_id_,
      host_resolver_id_,
      host_port,
      *hint));
}

void HostResolver::SendToBrowser(IPC::Message* msg) {
  PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(msg);
}

}  // namespace

//------------------------------------------------------------------------------

PPB_HostResolver_Private_Proxy::PPB_HostResolver_Private_Proxy(
    Dispatcher* dispatcher) : InterfaceProxy(dispatcher) {
}

PPB_HostResolver_Private_Proxy::~PPB_HostResolver_Private_Proxy() {
}

// static
PP_Resource PPB_HostResolver_Private_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResolver* host_resolver =
      new HostResolver(HostResource::MakeInstanceOnly(instance),
                       dispatcher->plugin_dispatcher_id());
  return host_resolver->GetReference();
}

bool PPB_HostResolver_Private_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_HostResolver_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBHostResolver_ResolveACK, OnMsgResolveACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_HostResolver_Private_Proxy::OnMsgResolveACK(
    uint32 plugin_dispatcher_id,
    uint32 host_resolver_id,
    bool succeeded,
    const std::string& canonical_name,
    const std::vector<PP_NetAddress_Private>& net_address_list) {
  if (!g_id_to_host_resolver) {
    NOTREACHED();
    return;
  }
  IDToHostResolverMap::iterator it =
      g_id_to_host_resolver->find(host_resolver_id);
  if (it == g_id_to_host_resolver->end())
    return;
  it->second->OnResolveCompleted(succeeded, canonical_name, net_address_list);
}

}  // namespace proxy
}  // namespace ppapi
