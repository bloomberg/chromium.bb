// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include <string>

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/private/ppb_flash.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

namespace {

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, PP_Bool on_top) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return;
  instance->set_always_on_top(PPBoolToBool(on_top));
}

PP_Var GetProxyForURL(PP_Instance pp_instance, const char* url) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_MakeUndefined();

  GURL gurl(url);
  if (!gurl.is_valid())
    return PP_MakeUndefined();

  std::string proxy_host = instance->delegate()->ResolveProxy(gurl);
  if (proxy_host.empty())
    return PP_MakeUndefined();  // No proxy.
  return StringVar::StringToPPVar(instance->module(), proxy_host);
}

PP_Bool NavigateToURL(PP_Instance pp_instance,
                      const char* url,
                      const char* target) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_FALSE;
  return BoolToPPBool(instance->NavigateToURL(url, target));
}

void RunMessageLoop(PP_Instance instance) {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop(PP_Instance instance) {
  MessageLoop::current()->QuitNow();
}

const PPB_Flash ppb_flash = {
  &SetInstanceAlwaysOnTop,
  &PPB_Flash_Impl::DrawGlyphs,
  &GetProxyForURL,
  &NavigateToURL,
  &RunMessageLoop,
  &QuitMessageLoop,
};

}  // namespace

// static
const PPB_Flash* PPB_Flash_Impl::GetInterface() {
  return &ppb_flash;
}

}  // namespace ppapi
}  // namespace webkit
