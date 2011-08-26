// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_response_info_proxy.h"

#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_url_response_info_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_URLResponseInfo_API;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateURLResponseInfoProxy(Dispatcher* dispatcher,
                                           const void* target_interface) {
  return new PPB_URLResponseInfo_Proxy(dispatcher, target_interface);
}

}  // namespace

// URLResponseInfo -------------------------------------------------------------

class URLResponseInfo : public Resource, public PPB_URLResponseInfo_API {
 public:
  URLResponseInfo(const HostResource& resource);
  virtual ~URLResponseInfo();

  // Resource override.
  virtual PPB_URLResponseInfo_API* AsPPB_URLResponseInfo_API() OVERRIDE;

  // PPB_URLResponseInfo_API implementation.
  virtual PP_Var GetProperty(PP_URLResponseProperty property) OVERRIDE;
  virtual PP_Resource GetBodyAsFileRef() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLResponseInfo);
};

URLResponseInfo::URLResponseInfo(const HostResource& resource)
    : Resource(resource) {
}

URLResponseInfo::~URLResponseInfo() {
}

PPB_URLResponseInfo_API* URLResponseInfo::AsPPB_URLResponseInfo_API() {
  return this;
}

PP_Var URLResponseInfo::GetProperty(PP_URLResponseProperty property) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBURLResponseInfo_GetProperty(
      INTERFACE_ID_PPB_URL_RESPONSE_INFO, host_resource(), property, &result));
  return result.Return(dispatcher);
}

PP_Resource URLResponseInfo::GetBodyAsFileRef() {
  // This could be more efficient by having the host automatically send us the
  // file ref when the request is streaming to a file and it's in the state
  // where the file is ready. This will prevent us from having to do this sync
  // IPC here.
  PPB_FileRef_CreateInfo create_info;
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef(
          INTERFACE_ID_PPB_URL_RESPONSE_INFO, host_resource(), &create_info));
  return PPB_FileRef_Proxy::DeserializeFileRef(create_info);
}

// PPB_URLResponseInfo_Proxy ---------------------------------------------------

PPB_URLResponseInfo_Proxy::PPB_URLResponseInfo_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLResponseInfo_Proxy::~PPB_URLResponseInfo_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_URLResponseInfo_Proxy::GetInfo() {
  static const Info info = {
    ppapi::thunk::GetPPB_URLResponseInfo_Thunk(),
    PPB_URLRESPONSEINFO_INTERFACE,
    INTERFACE_ID_PPB_URL_RESPONSE_INFO,
    false,
    &CreateURLResponseInfoProxy,
  };
  return &info;
}

// static
PP_Resource PPB_URLResponseInfo_Proxy::CreateResponseForResource(
    const HostResource& resource) {
  return (new URLResponseInfo(resource))->GetReference();
}

bool PPB_URLResponseInfo_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_URLResponseInfo_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLResponseInfo_GetProperty,
                        OnMsgGetProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef,
                        OnMsgGetBodyAsFileRef)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages.
  return handled;
}

void PPB_URLResponseInfo_Proxy::OnMsgGetProperty(
    const HostResource& response,
    int32_t property,
    SerializedVarReturnValue result) {
  EnterHostFromHostResource<PPB_URLResponseInfo_API> enter(response);
  PP_Var result_var = PP_MakeUndefined();
  if (enter.succeeded()) {
    result_var = enter.object()->GetProperty(
        static_cast<PP_URLResponseProperty>(property));
  }
  result.Return(dispatcher(), result_var);
}

void PPB_URLResponseInfo_Proxy::OnMsgGetBodyAsFileRef(
    const HostResource& response,
    PPB_FileRef_CreateInfo* result) {
  EnterHostFromHostResource<PPB_URLResponseInfo_API> enter(response);
  PP_Resource file_ref = 0;
  if (enter.succeeded())
    file_ref = enter.object()->GetBodyAsFileRef();

  // Use the FileRef proxy to serialize.
  DCHECK(!dispatcher()->IsPlugin());
  HostDispatcher* host_disp = static_cast<HostDispatcher*>(dispatcher());
  PPB_FileRef_Proxy* file_ref_proxy = static_cast<PPB_FileRef_Proxy*>(
      host_disp->GetOrCreatePPBInterfaceProxy(INTERFACE_ID_PPB_FILE_REF));
  file_ref_proxy->SerializeFileRef(file_ref, result);
}

}  // namespace proxy
}  // namespace ppapi
