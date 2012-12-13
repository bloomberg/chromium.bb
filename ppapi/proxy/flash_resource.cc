// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"

using ppapi::thunk::EnterResourceNoLock;

namespace ppapi {
namespace proxy {

FlashResource::FlashResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_Flash_Create());
  SendCreate(BROWSER, PpapiHostMsg_Flash_Create());
}

FlashResource::~FlashResource() {
}

thunk::PPB_Flash_Functions_API* FlashResource::AsPPB_Flash_Functions_API() {
  return this;
}

PP_Var FlashResource::GetProxyForURL(PP_Instance instance,
                                     const std::string& url) {
  std::string proxy;
  int32_t result = SyncCall<PpapiPluginMsg_Flash_GetProxyForURLReply>(RENDERER,
      PpapiHostMsg_Flash_GetProxyForURL(url), &proxy);

  if (result == PP_OK)
    return StringVar::StringToPPVar(proxy);
  return PP_MakeUndefined();
}

void FlashResource::UpdateActivity(PP_Instance instance) {
  Post(BROWSER, PpapiHostMsg_Flash_UpdateActivity());
}

PP_Bool FlashResource::SetCrashData(PP_Instance instance,
                                    PP_FlashCrashKey key,
                                    PP_Var value) {
  switch (key) {
    case PP_FLASHCRASHKEY_URL: {
      StringVar* url_string_var(StringVar::FromPPVar(value));
      if (!url_string_var)
        return PP_FALSE;
      PluginGlobals::Get()->SetActiveURL(url_string_var->value());
      return PP_TRUE;
    }
  }
  return PP_FALSE;
}

void FlashResource::SetInstanceAlwaysOnTop(PP_Instance instance,
                                           PP_Bool on_top) {
  Post(RENDERER, PpapiHostMsg_Flash_SetInstanceAlwaysOnTop(PP_ToBool(on_top)));
}

PP_Bool FlashResource::DrawGlyphs(
    PP_Instance instance,
    PP_Resource pp_image_data,
    const PP_BrowserFont_Trusted_Description* font_desc,
    uint32_t color,
    const PP_Point* position,
    const PP_Rect* clip,
    const float transformation[3][3],
    PP_Bool allow_subpixel_aa,
    uint32_t glyph_count,
    const uint16_t glyph_indices[],
    const PP_Point glyph_advances[]) {
  EnterResourceNoLock<thunk::PPB_ImageData_API> enter(pp_image_data, true);
  if (enter.failed())
    return PP_FALSE;
  // The instance parameter isn't strictly necessary but we check that it
  // matches anyway.
  if (enter.resource()->pp_instance() != instance)
    return PP_FALSE;

  PPBFlash_DrawGlyphs_Params params;
  params.image_data = enter.resource()->host_resource();
  params.font_desc.SetFromPPBrowserFontDescription(*font_desc);
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

  // This has to be synchronous because the caller may want to composite on
  // top of the resulting text after the call is complete.
  int32_t result = SyncCall<IPC::Message>(RENDERER,
      PpapiHostMsg_Flash_DrawGlyphs(params));
  return PP_FromBool(result == PP_OK);
}

int32_t FlashResource::Navigate(PP_Instance instance,
                                PP_Resource request_info,
                                const char* target,
                                PP_Bool from_user_action) {
  thunk::EnterResourceNoLock<thunk::PPB_URLRequestInfo_API> enter(request_info,
                                                                  true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return SyncCall<IPC::Message>(RENDERER, PpapiHostMsg_Flash_Navigate(
      enter.object()->GetData(), target, PP_ToBool(from_user_action)));
}

PP_Bool FlashResource::IsRectTopmost(PP_Instance instance,
                                     const PP_Rect* rect) {
  int32_t result = SyncCall<IPC::Message>(RENDERER,
      PpapiHostMsg_Flash_IsRectTopmost(*rect));
  return PP_FromBool(result == PP_OK);
}

}  // namespace proxy
}  // namespace ppapi
