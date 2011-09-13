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

namespace ppapi {
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
          dispatcher->local_get_interface()(PPB_FULLSCREEN_DEV_INTERFACE));
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

PP_Bool HandleDocumentLoad(PP_Instance instance,
                           PP_Resource url_loader) {
  PP_Bool result = PP_FALSE;
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);

  // Set up the URLLoader for proxying.

  PPB_URLLoader_Proxy* url_loader_proxy = static_cast<PPB_URLLoader_Proxy*>(
      dispatcher->GetInterfaceProxy(INTERFACE_ID_PPB_URL_LOADER));
  url_loader_proxy->PrepareURLLoaderForSendingToPlugin(url_loader);

  // PluginResourceTracker in the plugin process assumes that resources that it
  // tracks have been addrefed on behalf of the plugin at the renderer side. So
  // we explicitly do it for |url_loader| here.
  //
  // Please also see comments in PPP_Instance_Proxy::OnMsgHandleDocumentLoad()
  // about releasing of this extra reference.
  const PPB_Core* core = reinterpret_cast<const PPB_Core*>(
      dispatcher->local_get_interface()(PPB_CORE_INTERFACE));
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

static const PPP_Instance_1_0 instance_interface_1_0 = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleDocumentLoad
};

InterfaceProxy* CreateInstanceProxy(Dispatcher* dispatcher) {
  return new PPP_Instance_Proxy(dispatcher);
}

}  // namespace

PPP_Instance_Proxy::PPP_Instance_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
  if (dispatcher->IsPlugin()) {
    combined_interface_.reset(
        new PPP_Instance_Combined(*static_cast<const PPP_Instance_1_0*>(
            dispatcher->local_get_interface()(PPP_INSTANCE_INTERFACE_1_0))));
  }
}

PPP_Instance_Proxy::~PPP_Instance_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_Instance_Proxy::GetInfo1_0() {
  static const Info info = {
    &instance_interface_1_0,
    PPP_INSTANCE_INTERFACE_1_0,
    INTERFACE_ID_PPP_INSTANCE,
    false,
    &CreateInstanceProxy
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
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_HandleDocumentLoad,
                        OnMsgHandleDocumentLoad)
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
  ppapi::TrackerBase::Get()->GetResourceTracker()->DidCreateInstance(instance);

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
  ppapi::TrackerBase::Get()->GetResourceTracker()->DidDeleteInstance(instance);
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

}  // namespace proxy
}  // namespace ppapi
