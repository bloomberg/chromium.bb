// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_instance_proxy.h"

#include <algorithm>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_url_loader_proxy.h"

namespace pp {
namespace proxy {

namespace {

PP_Bool DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[]) {
  std::vector<std::string> argn_vect;
  std::vector<std::string> argv_vect;
  for (uint32_t i = 0; i < argc; i++) {
    argn_vect.push_back(std::string(argn[i]));
    argv_vect.push_back(std::string(argv[i]));
  }

  PP_Bool result = PP_FALSE;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidCreate(INTERFACE_ID_PPP_INSTANCE, instance,
                                         argn_vect, argv_vect, &result));
  return result;
}

void DidDestroy(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidDestroy(INTERFACE_ID_PPP_INSTANCE, instance));
}

void DidChangeView(PP_Instance instance,
                   const PP_Rect* position,
                   const PP_Rect* clip) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidChangeView(INTERFACE_ID_PPP_INSTANCE,
                                             instance, *position, *clip));
}

void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidChangeFocus(INTERFACE_ID_PPP_INSTANCE,
                                              instance, has_focus));
}

PP_Bool HandleInputEvent(PP_Instance instance,
                         const PP_InputEvent* event) {
  PP_Bool result = PP_FALSE;
  IPC::Message* msg = new PpapiMsg_PPPInstance_HandleInputEvent(
      INTERFACE_ID_PPP_INSTANCE, instance, *event, &result);
  // Make this message not unblock, to avoid re-entrancy problems when the
  // plugin does a synchronous call to the renderer. This will force any
  // synchronous calls from the plugin to complete before processing this
  // message. We avoid deadlock by never un-setting the unblock flag on messages
  // from the plugin to the renderer.
  msg->set_unblock(false);
  HostDispatcher::GetForInstance(instance)->Send(msg);
  return result;
}

PP_Bool HandleDocumentLoad(PP_Instance instance,
                           PP_Resource url_loader) {
  PP_Bool result = PP_FALSE;
  HostResource serialized_loader;
  serialized_loader.SetHostResource(instance, url_loader);
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_HandleDocumentLoad(INTERFACE_ID_PPP_INSTANCE,
                                                  instance, serialized_loader,
                                                  &result));
  return result;
}

PP_Var GetInstanceObject(PP_Instance instance) {
  Dispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiMsg_PPPInstance_GetInstanceObject(
      INTERFACE_ID_PPP_INSTANCE, instance, &result));
  return result.Return(dispatcher);
}

static const PPP_Instance instance_interface = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleInputEvent,
  &HandleDocumentLoad,
  &GetInstanceObject
};

}  // namespace

PPP_Instance_Proxy::PPP_Instance_Proxy(Dispatcher* dispatcher,
                                       const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPP_Instance_Proxy::~PPP_Instance_Proxy() {
}

const void* PPP_Instance_Proxy::GetSourceInterface() const {
  return &instance_interface;
}

InterfaceID PPP_Instance_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPP_INSTANCE;
}

bool PPP_Instance_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Instance_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidCreate,
                        OnMsgDidCreate)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidDestroy,
                        OnMsgDidDestroy)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidChangeView,
                        OnMsgDidChangeView)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidChangeFocus,
                        OnMsgDidChangeFocus)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_HandleInputEvent,
                        OnMsgHandleInputEvent)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_HandleDocumentLoad,
                        OnMsgHandleDocumentLoad)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_GetInstanceObject,
                        OnMsgGetInstanceObject)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Instance_Proxy::OnMsgDidCreate(
    PP_Instance instance,
    const std::vector<std::string>& argn,
    const std::vector<std::string>& argv,
    PP_Bool* result) {
  *result = PP_FALSE;
  if (argn.size() != argv.size())
    return;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  dispatcher->DidCreateInstance(instance);

  // Make sure the arrays always have at least one element so we can take the
  // address below.
  std::vector<const char*> argn_array(
      std::max(static_cast<size_t>(1), argn.size()));
  std::vector<const char*> argv_array(
      std::max(static_cast<size_t>(1), argn.size()));
  for (size_t i = 0; i < argn.size(); i++) {
    argn_array[i] = argn[i].c_str();
    argv_array[i] = argv[i].c_str();
  }

  DCHECK(ppp_instance_target());
  *result = ppp_instance_target()->DidCreate(instance,
                                             static_cast<uint32_t>(argn.size()),
                                             &argn_array[0], &argv_array[0]);
  DCHECK(*result);
}

void PPP_Instance_Proxy::OnMsgDidDestroy(PP_Instance instance) {
  ppp_instance_target()->DidDestroy(instance);
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;

  dispatcher->DidDestroyInstance(instance);
}

void PPP_Instance_Proxy::OnMsgDidChangeView(PP_Instance instance,
                                            const PP_Rect& position,
                                            const PP_Rect& clip) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  InstanceData* data = dispatcher->GetInstanceData(instance);
  if (!data)
    return;
  data->position = position;
  ppp_instance_target()->DidChangeView(instance, &position, &clip);
}

void PPP_Instance_Proxy::OnMsgDidChangeFocus(PP_Instance instance,
                                             PP_Bool has_focus) {
  ppp_instance_target()->DidChangeFocus(instance, has_focus);
}

void PPP_Instance_Proxy::OnMsgHandleInputEvent(PP_Instance instance,
                                               const PP_InputEvent& event,
                                               PP_Bool* result) {
  *result = ppp_instance_target()->HandleInputEvent(instance, &event);
}

void PPP_Instance_Proxy::OnMsgHandleDocumentLoad(PP_Instance instance,
                                                 const HostResource& url_loader,
                                                 PP_Bool* result) {
  PP_Resource plugin_loader =
      PPB_URLLoader_Proxy::TrackPluginResource(url_loader);
  *result = ppp_instance_target()->HandleDocumentLoad(
      instance, plugin_loader);
}

void PPP_Instance_Proxy::OnMsgGetInstanceObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppp_instance_target()->GetInstanceObject(instance));
}

}  // namespace proxy
}  // namespace pp
