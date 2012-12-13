// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

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

}  // namespace proxy
}  // namespace ppapi
