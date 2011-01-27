// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_request_info_proxy.h"

#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

class URLRequestInfo : public PluginResource {
 public:
  URLRequestInfo(const HostResource& resource) : PluginResource(resource) {
  }
  virtual ~URLRequestInfo() {
  }

  // Resource overrides.
  virtual URLRequestInfo* AsURLRequestInfo() { return this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestInfo);
};

namespace {

// Computes the dispatcher and request object for the given plugin resource,
// returning true on success.
bool DispatcherFromURLRequestInfo(PP_Resource resource,
                                  PluginDispatcher** dispatcher,
                                  URLRequestInfo** request_info) {
  *request_info = PluginResource::GetAs<URLRequestInfo>(resource);
  if (!*request_info)
    return false;
  *dispatcher = PluginDispatcher::GetForInstance((*request_info)->instance());
  return !!*dispatcher;
}

PP_Resource Create(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_Create(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, instance, &result));
  if (result.is_null())
    return 0;

  linked_ptr<URLRequestInfo> object(new URLRequestInfo(result));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  URLRequestInfo* object = PluginResource::GetAs<URLRequestInfo>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool SetProperty(PP_Resource request_id,
                    PP_URLRequestProperty property,
                    PP_Var var) {
  PluginDispatcher* dispatcher;
  URLRequestInfo* request_info;
  if (!DispatcherFromURLRequestInfo(request_id, &dispatcher, &request_info))
    return PP_FALSE;

  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_SetProperty(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, request_info->host_resource(),
      static_cast<int32_t>(property),
      SerializedVarSendInput(dispatcher, var)));

  // TODO(brettw) do some validation on the types. We should be able to tell on
  // the plugin side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

PP_Bool AppendDataToBody(PP_Resource request_id,
                         const char* data, uint32_t len) {
  PluginDispatcher* dispatcher;
  URLRequestInfo* request_info;
  if (!DispatcherFromURLRequestInfo(request_id, &dispatcher, &request_info))
    return PP_FALSE;

  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_AppendDataToBody(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, request_info->host_resource(),
      std::string(data, len)));

  // TODO(brettw) do some validation. We should be able to tell on the plugin
  // side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

PP_Bool AppendFileToBody(PP_Resource request_id,
                         PP_Resource file_ref_id,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  PluginDispatcher* dispatcher;
  URLRequestInfo* request_info;
  if (!DispatcherFromURLRequestInfo(request_id, &dispatcher, &request_info))
    return PP_FALSE;
  PluginResource* file_ref_object =
      PluginResourceTracker::GetInstance()->GetResourceObject(file_ref_id);
  if (!file_ref_object)
    return PP_FALSE;

  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_AppendFileToBody(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, request_info->host_resource(),
      file_ref_object->host_resource(),
      start_offset, number_of_bytes, expected_last_modified_time));

  // TODO(brettw) do some validation. We should be able to tell on the plugin
  // side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

const PPB_URLRequestInfo ppb_urlrequestinfo = {
  &Create,
  &IsURLRequestInfo,
  &SetProperty,
  &AppendDataToBody,
  &AppendFileToBody
};

}  // namespace

PPB_URLRequestInfo_Proxy::PPB_URLRequestInfo_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLRequestInfo_Proxy::~PPB_URLRequestInfo_Proxy() {
}

const void* PPB_URLRequestInfo_Proxy::GetSourceInterface() const {
  return &ppb_urlrequestinfo;
}

InterfaceID PPB_URLRequestInfo_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_URL_REQUEST_INFO;
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
  result->SetHostResource(instance,
                          ppb_url_request_info_target()->Create(instance));
}

void PPB_URLRequestInfo_Proxy::OnMsgSetProperty(
    HostResource request,
    int32_t property,
    SerializedVarReceiveInput value) {
  ppb_url_request_info_target()->SetProperty(request.host_resource(),
      static_cast<PP_URLRequestProperty>(property),
      value.Get(dispatcher()));
}

void PPB_URLRequestInfo_Proxy::OnMsgAppendDataToBody(
    HostResource request,
    const std::string& data) {
  ppb_url_request_info_target()->AppendDataToBody(request.host_resource(),
                                                  data.c_str(), data.size());
}

void PPB_URLRequestInfo_Proxy::OnMsgAppendFileToBody(
    HostResource request,
    HostResource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    double expected_last_modified_time) {
  ppb_url_request_info_target()->AppendFileToBody(
      request.host_resource(), file_ref.host_resource(),
      start_offset, number_of_bytes, expected_last_modified_time);
}

}  // namespace proxy
}  // namespace pp
