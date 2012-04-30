// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_module.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"

using ppapi::thunk::EnterInstanceNoLock;

namespace ppapi {
namespace proxy {

PPB_Flash_Proxy::PPB_Flash_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_Proxy::~PPB_Flash_Proxy() {
}

bool PPB_Flash_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // Prevent the dispatcher from going away during a call to Navigate.
  // This must happen OUTSIDE of OnMsgNavigate since the handling code use
  // the dispatcher upon return of the function (sending the reply message).
  ScopedModuleReference death_grip(dispatcher());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop,
                        OnHostMsgSetInstanceAlwaysOnTop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_DrawGlyphs,
                        OnHostMsgDrawGlyphs)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetProxyForURL,
                        OnHostMsgGetProxyForURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_Navigate, OnHostMsgNavigate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_RunMessageLoop,
                        OnHostMsgRunMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_QuitMessageLoop,
                        OnHostMsgQuitMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset,
                        OnHostMsgGetLocalTimeZoneOffset)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_IsRectTopmost,
                        OnHostMsgIsRectTopmost)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_FlashSetFullscreen,
                        OnHostMsgFlashSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_FlashGetScreenSize,
                        OnHostMsgFlashGetScreenSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_IsClipboardFormatAvailable,
                        OnHostMsgIsClipboardFormatAvailable)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_ReadClipboardData,
                        OnHostMsgReadClipboardData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_WriteClipboardData,
                        OnHostMsgWriteClipboardData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_Proxy::SetInstanceAlwaysOnTop(PP_Instance instance,
                                             PP_Bool on_top) {
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop(
      API_ID_PPB_FLASH, instance, on_top));
}

PP_Bool PPB_Flash_Proxy::DrawGlyphs(PP_Instance instance,
                                    PP_Resource pp_image_data,
                                    const PP_FontDescription_Dev* font_desc,
                                    uint32_t color,
                                    const PP_Point* position,
                                    const PP_Rect* clip,
                                    const float transformation[3][3],
                                    PP_Bool allow_subpixel_aa,
                                    uint32_t glyph_count,
                                    const uint16_t glyph_indices[],
                                    const PP_Point glyph_advances[]) {
  Resource* image_data =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(pp_image_data);
  if (!image_data)
    return PP_FALSE;
  // The instance parameter isn't strictly necessary but we check that it
  // matches anyway.
  if (image_data->pp_instance() != instance)
    return PP_FALSE;

  PPBFlash_DrawGlyphs_Params params;
  params.image_data = image_data->host_resource();
  params.font_desc.SetFromPPFontDescription(dispatcher(), *font_desc, true);
  params.color = color;
  params.position = *position;
  params.clip = *clip;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++)
      params.transformation[i][j] = transformation[i][j];
  }
  params.allow_subpixel_aa = allow_subpixel_aa;

  params.glyph_indices.insert(params.glyph_indices.begin(),
                              &glyph_indices[0],
                              &glyph_indices[glyph_count]);
  params.glyph_advances.insert(params.glyph_advances.begin(),
                               &glyph_advances[0],
                               &glyph_advances[glyph_count]);

  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_DrawGlyphs(
      API_ID_PPB_FLASH, instance, params, &result));
  return result;
}

PP_Var PPB_Flash_Proxy::GetProxyForURL(PP_Instance instance, const char* url) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetProxyForURL(
      API_ID_PPB_FLASH, instance, url, &result));
  return result.Return(dispatcher());
}

int32_t PPB_Flash_Proxy::Navigate(PP_Instance instance,
                                  PP_Resource request_info,
                                  const char* target,
                                  PP_Bool from_user_action) {
  thunk::EnterResourceNoLock<thunk::PPB_URLRequestInfo_API> enter(
      request_info, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  int32_t result = PP_ERROR_FAILED;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_Navigate(
      API_ID_PPB_FLASH,
      instance, enter.object()->GetData(), target, from_user_action,
      &result));
  return result;
}

void PPB_Flash_Proxy::RunMessageLoop(PP_Instance instance) {
  IPC::SyncMessage* msg = new PpapiHostMsg_PPBFlash_RunMessageLoop(
      API_ID_PPB_FLASH, instance);
  msg->EnableMessagePumping();
  dispatcher()->Send(msg);
}

void PPB_Flash_Proxy::QuitMessageLoop(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_QuitMessageLoop(
      API_ID_PPB_FLASH, instance));
}

