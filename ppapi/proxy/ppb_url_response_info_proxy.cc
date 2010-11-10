// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_response_info_proxy.h"

#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

class URLResponseInfo : public PluginResource {
 public:
  URLResponseInfo() {}
  virtual ~URLResponseInfo() {}

  // Resource overrides.
  virtual URLResponseInfo* AsURLResponseInfo() { return this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(URLResponseInfo);
};

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource) {
  URLResponseInfo* object = PluginResource::GetAs<URLResponseInfo>(resource);
  return BoolToPPBool(!!object);
}

PP_Var GetProperty(PP_Resource response, PP_URLResponseProperty property) {
  Dispatcher* dispatcher = PluginDispatcher::Get();
  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBURLResponseInfo_GetProperty(
      INTERFACE_ID_PPB_URL_RESPONSE_INFO, response, property, &result));
  return result.Return(dispatcher);
}

PP_Resource GetBodyAsFileRef(PP_Resource response) {
  PP_Resource result = 0;
  /*
  Dispatcher* dispatcher = PluginDispatcher::Get();
  dispatcher->Send(new PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef(
      INTERFACE_ID_PPB_URL_RESPONSE_INFO, response, &result));
  // TODO(brettw) when we have FileRef proxied, make an object from that
  // ref so we can track it properly and then uncomment this.
  */
  return result;
}

const PPB_URLResponseInfo ppb_urlresponseinfo = {
  &IsURLResponseInfo,
  &GetProperty,
  &GetBodyAsFileRef
};

}  // namespace

PPB_URLResponseInfo_Proxy::PPB_URLResponseInfo_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLResponseInfo_Proxy::~PPB_URLResponseInfo_Proxy() {
}

// static
void PPB_URLResponseInfo_Proxy::TrackPluginResource(
    PP_Resource response_resource) {
  linked_ptr<URLResponseInfo> object(new URLResponseInfo);
  PluginDispatcher::Get()->plugin_resource_tracker()->AddResource(
      response_resource, object);
}

const void* PPB_URLResponseInfo_Proxy::GetSourceInterface() const {
  return &ppb_urlresponseinfo;
}

InterfaceID PPB_URLResponseInfo_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_URL_RESPONSE_INFO;
}

void PPB_URLResponseInfo_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_URLResponseInfo_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLResponseInfo_GetProperty,
                        OnMsgGetProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef,
                        OnMsgGetBodyAsFileRef)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages.
}

void PPB_URLResponseInfo_Proxy::OnMsgGetProperty(
    PP_Resource response,
    int32_t property,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(), ppb_url_response_info_target()->GetProperty(
      response, static_cast<PP_URLResponseProperty>(property)));
}

void PPB_URLResponseInfo_Proxy::OnMsgGetBodyAsFileRef(
    PP_Resource response,
    PP_Resource* file_ref_result) {
  *file_ref_result = ppb_url_response_info_target()->GetBodyAsFileRef(response);
}

}  // namespace proxy
}  // namespace pp
