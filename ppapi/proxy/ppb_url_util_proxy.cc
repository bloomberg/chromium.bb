// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_util_proxy.h"

#include "base/basictypes.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/url_util_impl.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

namespace {

PP_Var Canonicalize(PP_Var url,
                    PP_URLComponents_Dev* components) {
  return URLUtilImpl::Canonicalize(0, url, components);
}

// Helper function for the functions below that optionally take a components
// structure. It's annoying to serialze the large PP_URLComponents structure
// and this data often isn't needed.
//
// To avoid this, we instead just parse the result again in the plugin, which
// this function does if the given URL is valid and the components are
// non-NULL. The URL var will be returned.
PP_Var ConvertComponentsAndReturnURL(PP_Var url,
                                     PP_URLComponents_Dev* components) {
  if (!components)
    return url;  // Common case - plugin doesn't care about parsing.

  StringVar* url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return url;

  PP_Var result = Canonicalize(url, components);
  PluginResourceTracker::GetInstance()->var_tracker().ReleaseVar(url);
  return result;
}

PP_Var ResolveRelativeToURL(PP_Var base_url,
                            PP_Var relative,
                            PP_URLComponents_Dev* components) {
  return URLUtilImpl::ResolveRelativeToURL(0, base_url, relative, components);
}

PP_Var ResolveRelativeToDocument(PP_Instance instance,
                                 PP_Var relative_string,
                                 PP_URLComponents_Dev* components) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeNull();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBURLUtil_ResolveRelativeToDocument(
      INTERFACE_ID_PPB_URL_UTIL, instance,
      SerializedVarSendInput(dispatcher, relative_string),
      &result));
  return ConvertComponentsAndReturnURL(result.Return(dispatcher), components);
}

PP_Bool IsSameSecurityOrigin(PP_Var url_a, PP_Var url_b) {
  return URLUtilImpl::IsSameSecurityOrigin(url_a, url_b);
}

PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBURLUtil_DocumentCanRequest(
      INTERFACE_ID_PPB_URL_UTIL, instance,
      SerializedVarSendInput(dispatcher, url),
      &result));
  return result;
}

PP_Bool DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(active);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBURLUtil_DocumentCanAccessDocument(
      INTERFACE_ID_PPB_URL_UTIL, active, target, &result));
  return result;
}

PP_Var GetDocumentURL(PP_Instance instance,
                      struct PP_URLComponents_Dev* components) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeNull();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBURLUtil_GetDocumentURL(
      INTERFACE_ID_PPB_URL_UTIL, instance, &result));
  return ConvertComponentsAndReturnURL(result.Return(dispatcher), components);
}

PP_Var GetPluginInstanceURL(PP_Instance instance,
                            struct PP_URLComponents_Dev* components) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeNull();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBURLUtil_GetPluginInstanceURL(
      INTERFACE_ID_PPB_URL_UTIL, instance, &result));
  return ConvertComponentsAndReturnURL(result.Return(dispatcher), components);
}

const PPB_URLUtil_Dev url_util_interface = {
  &Canonicalize,
  &ResolveRelativeToURL,
  &ResolveRelativeToDocument,
  &IsSameSecurityOrigin,
  &DocumentCanRequest,
  &DocumentCanAccessDocument,
  &GetDocumentURL,
  &GetPluginInstanceURL
};

InterfaceProxy* CreateURLUtilProxy(Dispatcher* dispatcher,
                                   const void* target_interface) {
  return new PPB_URLUtil_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_URLUtil_Proxy::PPB_URLUtil_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLUtil_Proxy::~PPB_URLUtil_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_URLUtil_Proxy::GetInfo() {
  static const Info info = {
    &url_util_interface,
    PPB_URLUTIL_DEV_INTERFACE,
    INTERFACE_ID_PPB_URL_UTIL,
    false,
    &CreateURLUtilProxy,
  };
  return &info;
}

bool PPB_URLUtil_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_URLUtil_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLUtil_ResolveRelativeToDocument,
                        OnMsgResolveRelativeToDocument)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLUtil_DocumentCanRequest,
                        OnMsgDocumentCanRequest)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLUtil_DocumentCanAccessDocument,
                        OnMsgDocumentCanAccessDocument)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLUtil_GetDocumentURL,
                        OnMsgGetDocumentURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLUtil_GetPluginInstanceURL,
                        OnMsgGetPluginInstanceURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_URLUtil_Proxy::OnMsgResolveRelativeToDocument(
    PP_Instance instance,
    SerializedVarReceiveInput relative,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_url_util_target()->ResolveRelativeToDocument(
                    instance, relative.Get(dispatcher()), NULL));
}

void PPB_URLUtil_Proxy::OnMsgDocumentCanRequest(PP_Instance instance,
                                                SerializedVarReceiveInput url,
                                                PP_Bool* result) {
  *result = ppb_url_util_target()->DocumentCanRequest(instance,
                                                      url.Get(dispatcher()));
}

void PPB_URLUtil_Proxy::OnMsgDocumentCanAccessDocument(PP_Instance active,
                                                       PP_Instance target,
                                                       PP_Bool* result) {
  *result = ppb_url_util_target()->DocumentCanAccessDocument(
      active, target);
}

void PPB_URLUtil_Proxy::OnMsgGetDocumentURL(PP_Instance instance,
                                            SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_url_util_target()->GetDocumentURL(instance, NULL));
}

void PPB_URLUtil_Proxy::OnMsgGetPluginInstanceURL(
    PP_Instance instance, SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_url_util_target()->GetPluginInstanceURL(instance, NULL));
}

}  // namespace proxy
}  // namespace ppapi

