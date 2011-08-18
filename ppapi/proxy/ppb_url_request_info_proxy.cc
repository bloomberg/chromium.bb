// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_request_info_proxy.h"

#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::PPB_URLRequestInfo_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateURLRequestInfoProxy(Dispatcher* dispatcher,
                                          const void* target_interface) {
  return new PPB_URLRequestInfo_Proxy(dispatcher, target_interface);
}

}  // namespace

class URLRequestInfo : public Resource,
                       public PPB_URLRequestInfo_API {
 public:
  URLRequestInfo(const HostResource& resource);
  virtual ~URLRequestInfo();

  virtual PPB_URLRequestInfo_API* AsPPB_URLRequestInfo_API() OVERRIDE;

  // PPB_URLRequestInfo_API implementation.
  virtual PP_Bool SetProperty(PP_URLRequestProperty property,
                              PP_Var var) OVERRIDE;
  virtual PP_Bool AppendDataToBody(const void* data, uint32_t len) OVERRIDE;
  virtual PP_Bool AppendFileToBody(
      PP_Resource file_ref,
      int64_t start_offset,
      int64_t number_of_bytes,
      PP_Time expected_last_modified_time) OVERRIDE;

 private:
  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  DISALLOW_COPY_AND_ASSIGN(URLRequestInfo);
};

URLRequestInfo::URLRequestInfo(const HostResource& resource)
    : Resource(resource) {
}

URLRequestInfo::~URLRequestInfo() {
}

PPB_URLRequestInfo_API* URLRequestInfo::AsPPB_URLRequestInfo_API() {
  return this;
}

PP_Bool URLRequestInfo::SetProperty(PP_URLRequestProperty property,
                                    PP_Var var) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBURLRequestInfo_SetProperty(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, host_resource(),
      static_cast<int32_t>(property),
      SerializedVarSendInput(GetDispatcher(), var)));

  // TODO(brettw) do some validation on the types. We should be able to tell on
  // the plugin side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

PP_Bool URLRequestInfo::AppendDataToBody(const void* data, uint32_t len) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBURLRequestInfo_AppendDataToBody(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, host_resource(),
      std::string(static_cast<const char*>(data), len)));

  // TODO(brettw) do some validation. We should be able to tell on the plugin
  // side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

PP_Bool URLRequestInfo::AppendFileToBody(PP_Resource file_ref,
                                         int64_t start_offset,
                                         int64_t number_of_bytes,
                                         PP_Time expected_last_modified_time) {
  Resource* file_ref_object =
      PluginResourceTracker::GetInstance()->GetResource(file_ref);
  if (!file_ref_object)
    return PP_FALSE;

  GetDispatcher()->Send(new PpapiHostMsg_PPBURLRequestInfo_AppendFileToBody(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, host_resource(),
      file_ref_object->host_resource(),
      start_offset, number_of_bytes, expected_last_modified_time));

  // TODO(brettw) do some validation. We should be able to tell on the plugin
  // side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

// PPB_URLRequestInfo_Proxy ----------------------------------------------------

PPB_URLRequestInfo_Proxy::PPB_URLRequestInfo_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLRequestInfo_Proxy::~PPB_URLRequestInfo_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_URLRequestInfo_Proxy::GetInfo() {
  static const Info info = {
    thunk::GetPPB_URLRequestInfo_Thunk(),
    PPB_URLREQUESTINFO_INTERFACE,
    INTERFACE_ID_PPB_URL_REQUEST_INFO,
    false,
    &CreateURLRequestInfoProxy,
  };
  return &info;
}

// static
PP_Resource PPB_URLRequestInfo_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_Create(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, instance, &result));
  if (result.is_null())
    return 0;
  return (new URLRequestInfo(result))->GetReference();
}

bool PPB_URLRequestInfo_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_URLRequestInfo_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLRequestInfo_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLRequestInfo_SetProperty,
                        OnMsgSetProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLRequestInfo_AppendDataToBody,
                        OnMsgAppendDataToBody)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLRequestInfo_AppendFileToBody,
                        OnMsgAppendFileToBody)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages.
  return handled;
}

void PPB_URLRequestInfo_Proxy::OnMsgCreate(
    PP_Instance instance,
    HostResource* result) {
  EnterFunctionNoLock<ResourceCreationAPI> enter(instance, true);
  if (enter.succeeded()) {
    result->SetHostResource(instance,
                            enter.functions()->CreateURLRequestInfo(instance));
  }
}

void PPB_URLRequestInfo_Proxy::OnMsgSetProperty(
    HostResource request,
    int32_t property,
    SerializedVarReceiveInput value) {
  EnterHostFromHostResource<PPB_URLRequestInfo_API> enter(request);
  if (enter.succeeded()) {
    enter.object()->SetProperty(static_cast<PP_URLRequestProperty>(property),
                                value.Get(dispatcher()));
  }
}

void PPB_URLRequestInfo_Proxy::OnMsgAppendDataToBody(
    HostResource request,
    const std::string& data) {
  EnterHostFromHostResource<PPB_URLRequestInfo_API> enter(request);
  if (enter.succeeded())
    enter.object()->AppendDataToBody(data.c_str(), data.size());
}

void PPB_URLRequestInfo_Proxy::OnMsgAppendFileToBody(
    HostResource request,
    HostResource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    double expected_last_modified_time) {
  EnterHostFromHostResource<PPB_URLRequestInfo_API> enter(request);
  if (enter.succeeded()) {
    enter.object()->AppendFileToBody(
        file_ref.host_resource(), start_offset, number_of_bytes,
        expected_last_modified_time);
  }
}

}  // namespace proxy
}  // namespace ppapi
