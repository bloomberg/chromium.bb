// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_font_proxy.h"

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

using ppapi::thunk::PPB_Font_FunctionAPI;

namespace ppapi {
namespace proxy {

PPB_Font_Proxy::PPB_Font_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Font_Proxy::~PPB_Font_Proxy() {
}

PPB_Font_FunctionAPI* PPB_Font_Proxy::AsPPB_Font_FunctionAPI() {
  return this;
}

// TODO(ananta)
// This needs to be wired up to the PPAPI plugin code.
PP_Var PPB_Font_Proxy::GetFontFamilies(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  // Assume the font families don't change, so we can cache the result globally.
  CR_DEFINE_STATIC_LOCAL(std::string, families, ());
  if (families.empty()) {
    PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(
        new PpapiHostMsg_PPBFont_GetFontFamilies(&families));
  }

  return StringVar::StringToPPVar(families);
}

bool PPB_Font_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // There aren't any font messages.
  NOTREACHED();
  return false;
}

}  // namespace proxy
}  // namespace ppapi
