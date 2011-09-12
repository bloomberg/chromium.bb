// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_proxy.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_module.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace proxy {

namespace {

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, PP_Bool on_top) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(pp_instance);
  if (dispatcher) {
    dispatcher->Send(new PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop(
        INTERFACE_ID_PPB_FLASH, pp_instance, on_top));
  }
}

PP_Bool DrawGlyphs(PP_Instance instance,
                   PP_Resource pp_image_data,
                   const PP_FontDescription_Dev* font_desc,
                   uint32_t color,
                   PP_Point position,
                   PP_Rect clip,
                   const float transformation[3][3],
                   uint32_t glyph_count,
                   const uint16_t glyph_indices[],
                   const PP_Point glyph_advances[]) {
  Resource* image_data = PluginResourceTracker::GetInstance()->GetResource(
      pp_image_data);
  if (!image_data)
    return PP_FALSE;
  // The instance parameter isn't strictly necessary but we check that it
  // matches anyway.
  if (image_data->pp_instance() != instance)
    return PP_FALSE;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      image_data->pp_instance());
  if (!dispatcher)
    return PP_FALSE;

  PPBFlash_DrawGlyphs_Params params;
  params.image_data = image_data->host_resource();
  params.font_desc.SetFromPPFontDescription(dispatcher, *font_desc, true);
  params.color = color;
  params.position = position;
  params.clip = clip;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++)
      params.transformation[i][j] = transformation[i][j];
  }

  params.glyph_indices.insert(params.glyph_indices.begin(),
                              &glyph_indices[0],
                              &glyph_indices[glyph_count]);
  params.glyph_advances.insert(params.glyph_advances.begin(),
                               &glyph_advances[0],
                               &glyph_advances[glyph_count]);

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBFlash_DrawGlyphs(
      INTERFACE_ID_PPB_FLASH, params, &result));
  return result;
}

PP_Var GetProxyForURL(PP_Instance instance, const char* url) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBFlash_GetProxyForURL(
      INTERFACE_ID_PPB_FLASH, instance, url, &result));
  return result.Return(dispatcher);
}

int32_t Navigate(PP_Resource request_id,
                 const char* target,
                 bool from_user_action) {
  thunk::EnterResource<thunk::PPB_URLRequestInfo_API> enter(request_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PP_Instance instance = enter.resource()->pp_instance();

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlash_Navigate(
      INTERFACE_ID_PPB_FLASH,
      instance, enter.object()->GetData(), target, from_user_action,
      &result));
  return result;
}

void RunMessageLoop(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  IPC::SyncMessage* msg = new PpapiHostMsg_PPBFlash_RunMessageLoop(
      INTERFACE_ID_PPB_FLASH, instance);
  msg->EnableMessagePumping();
  dispatcher->Send(msg);
}

void QuitMessageLoop(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  dispatcher->Send(new PpapiHostMsg_PPBFlash_QuitMessageLoop(
      INTERFACE_ID_PPB_FLASH, instance));
}

double GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0.0;

  // TODO(brettw) on Windows it should be possible to do the time calculation
  // in-process since it doesn't need to read files on disk. This will improve
  // performance.
  //
  // On Linux, it would be better to go directly to the browser process for
  // this message rather than proxy it through some instance in a renderer.
  double result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset(
      INTERFACE_ID_PPB_FLASH, instance, t, &result));
  return result;
}

PP_Var GetCommandLineArgs(PP_Module pp_module) {
  const PPB_Var_Deprecated* var_deprecated =
      static_cast<const PPB_Var_Deprecated*>(
          PluginDispatcher::GetInterfaceFromDispatcher(
              PPB_VAR_DEPRECATED_INTERFACE));
  std::string args = ProxyModule::GetInstance()->GetFlashCommandLineArgs();
  return var_deprecated->VarFromUtf8(pp_module, args.data(), args.length());
}

const PPB_Flash flash_interface = {
  &SetInstanceAlwaysOnTop,
  &DrawGlyphs,
  &GetProxyForURL,
  &Navigate,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLocalTimeZoneOffset,
  &GetCommandLineArgs
};

