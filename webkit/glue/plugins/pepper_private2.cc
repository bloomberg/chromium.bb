// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_private2.h"

#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/plugins/ppb_private2.h"

namespace pepper {

namespace {

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, bool on_top) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return;
  instance->set_always_on_top(on_top);
}

PP_Var GetProxyForURL(PP_Module pp_module, const char* url) {
  PluginModule* module = ResourceTracker::Get()->GetModule(pp_module);
  if (!module)
    return PP_MakeUndefined();

  PluginInstance* instance = module->GetSomeInstance();
  if (!instance)
    return PP_MakeUndefined();

  GURL gurl(url);
  if (!gurl.is_valid())
    return PP_MakeUndefined();

  std::string proxy_host = instance->delegate()->ResolveProxy(gurl);
  if (proxy_host.empty())
    return PP_MakeUndefined();  // No proxy.
  return StringVar::StringToPPVar(module, proxy_host);
}

const PPB_Private2 ppb_private2 = {
  &SetInstanceAlwaysOnTop,
  &Private2::DrawGlyphs,
  &GetProxyForURL
};

}  // namespace

// static
const PPB_Private2* Private2::GetInterface() {
  return &ppb_private2;
}

}  // namespace pepper
