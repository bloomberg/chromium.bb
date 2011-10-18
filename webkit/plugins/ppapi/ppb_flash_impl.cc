// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include <string>

#include "base/message_loop.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ppapi::PPTimeToTime;
using ppapi::StringVar;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_URLRequestInfo_API;

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
  return StringVar::StringToPPVar(instance->module()->pp_module(), proxy_host);
}

int32_t Navigate(PP_Resource request_id,
                 const char* target,
                 bool from_user_action) {
  EnterResource<PPB_URLRequestInfo_API> enter(request_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_URLRequestInfo_Impl* request =
      static_cast<PPB_URLRequestInfo_Impl*>(enter.object());

  if (!target)
    return PP_ERROR_BADARGUMENT;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(request);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  return plugin_instance->Navigate(request, target, from_user_action);
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

double GetLocalTimeZoneOffset(PP_Instance pp_instance, PP_Time t) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0.0;

  // Evil hack. The time code handles exact "0" values as special, and produces
  // a "null" Time object. This will represent some date hundreds of years ago
  // and will give us funny results at 1970 (there are some tests where this
  // comes up, but it shouldn't happen in real life). To work around this
  // special handling, we just need to give it some nonzero value.
  if (t == 0.0)
    t = 0.0000000001;

  // We can't do the conversion here because on Linux, the localtime calls
  // require filesystem access prohibited by the sandbox.
  return instance->delegate()->GetLocalTimeZoneOffset(PPTimeToTime(t));
}

PP_Var GetCommandLineArgs(PP_Module pp_module) {
  PluginModule* module = ResourceTracker::Get()->GetModule(pp_module);
  if (!module)
    return PP_MakeUndefined();

  PluginInstance* instance = module->GetSomeInstance();
  if (!instance)
    return PP_MakeUndefined();

  std::string args = instance->delegate()->GetFlashCommandLineArgs();
  return StringVar::StringToPPVar(pp_module, args);
}

const PPB_Flash ppb_flash = {
  &SetInstanceAlwaysOnTop,
  &PPB_Flash_Impl::DrawGlyphs,
  &GetProxyForURL,
  &Navigate,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLocalTimeZoneOffset,
  &GetCommandLineArgs
};

}  // namespace

// static
const PPB_Flash* PPB_Flash_Impl::GetInterface() {
  return &ppb_flash;
}

}  // namespace ppapi
}  // namespace webkit