InterfaceProxy* CreateFlashProxy(Dispatcher* dispatcher,
                                 const void* target_interface) {
  return new PPB_Flash_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Flash_Proxy::PPB_Flash_Proxy(Dispatcher* dispatcher,
                                 const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Flash_Proxy::~PPB_Flash_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_Proxy::GetInfo() {
  static const Info info = {
    &flash_interface,
    PPB_FLASH_INTERFACE,
    INTERFACE_ID_PPB_FLASH,
    true,
    &CreateFlashProxy,
  };
  return &info;
}

bool PPB_Flash_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // Prevent the dispatcher from going away during a call to Navigate.
  // This must happen OUTSIDE of OnMsgNavigate since the handling code use
  // the dispatcher upon return of the function (sending the reply message).
  ScopedModuleReference death_grip(dispatcher());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop,
                        OnMsgSetInstanceAlwaysOnTop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_DrawGlyphs,
                        OnMsgDrawGlyphs)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetProxyForURL,
                        OnMsgGetProxyForURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_Navigate, OnMsgNavigate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_RunMessageLoop,
                        OnMsgRunMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_QuitMessageLoop,
                        OnMsgQuitMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset,
                        OnMsgGetLocalTimeZoneOffset)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_Proxy::OnMsgSetInstanceAlwaysOnTop(
    PP_Instance instance,
    PP_Bool on_top) {
  ppb_flash_target()->SetInstanceAlwaysOnTop(instance, on_top);
}

void PPB_Flash_Proxy::OnMsgDrawGlyphs(const PPBFlash_DrawGlyphs_Params& params,
                                      PP_Bool* result) {
  *result = PP_FALSE;

  PP_FontDescription_Dev font_desc;
  params.font_desc.SetToPPFontDescription(dispatcher(), &font_desc, false);

  if (params.glyph_indices.size() != params.glyph_advances.size() ||
      params.glyph_indices.empty())
    return;

  *result = ppb_flash_target()->DrawGlyphs(
      0,  // Unused instance param.
      params.image_data.host_resource(), &font_desc,
      params.color, params.position, params.clip,
      const_cast<float(*)[3]>(params.transformation),
      static_cast<uint32_t>(params.glyph_indices.size()),
      const_cast<uint16_t*>(&params.glyph_indices[0]),
      const_cast<PP_Point*>(&params.glyph_advances[0]));
}

void PPB_Flash_Proxy::OnMsgGetProxyForURL(PP_Instance instance,
                                          const std::string& url,
                                          SerializedVarReturnValue result) {
  result.Return(dispatcher(), ppb_flash_target()->GetProxyForURL(
      instance, url.c_str()));
}

void PPB_Flash_Proxy::OnMsgNavigate(PP_Instance instance,
                                    const PPB_URLRequestInfo_Data& data,
                                    const std::string& target,
                                    bool from_user_action,
                                    int32_t* result) {
  DCHECK(!dispatcher()->IsPlugin());

  // Validate the PP_Instance since we'll be constructing resources on its
  // behalf.
  HostDispatcher* host_dispatcher = static_cast<HostDispatcher*>(dispatcher());
  if (HostDispatcher::GetForInstance(instance) != host_dispatcher) {
    NOTREACHED();
    *result = PP_ERROR_BADARGUMENT;
    return;
  }

  // We need to allow re-entrancy here, because this may call into Javascript
  // (e.g. with a "javascript:" URL), or do things like navigate away from the
  // page, either one of which will need to re-enter into the plugin.
  // It is safe, because it is essentially equivalent to NPN_GetURL, where Flash
  // would expect re-entrancy. When running in-process, it does re-enter here.
  host_dispatcher->set_allow_plugin_reentrancy();

  // Make a temporary request resource.
  thunk::EnterFunctionNoLock<thunk::ResourceCreationAPI> enter(instance, true);
  if (enter.failed()) {
    *result = PP_ERROR_FAILED;
    return;
  }
  ScopedPPResource request_resource(
      ScopedPPResource::PassRef(),
      enter.functions()->CreateURLRequestInfo(instance, data));

  *result = ppb_flash_target()->Navigate(request_resource,
                                         target.c_str(),
                                         from_user_action);
}

void PPB_Flash_Proxy::OnMsgRunMessageLoop(PP_Instance instance) {
  ppb_flash_target()->RunMessageLoop(instance);
}

void PPB_Flash_Proxy::OnMsgQuitMessageLoop(PP_Instance instance) {
  ppb_flash_target()->QuitMessageLoop(instance);
}

void PPB_Flash_Proxy::OnMsgGetLocalTimeZoneOffset(PP_Instance instance,
                                                  PP_Time t,
                                                  double* result) {
  *result = ppb_flash_target()->GetLocalTimeZoneOffset(instance, t);
}

}  // namespace proxy
}  // namespace ppapi
