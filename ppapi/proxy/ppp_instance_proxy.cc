// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_instance_proxy.h"

#include <algorithm>

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
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
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  const PPB_Fullscreen_Dev* fullscreen_interface =
      static_cast<const PPB_Fullscreen_Dev*>(
          dispatcher->GetLocalInterface(PPB_FULLSCREEN_DEV_INTERFACE));
  DCHECK(fullscreen_interface);
  PP_Bool fullscreen = fullscreen_interface->IsFullscreen(instance);
  dispatcher->Send(
      new PpapiMsg_PPPInstance_DidChangeView(INTERFACE_ID_PPP_INSTANCE,
                                             instance, *position, *clip,
                                             fullscreen));
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
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);

  // Set up the URLLoader for proxying.

  PPB_URLLoader_Proxy* url_loader_proxy = static_cast<PPB_URLLoader_Proxy*>(
      dispatcher->GetOrCreatePPBInterfaceProxy(INTERFACE_ID_PPB_URL_LOADER));
  url_loader_proxy->PrepareURLLoaderForSendingToPlugin(url_loader);

  // PluginResourceTracker in the plugin process assumes that resources that it
  // tracks have been addrefed on behalf of the plugin at the renderer side. So
  // we explicitly do it for |url_loader| here.
  //
  // Please also see comments in PPP_Instance_Proxy::OnMsgHandleDocumentLoad()
  // about releasing of this extra reference.
  const PPB_Core* core = reinterpret_cast<const PPB_Core*>(
      dispatcher->GetLocalInterface(PPB_CORE_INTERFACE));
  if (!core) {
    NOTREACHED();
    return PP_FALSE;
  }
  core->AddRefResource(url_loader);

  HostResource serialized_loader;
  serialized_loader.SetHostResource(instance, url_loader);
  dispatcher->Send(new PpapiMsg_PPPInstance_HandleDocumentLoad(
      INTERFACE_ID_PPP_INSTANCE, instance, serialized_loader, &result));
  return result;
}

PP_Var GetInstanceObject(PP_Instance instance) {
  Dispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiMsg_PPPInstance_GetInstanceObject(
      INTERFACE_ID_PPP_INSTANCE, instance, &result));
  return result.Return(dispatcher);
}

static const PPP_Instance_0_4 instance_interface_0_4 = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleInputEvent,
  &HandleDocumentLoad,
  &GetInstanceObject
};

static const PPP_Instance_0_5 instance_interface_0_5 = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleInputEvent,
  &HandleDocumentLoad
};

template <class PPP_Instance_Type>
InterfaceProxy* CreateInstanceProxy(Dispatcher* dispatcher,
                                    const void* target_interface) {
  return new PPP_Instance_Proxy(
      dispatcher,
      static_cast<const PPP_Instance_Type*>(target_interface));
}

}  // namespace

PPP_Instance_Proxy::~PPP_Instance_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_Instance_Proxy::GetInfo0_4() {
  static const Info info = {
    &instance_interface_0_4,
    PPP_INSTANCE_INTERFACE_0_4,
    INTERFACE_ID_PPP_INSTANCE,
    false,
    &CreateInstanceProxy<PPP_Instance_0_4>
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPP_Instance_Proxy::GetInfo0_5() {
  static const Info info = {
    &instance_interface_0_5,
    PPP_INSTANCE_INTERFACE_0_5,
    INTERFACE_ID_PPP_INSTANCE,
    false,
    &CreateInstanceProxy<PPP_Instance_0_5>,
  };
  return &info;
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

  // Set up the routing associating this new instance with the dispatcher we
  // just got the message from. This must be done before calling into the
  // plugin so it can in turn call PPAPI functions.
  PluginDispatcher* plugin_dispatcher =
      static_cast<PluginDispatcher*>(dispatcher());
  plugin_dispatcher->DidCreateInstance(instance);

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

  DCHECK(combined_interface_.get());
  *result = combined_interface_->DidCreate(instance,
                                           static_cast<uint32_t>(argn.size()),
                                           &argn_array[0], &argv_array[0]);
}

void PPP_Instance_Proxy::OnMsgDidDestroy(PP_Instance instance) {
  combined_interface_->DidDestroy(instance);
  static_cast<PluginDispatcher*>(dispatcher())->DidDestroyInstance(instance);
}

void PPP_Instance_Proxy::OnMsgDidChangeView(PP_Instance instance,
                                            const PP_Rect& position,
                                            const PP_Rect& clip,
                                            PP_Bool fullscreen) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  InstanceData* data = dispatcher->GetInstanceData(instance);
  if (!data)
    return;
  data->position = position;
  data->fullscreen = fullscreen;
  combined_interface_->DidChangeView(instance, &position, &clip);
}

void PPP_Instance_Proxy::OnMsgDidChangeFocus(PP_Instance instance,
                                             PP_Bool has_focus) {
  combined_interface_->DidChangeFocus(instance, has_focus);
}

void PPP_Instance_Proxy::OnMsgHandleInputEvent(PP_Instance instance,
                                               const PP_InputEvent& event,
                                               PP_Bool* result) {
  *result = combined_interface_->HandleInputEvent(instance, &event);
}

void PPP_Instance_Proxy::OnMsgHandleDocumentLoad(PP_Instance instance,
                                                 const HostResource& url_loader,
                                                 PP_Bool* result) {
  PP_Resource plugin_loader =
      PPB_URLLoader_Proxy::TrackPluginResource(url_loader);
  *result = combined_interface_->HandleDocumentLoad(instance, plugin_loader);

  // This balances the one reference that TrackPluginResource() initialized it
  // with. The plugin will normally take an additional reference which will keep
  // the resource alive in the plugin (and the one reference in the renderer
  // representing all plugin references).
  // Once all references at the plugin side are released, the renderer side will
  // be notified and release the reference added in HandleDocumentLoad() above.
  PluginResourceTracker::GetInstance()->ReleaseResource(plugin_loader);
}

void PPP_Instance_Proxy::OnMsgGetInstanceObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  // GetInstanceObject_0_4 can be null if we're talking to version 0.5 or later,
  // however the host side of the proxy should never call this function on an
  // 0.5 or later version.
  DCHECK(combined_interface_->GetInstanceObject_0_4);
  result.Return(dispatcher(),
                combined_interface_->GetInstanceObject_0_4(instance));
}

}  // namespace proxy
}  // namespace pp
