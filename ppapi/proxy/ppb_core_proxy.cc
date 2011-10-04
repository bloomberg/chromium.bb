// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_core_proxy.h"

#include <stdlib.h>  // For malloc

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/time_conversion.h"

namespace ppapi {
namespace proxy {

namespace {

base::MessageLoopProxy* GetMainThreadMessageLoop() {
  static scoped_refptr<base::MessageLoopProxy> proxy(
      base::MessageLoopProxy::current());
  return proxy.get();
}

void AddRefResource(PP_Resource resource) {
  PluginResourceTracker::GetInstance()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  PluginResourceTracker::GetInstance()->ReleaseResource(resource);
}

double GetTime() {
  return TimeToPPTime(base::Time::Now());
}

double GetTimeTicks() {
  return TimeTicksToPPTimeTicks(base::TimeTicks::Now());
}

void CallbackWrapper(PP_CompletionCallback callback, int32_t result) {
  TRACE_EVENT2("ppapi proxy", "CallOnMainThread callback",
               "Func", reinterpret_cast<void*>(callback.func),
               "UserData", callback.user_data);
  PP_RunCompletionCallback(&callback, result);
}

void CallOnMainThread(int delay_in_ms,
                      PP_CompletionCallback callback,
                      int32_t result) {
  GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CallbackWrapper, callback, result),
      delay_in_ms);
}

PP_Bool IsMainThread() {
  return PP_FromBool(GetMainThreadMessageLoop()->BelongsToCurrentThread());
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &GetTime,
  &GetTimeTicks,
  &CallOnMainThread,
  &IsMainThread
};

}  // namespace

PPB_Core_Proxy::PPB_Core_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppb_core_impl_(NULL) {
  if (!dispatcher->IsPlugin()) {
    ppb_core_impl_ = static_cast<const PPB_Core*>(
        dispatcher->local_get_interface()(PPB_CORE_INTERFACE));
  }
}

PPB_Core_Proxy::~PPB_Core_Proxy() {
}

// static
const PPB_Core* PPB_Core_Proxy::GetPPB_Core_Interface() {
  return &core_interface;
}

bool PPB_Core_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Core_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_AddRefResource,
                        OnMsgAddRefResource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_ReleaseResource,
                        OnMsgReleaseResource)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Core_Proxy::OnMsgAddRefResource(const HostResource& resource) {
  ppb_core_impl_->AddRefResource(resource.host_resource());
}

void PPB_Core_Proxy::OnMsgReleaseResource(const HostResource& resource) {
  ppb_core_impl_->ReleaseResource(resource.host_resource());
}

}  // namespace proxy
}  // namespace ppapi