double PPB_Flash_Proxy::GetLocalTimeZoneOffset(PP_Instance instance,
                                               PP_Time t) {
  // TODO(brettw) on Windows it should be possible to do the time calculation
  // in-process since it doesn't need to read files on disk. This will improve
  // performance.
  //
  // On Linux, it would be better to go directly to the browser process for
  // this message rather than proxy it through some instance in a renderer.
  double result = 0;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset(
      API_ID_PPB_FLASH, instance, t, &result));
  return result;
}

PP_Bool PPB_Flash_Proxy::IsRectTopmost(PP_Instance instance,
                                       const PP_Rect* rect) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_IsRectTopmost(
      API_ID_PPB_FLASH, instance, *rect, &result));
  return result;
}

int32_t PPB_Flash_Proxy::InvokePrinting(PP_Instance instance) {
  return PP_ERROR_NOTSUPPORTED;
}

void PPB_Flash_Proxy::UpdateActivity(PP_Instance instance) {
  PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(
      new PpapiHostMsg_PPBFlash_UpdateActivity(API_ID_PPB_FLASH));
}

PP_Var PPB_Flash_Proxy::GetDeviceID(PP_Instance instance) {
  std::string id;
  PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(
      new PpapiHostMsg_PPBFlash_GetDeviceID(API_ID_PPB_FLASH, &id));
  return StringVar::StringToPPVar(id);
}

PP_Bool PPB_Flash_Proxy::IsClipboardFormatAvailable(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!IsValidClipboardType(clipboard_type) || !IsValidClipboardFormat(format))
    return PP_FALSE;

  bool result = false;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_IsClipboardFormatAvailable(
      API_ID_PPB_FLASH,
      instance,
      static_cast<int>(clipboard_type),
      static_cast<int>(format),
      &result));
  return PP_FromBool(result);
}

PP_Var PPB_Flash_Proxy::ReadClipboardData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!IsValidClipboardType(clipboard_type) || !IsValidClipboardFormat(format))
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_ReadClipboardData(
      API_ID_PPB_FLASH, instance,
      static_cast<int>(clipboard_type), static_cast<int>(format), &result));
  return result.Return(dispatcher());
}

int32_t PPB_Flash_Proxy::WriteClipboardData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    uint32_t data_item_count,
    const PP_Flash_Clipboard_Format formats[],
    const PP_Var data_items[]) {
  if (!IsValidClipboardType(clipboard_type))
    return PP_ERROR_BADARGUMENT;

  std::vector<SerializedVar> data_items_vector;
  SerializedVarSendInput::ConvertVector(
      dispatcher(),
      data_items,
      data_item_count,
      &data_items_vector);
  for (size_t i = 0; i < data_item_count; ++i) {
    if (!IsValidClipboardFormat(formats[i]))
      return PP_ERROR_BADARGUMENT;
  }

  std::vector<int> formats_vector(formats, formats + data_item_count);
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_WriteClipboardData(
      API_ID_PPB_FLASH,
      instance,
      static_cast<int>(clipboard_type),
      formats_vector,
      data_items_vector));
  // Assume success, since it allows us to avoid a sync IPC.
  return PP_OK;
}

PP_Bool PPB_Flash_Proxy::FlashIsFullscreen(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_FALSE;
  return data->flash_fullscreen;
}

PP_Bool PPB_Flash_Proxy::FlashSetFullscreen(PP_Instance instance,
                                            PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_FlashSetFullscreen(
      API_ID_PPB_FLASH, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Flash_Proxy::FlashGetScreenSize(PP_Instance instance,
                                            PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_FlashGetScreenSize(
      API_ID_PPB_FLASH, instance, &result, size));
  return result;
}

void PPB_Flash_Proxy::OnHostMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                                      PP_Bool on_top) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->SetInstanceAlwaysOnTop(instance, on_top);
}

void PPB_Flash_Proxy::OnHostMsgDrawGlyphs(
    PP_Instance instance,
    const PPBFlash_DrawGlyphs_Params& params,
    PP_Bool* result) {
  *result = PP_FALSE;
  EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return;

  PP_FontDescription_Dev font_desc;
  params.font_desc.SetToPPFontDescription(dispatcher(), &font_desc, false);

  if (params.glyph_indices.size() != params.glyph_advances.size() ||
      params.glyph_indices.empty())
    return;

  *result = enter.functions()->GetFlashAPI()->DrawGlyphs(
      0,  // Unused instance param.
      params.image_data.host_resource(), &font_desc,
      params.color, &params.position, &params.clip,
      const_cast<float(*)[3]>(params.transformation),
      params.allow_subpixel_aa,
      static_cast<uint32_t>(params.glyph_indices.size()),
      const_cast<uint16_t*>(&params.glyph_indices[0]),
      const_cast<PP_Point*>(&params.glyph_advances[0]));
}

