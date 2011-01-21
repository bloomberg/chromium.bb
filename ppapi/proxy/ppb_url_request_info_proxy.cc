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
  URLRequestInfo(PP_Instance instance) : PluginResource(instance) {}
  virtual ~URLRequestInfo() {}

  // Resource overrides.
  virtual URLRequestInfo* AsURLRequestInfo() { return this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestInfo);
};

namespace {

// Returns the dispatcher associated with the given URLRequestInfo, or NULL if
// none exists.
PluginDispatcher* DispatcherFromURLRequestInfo(PP_Resource resource) {
  URLRequestInfo* object = PluginResource::GetAs<URLRequestInfo>(resource);
  if (!object)
    return NULL;
  return PluginDispatcher::GetForInstance(object->instance());
}

PP_Resource Create(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  PP_Resource result;
  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_Create(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, instance, &result));
  if (result) {
    linked_ptr<URLRequestInfo> object(new URLRequestInfo(instance));
    PluginResourceTracker::GetInstance()->AddResource(result, object);
  }
  return result;
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  URLRequestInfo* object = PluginResource::GetAs<URLRequestInfo>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool SetProperty(PP_Resource request_id,
                    PP_URLRequestProperty property,
                    PP_Var var) {
  PluginDispatcher* dispatcher = DispatcherFromURLRequestInfo(request_id);
  if (!dispatcher)
    return PP_FALSE;

  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_SetProperty(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, request_id,
      static_cast<int32_t>(property),
      SerializedVarSendInput(dispatcher, var)));

  // TODO(brettw) do some validation on the types. We should be able to tell on
  // the plugin side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

PP_Bool AppendDataToBody(PP_Resource request_id,
                         const char* data, uint32_t len) {
  PluginDispatcher* dispatcher = DispatcherFromURLRequestInfo(request_id);
  if (!dispatcher)
    return PP_FALSE;

  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_AppendDataToBody(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, request_id, std::string(data, len)));

  // TODO(brettw) do some validation. We should be able to tell on the plugin
  // side whether the request will succeed or fail in the renderer.
  return PP_TRUE;
}

PP_Bool AppendFileToBody(PP_Resource request_id,
                         PP_Resource file_ref_id,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  PluginDispatcher* dispatcher = DispatcherFromURLRequestInfo(request_id);
  if (!dispatcher)
    return PP_FALSE;

  dispatcher->Send(new PpapiHostMsg_PPBURLRequestInfo_AppendFileToBody(
      INTERFACE_ID_PPB_URL_REQUEST_INFO, request_id, file_ref_id,
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
    PP_Resource* result) {
  *result = ppb_url_request_info_target()->Create(instance);
}

void PPB_URLRequestInfo_Proxy::OnMsgSetProperty(
    PP_Resource request,
    int32_t property,
    SerializedVarReceiveInput value) {
  ppb_url_request_info_target()->SetProperty(request,
      static_cast<PP_URLRequestProperty>(property),
      value.Get(dispatcher()));
}

void PPB_URLRequestInfo_Proxy::OnMsgAppendDataToBody(
    PP_Resource request,
    const std::string& data) {
  ppb_url_request_info_target()->AppendDataToBody(request, data.c_str(),
                                                  data.size());
}

void PPB_URLRequestInfo_Proxy::OnMsgAppendFileToBody(
    PP_Resource request,
    PP_Resource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    double expected_last_modified_time) {
  ppb_url_request_info_target()->AppendFileToBody(
      request, file_ref, start_offset, number_of_bytes,
      expected_last_modified_time);
}

}  // namespace proxy
}  // namespace pp