void PPB_Flash_Proxy::OnHostMsgGetProxyForURL(PP_Instance instance,
                                              const std::string& url,
                                              SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetFlashAPI()->GetProxyForURL(
                      instance, url.c_str()));
  } else {
    result.Return(dispatcher(), PP_MakeUndefined());
  }
}

void PPB_Flash_Proxy::OnHostMsgNavigate(PP_Instance instance,
                                        const PPB_URLRequestInfo_Data& data,
                                        const std::string& target,
                                        PP_Bool from_user_action,
                                        int32_t* result) {
  EnterInstanceNoLock enter_instance(instance);
  if (enter_instance.failed()) {
    *result = PP_ERROR_BADARGUMENT;
    return;
  }
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
  thunk::EnterResourceCreation enter(instance);
  if (enter.failed()) {
    *result = PP_ERROR_FAILED;
    return;
  }
  ScopedPPResource request_resource(
      ScopedPPResource::PassRef(),
      enter.functions()->CreateURLRequestInfo(instance, data));

  *result = enter_instance.functions()->GetFlashAPI()->Navigate(
      instance, request_resource, target.c_str(), from_user_action);
}

void PPB_Flash_Proxy::OnHostMsgRunMessageLoop(PP_Instance instance) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->RunMessageLoop(instance);
}

void PPB_Flash_Proxy::OnHostMsgQuitMessageLoop(PP_Instance instance) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->QuitMessageLoop(instance);
}

void PPB_Flash_Proxy::OnHostMsgGetLocalTimeZoneOffset(PP_Instance instance,
                                                  PP_Time t,
                                                  double* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->GetLocalTimeZoneOffset(
        instance, t);
  } else {
    *result = 0.0;
  }
}

void PPB_Flash_Proxy::OnHostMsgIsRectTopmost(PP_Instance instance,
                                             PP_Rect rect,
                                             PP_Bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->GetFlashAPI()->IsRectTopmost(instance, &rect);
  else
    *result = PP_FALSE;
}

void PPB_Flash_Proxy::OnHostMsgFlashSetFullscreen(PP_Instance instance,
                                                  PP_Bool fullscreen,
                                                  PP_Bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->FlashSetFullscreen(
        instance, fullscreen);
  } else {
    *result = PP_FALSE;
  }
}

void PPB_Flash_Proxy::OnHostMsgFlashGetScreenSize(PP_Instance instance,
                                                  PP_Bool* result,
                                                  PP_Size* size) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->FlashGetScreenSize(
        instance, size);
  } else {
    size->width = 0;
    size->height = 0;
  }
}

void PPB_Flash_Proxy::OnHostMsgIsClipboardFormatAvailable(
    PP_Instance instance,
    int clipboard_type,
    int format,
    bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = PP_ToBool(
        enter.functions()->GetFlashAPI()->IsClipboardFormatAvailable(
            instance,
            static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
            static_cast<PP_Flash_Clipboard_Format>(format)));
  } else {
    *result = false;
  }
}

void PPB_Flash_Proxy::OnHostMsgReadClipboardData(
    PP_Instance instance,
    int clipboard_type,
    int format,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetFlashAPI()->ReadClipboardData(
                      instance,
                      static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
                      static_cast<PP_Flash_Clipboard_Format>(format)));
  }
}

void PPB_Flash_Proxy::OnHostMsgWriteClipboardData(
    PP_Instance instance,
    int clipboard_type,
    const std::vector<int>& formats,
    SerializedVarVectorReceiveInput data_items) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    uint32_t data_item_count;
    PP_Var* data_items_array = data_items.Get(dispatcher(), &data_item_count);
    CHECK(data_item_count == formats.size());

    scoped_array<PP_Flash_Clipboard_Format> formats_array(
        new PP_Flash_Clipboard_Format[formats.size()]);
    for (uint32_t i = 0; i < formats.size(); ++i)
      formats_array[i] = static_cast<PP_Flash_Clipboard_Format>(formats[i]);

    int32_t result = enter.functions()->GetFlashAPI()->WriteClipboardData(
        instance,
        static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
        data_item_count,
        formats_array.get(),
        data_items_array);
    DLOG_IF(WARNING, result != PP_OK)
        << "Write to clipboard failed unexpectedly.";
    (void)result;  // Prevent warning in release mode.
  }
}

}  // namespace proxy
}  // namespace ppapi
